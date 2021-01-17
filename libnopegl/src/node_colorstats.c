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
    int group_size;

    /* Init compute */
    struct pgcraft *crafter_init;
    struct pipeline_compat *pipeline_compat_init;
    int init_wg_count;
    int depth_index;
    int length_minus1_index;

    /* Waveform compute */
    struct pgcraft *crafter_waveform;
    struct pipeline_compat *pipeline_compat_waveform;
    int waveform_wg_count;

    /* Summary-scale compute */
    struct pgcraft *crafter_sumscale;
    struct pipeline_compat *pipeline_compat_sumscale;
    int sumscale_wg_count;
};

NGLI_STATIC_ASSERT(block_priv_first, offsetof(struct colorstats_priv, blk) == 0);

static int setup_compute(struct colorstats_priv *s, struct pgcraft *crafter,
                         struct pipeline_compat *pipeline_compat,
                         const struct pgcraft_params *crafter_params)
{
    int ret = ngli_pgcraft_craft(crafter, crafter_params);
    if (ret < 0)
        return ret;

    const struct pipeline_params pipeline_params = {
        .type    = NGLI_PIPELINE_TYPE_COMPUTE,
        .program = ngli_pgcraft_get_program(crafter),
        .layout  = ngli_pgcraft_get_pipeline_layout(crafter),
    };

    const struct pipeline_resources pipeline_resources = ngli_pgcraft_get_pipeline_resources(crafter);
    const struct pgcraft_compat_info *compat_info = ngli_pgcraft_get_compat_info(crafter);

    const struct pipeline_compat_params params = {
        .params      = &pipeline_params,
        .resources   = &pipeline_resources,
        .compat_info = compat_info,
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

    int ret = setup_compute(s, s->crafter_init, s->pipeline_compat_init, &crafter_params);
    if (ret < 0)
        return ret;

    s->depth_index         = ngli_pgcraft_get_uniform_index(s->crafter_init, "depth",         NGLI_PROGRAM_SHADER_COMP);
    s->length_minus1_index = ngli_pgcraft_get_uniform_index(s->crafter_init, "length_minus1", NGLI_PROGRAM_SHADER_COMP);
    return 0;
}

/* Phase 2: compute waveform in the data field (histograms per column) */
static int setup_waveform_compute(struct colorstats_priv *s, const struct pgcraft_block *block,
                                  const struct ngl_node *texture_node)
{
    struct texture_priv *texture_priv = texture_node->priv_data;
    const struct texture_opts *texture_opts = texture_node->opts;
    struct pgcraft_texture textures[] = {
        {
            .name        = "source",
            .stage       = NGLI_PROGRAM_SHADER_COMP,
            .image       = &texture_priv->image,
            .format      = texture_priv->params.format,
            .clamp_video = 0, /* clamping is done manually in the shader */
        },
    };

    if (texture_opts->data_src && texture_opts->data_src->cls->id == NGL_NODE_MEDIA)
        textures[0].type = NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO;
    else
        textures[0].type = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D;

    const struct pgcraft_params crafter_params = {
        .comp_base      = colorstats_waveform_comp,
        .textures       = textures,
        .nb_textures    = NGLI_ARRAY_NB(textures),
        .blocks         = block,
        .nb_blocks      = 1,
        .workgroup_size = {s->group_size, 1, 1},
    };

    return setup_compute(s, s->crafter_waveform, s->pipeline_compat_waveform, &crafter_params);
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

    return setup_compute(s, s->crafter_sumscale, s->pipeline_compat_sumscale, &crafter_params);
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
    LOG(DEBUG, "using a workgroup size of %d", s->group_size);

    s->pipeline_compat_init     = ngli_pipeline_compat_create(gpu_ctx);
    s->pipeline_compat_waveform = ngli_pipeline_compat_create(gpu_ctx);
    s->pipeline_compat_sumscale = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->pipeline_compat_init || !s->pipeline_compat_waveform || !s->pipeline_compat_sumscale)
        return NGL_ERROR_MEMORY;

    s->crafter_init     = ngli_pgcraft_create(ctx);
    s->crafter_waveform = ngli_pgcraft_create(ctx);
    s->crafter_sumscale = ngli_pgcraft_create(ctx);
    if (!s->crafter_init || !s->crafter_waveform || !s->crafter_sumscale)
        return NGL_ERROR_MEMORY;

    const struct pgcraft_block block = {
        .name     = "stats",
        .type     = NGLI_TYPE_STORAGE_BUFFER,
        .stage    = NGLI_PROGRAM_SHADER_COMP,
        .writable = 1,
        .block    = &s->blk.block,
        .buffer   = s->blk.buffer,
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
    ngli_block_init(block, NGLI_BLOCK_LAYOUT_STD430);

    /* Set block fields */
    static const struct {
        const char *name;
        int type;
        int count;
    } block_layout[] = {
        {"max_rgb",       NGLI_TYPE_UVEC2, 0},
        {"max_luma",      NGLI_TYPE_UVEC2, 0},
        {"depth",         NGLI_TYPE_I32,   0},
        {"length_minus1", NGLI_TYPE_I32,   0},
        {"summary",       NGLI_TYPE_UVEC4, 1 << MAX_BIT_DEPTH},
        {"data",          NGLI_TYPE_UVEC4, NGLI_BLOCK_VARIADIC_COUNT},
    };
    for (int i = 0; i < NGLI_ARRAY_NB(block_layout); i++) {
        int ret = ngli_block_add_field(block, block_layout[i].name, block_layout[i].type, block_layout[i].count);
        if (ret < 0)
            return ret;
    }

    /* We do not have any CPU data */
    s->blk.data = NULL;
    s->blk.data_size = 0;

    /* Colorstats needs to write into the block so we bind it as SSBO */
    s->blk.usage = NGLI_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    s->blk.buffer = ngli_buffer_create(gpu_ctx);
    if (!s->blk.buffer)
        return NGL_ERROR_MEMORY;

    /*
     * The size of the buffer depends on the texture size, which will only be
     * known in the update callback for textures fed by a Media node. We use a
     * variadic buffer size because don't want to wait for the texture update to
     * compile the shaders and prepare the pipelines.
     *
     * The init and prepare callbacks are still too early for this buffer
     * allocation, but we still need to set its usage for the pipeline bindings.
     */
    s->blk.buffer->usage |= NGLI_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    return 0;
}

static int colorstats_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct colorstats_priv *s = node->priv_data;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    if (!(gpu_ctx->features & NGLI_FEATURE_STORAGE_BUFFER)) {
        LOG(ERROR, "ColorStats is not supported by this context (requires compute shaders and SSBO support)");
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    int ret;
    if ((ret = init_block(s, gpu_ctx)) < 0 ||
        (ret = init_computes(node)) < 0)
        return ret;
    return 0;
}

static int alloc_block_buffer(struct ngl_node *node, int length)
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
    const int nb_workgroups = s->depth / s->group_size;
    ngli_assert(nb_workgroups <= 128); // should be 1, 2, 4 or 8, so always safe
    s->init_wg_count = nb_workgroups;
    s->sumscale_wg_count = nb_workgroups;
    ngli_assert(s->group_size <= s->depth);
    ngli_assert(s->depth % s->group_size == 0);

    /* Each workgroup of the waveform compute works on 1 column of pixels */
    s->waveform_wg_count = length;

    /*
     * Compute the size of the buffer depending on the resolution of the image
     * and allocate the variadic buffer accordingly.
     */
    const int data_field_count = length * s->depth;
    s->blk.data_size = ngli_block_get_size(&s->blk.block, data_field_count);
    return ngli_buffer_init(s->blk.buffer, s->blk.data_size, s->blk.usage);
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
    const int source_w = texture_priv->image.params.width;
    if (s->blk.buffer->size == 0)
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

    struct ngl_ctx *ctx = node->ctx;
    if (ctx->render_pass_started) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_end_render_pass(gpu_ctx);
        ctx->render_pass_started = 0;
        ctx->current_rendertarget = ctx->available_rendertargets[1];
    }

    /* Init */
    ngli_pipeline_compat_update_uniform(s->pipeline_compat_init, s->depth_index, &s->depth);
    ngli_pipeline_compat_update_uniform(s->pipeline_compat_init, s->length_minus1_index, &s->length_minus1);
    ngli_pipeline_compat_dispatch(s->pipeline_compat_init, s->init_wg_count, 1, 1);

    /* Waveform */
    const struct darray *texture_infos_array = ngli_pgcraft_get_texture_infos(s->crafter_waveform);
    const struct pgcraft_texture_info *info = ngli_darray_data(texture_infos_array);
    ngli_pipeline_compat_update_texture_info(s->pipeline_compat_waveform, info);
    ngli_pipeline_compat_dispatch(s->pipeline_compat_waveform, s->waveform_wg_count, 1, 1);

    /* Summary-scale */
    ngli_pipeline_compat_dispatch(s->pipeline_compat_sumscale, s->sumscale_wg_count, 1, 1);
}

static void colorstats_uninit(struct ngl_node *node)
{
    struct colorstats_priv *s = node->priv_data;

    ngli_pgcraft_freep(&s->crafter_waveform);
    ngli_pgcraft_freep(&s->crafter_sumscale);
    ngli_pipeline_compat_freep(&s->pipeline_compat_waveform);
    ngli_pipeline_compat_freep(&s->pipeline_compat_sumscale);
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
