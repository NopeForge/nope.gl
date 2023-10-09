/*
 * Copyright 2022-2023 Clément Bœsch <u pkh.me>
 * Copyright 2022 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "block.h"
#include "gpu_ctx.h"
#include "gpu_limits.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "type.h"

/* Compute shaders */
#include "colorstats_init_comp.h"
#include "colorstats_sumscale_comp.h"
#include "colorstats_waveform_comp.h"

/*
 * 8, 9 and 10 bit depth are supported. Larger value imply GPU memory limits
 * that may not be supported.
 *
 * This value needs to be kept in sync with MAX_DEPTH in the compute shaders.
 */
#define MAX_BIT_DEPTH 8

struct colorstats_opts {
    struct ngl_node *texture_node;
};

#define OFFSET(x) offsetof(struct colorstats_opts, x)
static const struct node_param colorstats_params[] = {
    {"texture", NGLI_PARAM_TYPE_NODE, OFFSET(texture_node),
                .flags=NGLI_PARAM_FLAG_NON_NULL,
                .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                .desc=NGLI_DOCSTRING("source texture to compute the color stats from")},
    {NULL}
};

struct colorstats_priv {
    struct block_info blk;
    int depth;
    int length_minus1;
    uint32_t group_size;

    /* Init compute */
    struct {
        struct pgcraft *crafter;
        struct pipeline_compat *pipeline_compat;
        int32_t wg_count;
        int32_t depth_index;
        int32_t length_minus1_index;
        int32_t block_index;
    } init;

    /* Waveform compute */
    struct {
        struct pgcraft *crafter;
        struct pipeline_compat *pipeline_compat;
        uint32_t wg_count;
        int32_t block_index;
        const struct pgcraft_texture_info *texture_info;
        size_t texture_image_rev;
    } waveform;

    /* Summary-scale compute */
    struct {
        struct pgcraft *crafter;
        struct pipeline_compat *pipeline_compat;
        uint32_t wg_count;
        int32_t block_index;
    } sumscale;
};

NGLI_STATIC_ASSERT(block_priv_first, offsetof(struct colorstats_priv, blk) == 0);

static int setup_compute(struct colorstats_priv *s, struct pgcraft *crafter,
                         struct pipeline_compat *pipeline_compat,
                         const struct pgcraft_params *crafter_params)
{
    int ret = ngli_pgcraft_craft(crafter, crafter_params);
    if (ret < 0)
        return ret;

    const struct pipeline_compat_params params = {
        .type        = NGLI_PIPELINE_TYPE_COMPUTE,
        .program     = ngli_pgcraft_get_program(crafter),
        .layout      = ngli_pgcraft_get_pipeline_layout(crafter),
        .resources   = ngli_pgcraft_get_pipeline_resources(crafter),
        .compat_info = ngli_pgcraft_get_compat_info(crafter),
    };

    return ngli_pipeline_compat_init(pipeline_compat, &params);
}

/* Phase 1: initialization (set globally shared values) */
static int setup_init_compute(struct colorstats_priv *s, const struct pgcraft_block *block)
{
    const struct pgcraft_uniform uniforms[] = {
        {.name="depth",         .type=NGLI_TYPE_I32, .stage=NGLI_PROGRAM_SHADER_COMP},
        {.name="length_minus1", .type=NGLI_TYPE_I32, .stage=NGLI_PROGRAM_SHADER_COMP},
    };

    const struct pgcraft_params crafter_params = {
        .comp_base      = colorstats_init_comp,
        .uniforms       = uniforms,
        .nb_uniforms    = NGLI_ARRAY_NB(uniforms),
        .blocks         = block,
        .nb_blocks      = 1,
        .workgroup_size = {s->group_size, 1, 1},
    };

    int ret = setup_compute(s, s->init.crafter, s->init.pipeline_compat, &crafter_params);
    if (ret < 0)
        return ret;

    s->init.depth_index         = ngli_pgcraft_get_uniform_index(s->init.crafter, "depth",         NGLI_PROGRAM_SHADER_COMP);
    s->init.length_minus1_index = ngli_pgcraft_get_uniform_index(s->init.crafter, "length_minus1", NGLI_PROGRAM_SHADER_COMP);

    s->init.block_index    = ngli_pgcraft_get_block_index(s->init.crafter, block->name, block->stage);

    return 0;
}

/* Phase 2: compute waveform in the data field (histograms per column) */
static int setup_waveform_compute(struct colorstats_priv *s, const struct pgcraft_block *block,
                                  const struct ngl_node *texture_node)
{
    struct texture_priv *texture_priv = texture_node->priv_data;
    struct pgcraft_texture textures[] = {
        {
            .name        = "source",
            .type        = NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO,
            .stage       = NGLI_PROGRAM_SHADER_COMP,
            .image       = &texture_priv->image,
            .format      = texture_priv->params.format,
            .clamp_video = 0, /* clamping is done manually in the shader */
        },
    };

    const struct pgcraft_params crafter_params = {
        .comp_base      = colorstats_waveform_comp,
        .textures       = textures,
        .nb_textures    = NGLI_ARRAY_NB(textures),
        .blocks         = block,
        .nb_blocks      = 1,
        .workgroup_size = {s->group_size, 1, 1},
    };

    int ret = setup_compute(s, s->waveform.crafter, s->waveform.pipeline_compat, &crafter_params);
    if (ret < 0)
        return ret;

    const struct darray *texture_infos_array = ngli_pgcraft_get_texture_infos(s->waveform.crafter);
    s->waveform.texture_info = ngli_darray_get(texture_infos_array, 0);
    s->waveform.texture_image_rev = SIZE_MAX;

    s->waveform.block_index = ngli_pgcraft_get_block_index(s->waveform.crafter, block->name, block->stage);

    return 0;
}

/* Phase 3: summary and scale for global histograms */
static int setup_sumscale_compute(struct colorstats_priv *s, const struct pgcraft_block *block)
{
    const struct pgcraft_params crafter_params = {
        .comp_base      = colorstats_sumscale_comp,
        .blocks         = block,
        .nb_blocks      = 1,
        .workgroup_size = {s->group_size, 1, 1},
    };

    int ret = setup_compute(s, s->sumscale.crafter, s->sumscale.pipeline_compat, &crafter_params);
    if (ret < 0)
        return ret;

    s->sumscale.block_index = ngli_pgcraft_get_block_index(s->sumscale.crafter, block->name, block->stage);

    return 0;
}

static int init_computes(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct colorstats_priv *s = node->priv_data;
    const struct colorstats_opts *o = node->opts;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    /*
     * Define workgroup size using limits. We pick a value multiple of the
     * depth on purpose.
     *
     * The OpenGLES 3.1 and Vulkan core specifications mandate that
     * gpu_limits.max_compute_work_group_size must be least [128,128,64], but
     * gpu_limits.max_compute_work_group_invocations (x*y*z) minimum is only
     * 128. Meaning that if we pick a work group size X=128, we will have to
     * use Y=1 and Z=1. 128 remains an always safe value so we use that as
     * a fallback.
     */
    const struct gpu_limits *limits = &gpu_ctx->limits;
    const int max_group_size_x = limits->max_compute_work_group_size[0];
    s->group_size = max_group_size_x >= 256 ? 256 : 128;
    LOG(DEBUG, "using a workgroup size of %u", s->group_size);

    s->init.pipeline_compat     = ngli_pipeline_compat_create(gpu_ctx);
    s->waveform.pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    s->sumscale.pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->init.pipeline_compat || !s->waveform.pipeline_compat || !s->sumscale.pipeline_compat)
        return NGL_ERROR_MEMORY;

    s->init.crafter     = ngli_pgcraft_create(ctx);
    s->waveform.crafter = ngli_pgcraft_create(ctx);
    s->sumscale.crafter = ngli_pgcraft_create(ctx);
    if (!s->init.crafter || !s->waveform.crafter || !s->sumscale.crafter)
        return NGL_ERROR_MEMORY;

    const struct pgcraft_block block = {
        .name     = "stats",
        .type     = NGLI_TYPE_STORAGE_BUFFER,
        .stage    = NGLI_PROGRAM_SHADER_COMP,
        .writable = 1,
        .block    = &s->blk.block,
    };

    int ret;
    if ((ret = setup_init_compute(s, &block)) < 0 ||
        (ret = setup_waveform_compute(s, &block, o->texture_node)) < 0 ||
        (ret = setup_sumscale_compute(s, &block)) < 0)
        return ret;

    return 0;
}

static int init_block(struct colorstats_priv *s, struct gpu_ctx *gpu_ctx)
{
    struct block *block = &s->blk.block;
    ngli_block_init(gpu_ctx, block, NGLI_BLOCK_LAYOUT_STD430);

    static const struct block_field block_fields[] = {
        {"max_rgb",       NGLI_TYPE_UVEC2, 0},
        {"max_luma",      NGLI_TYPE_UVEC2, 0},
        {"depth",         NGLI_TYPE_I32,   0},
        {"length_minus1", NGLI_TYPE_I32,   0},
        {"summary",       NGLI_TYPE_UVEC4, 1 << MAX_BIT_DEPTH},
        {"data",          NGLI_TYPE_UVEC4, NGLI_BLOCK_VARIADIC_COUNT},
    };
    int ret = ngli_block_add_fields(block, block_fields, NGLI_ARRAY_NB(block_fields));
    if (ret < 0)
        return ret;

    /* We do not have any CPU data */
    s->blk.data = NULL;
    s->blk.data_size = 0;

    /* Colorstats needs to write into the block so we bind it as SSBO */
    s->blk.usage = NGLI_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    return 0;
}

static int colorstats_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct colorstats_priv *s = node->priv_data;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    if (!(gpu_ctx->features & NGLI_FEATURE_COMPUTE)) {
        LOG(ERROR, "ColorStats is not supported by this context (requires compute shaders and SSBO support)");
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    int ret;
    if ((ret = init_block(s, gpu_ctx)) < 0 ||
        (ret = init_computes(node)) < 0)
        return ret;
    return 0;
}

static int alloc_block_buffer(struct ngl_node *node, int32_t length)
{
    struct colorstats_priv *s = node->priv_data;

    /* We assume a 8-bit sampling all the time for now */
    s->depth = 1 << 8;

    /*
     * Horizontal length, minus 1 to reduce operations in the shader
     * TODO: add a vertical mode using the image height instead
     */
    s->length_minus1 = length - 1;

    /*
     * Given the following possible configurations:
     * - depth: 1<<8 (256), 1<<9 (512) or 1<<10 (1024)
     * - group_size: 128 or 256 (number of threads per workgroup)
     * We know we can split the workload of processing the summary data (of
     * length "depth") into an exact small number of workgroups (without any
     * remainder of data).
     */
    const uint32_t nb_workgroups = s->depth / s->group_size;
    ngli_assert(nb_workgroups <= 128); // should be 1, 2, 4 or 8, so always safe
    s->init.wg_count = nb_workgroups;
    s->sumscale.wg_count = nb_workgroups;
    ngli_assert(s->group_size <= s->depth);
    ngli_assert(s->depth % s->group_size == 0);

    /* Each workgroup of the waveform compute works on 1 column of pixels */
    s->waveform.wg_count = length;

    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    s->blk.buffer = ngli_buffer_create(gpu_ctx);
    if (!s->blk.buffer)
        return NGL_ERROR_MEMORY;

    /*
     * Compute the size of the buffer depending on the resolution of the image
     * and allocate the variadic buffer accordingly.
     */
    const size_t data_field_count = length * s->depth;
    s->blk.data_size = ngli_block_get_size(&s->blk.block, data_field_count);
    int ret = ngli_buffer_init(s->blk.buffer, s->blk.data_size, s->blk.usage);
    if (ret < 0)
        return ret;

    if ((ret = ngli_pipeline_compat_update_buffer(s->init.pipeline_compat, s->init.block_index, s->blk.buffer, 0, 0)) < 0 ||
        (ret = ngli_pipeline_compat_update_buffer(s->sumscale.pipeline_compat, s->sumscale.block_index, s->blk.buffer, 0, 0)) < 0 ||
        (ret = ngli_pipeline_compat_update_buffer(s->waveform.pipeline_compat, s->waveform.block_index, s->blk.buffer, 0, 0)) < 0)
        return ret;

    /* Signal buffer change */
    s->blk.buffer_rev++;

    return 0;
}

static int colorstats_update(struct ngl_node *node, double t)
{
    struct colorstats_priv *s = node->priv_data;
    const struct colorstats_opts *o = node->opts;

    int ret = ngli_node_update(o->texture_node, t);
    if (ret < 0)
        return ret;

    /*
     * Lazily allocate the data buffer because it depends on the texture
     * dimensions
     */
    const struct texture_priv *texture_priv = o->texture_node->priv_data;
    const int32_t source_w = texture_priv->image.params.width;
    if (!s->blk.buffer)
        return alloc_block_buffer(node, source_w);

    /* Stream size change event */
    if (s->length_minus1 != source_w - 1) {
        // TODO: we need to resize the block data field / reallocate the underlying buffer
        LOG(ERROR, "stream size change (%d -> %d) is not supported", s->length_minus1 + 1, source_w);
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

static void colorstats_draw(struct ngl_node *node)
{
    struct colorstats_priv *s = node->priv_data;
    const struct colorstats_opts *o = node->opts;

    struct ngl_ctx *ctx = node->ctx;
    if (ctx->render_pass_started) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_end_render_pass(gpu_ctx);
        ctx->render_pass_started = 0;
        ctx->current_rendertarget = ctx->available_rendertargets[1];
    }

    ngli_node_draw(o->texture_node);

    /* Init */
    ngli_pipeline_compat_update_uniform(s->init.pipeline_compat, s->init.depth_index, &s->depth);
    ngli_pipeline_compat_update_uniform(s->init.pipeline_compat, s->init.length_minus1_index, &s->length_minus1);
    ngli_pipeline_compat_dispatch(s->init.pipeline_compat, s->init.wg_count, 1, 1);

    /* Waveform */
    const struct image *image = s->waveform.texture_info->image;
    ngli_assert(image);
    if (s->waveform.texture_image_rev != image->rev) {
        ngli_pipeline_compat_update_texture_info(s->waveform.pipeline_compat, s->waveform.texture_info);
        s->waveform.texture_image_rev = image->rev;
    }

    ngli_pipeline_compat_dispatch(s->waveform.pipeline_compat, s->waveform.wg_count, 1, 1);

    /* Summary-scale */
    ngli_pipeline_compat_dispatch(s->sumscale.pipeline_compat, s->sumscale.wg_count, 1, 1);
}

static void colorstats_uninit(struct ngl_node *node)
{
    struct colorstats_priv *s = node->priv_data;

    ngli_pgcraft_freep(&s->init.crafter);
    ngli_pgcraft_freep(&s->waveform.crafter);
    ngli_pgcraft_freep(&s->sumscale.crafter);
    ngli_pipeline_compat_freep(&s->init.pipeline_compat);
    ngli_pipeline_compat_freep(&s->waveform.pipeline_compat);
    ngli_pipeline_compat_freep(&s->sumscale.pipeline_compat);
    ngli_buffer_freep(&s->blk.buffer);
    ngli_block_reset(&s->blk.block);
}

const struct node_class ngli_colorstats_class = {
    .id         = NGL_NODE_COLORSTATS,
    .category   = NGLI_NODE_CATEGORY_BLOCK,
    .name       = "ColorStats",
    .init       = colorstats_init,
    .update     = colorstats_update,
    .draw       = colorstats_draw,
    .uninit     = colorstats_uninit,
    .opts_size  = sizeof(struct colorstats_opts),
    .priv_size  = sizeof(struct colorstats_priv),
    .params     = colorstats_params,
    .file       = __FILE__,
};
