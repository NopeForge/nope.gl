/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "log.h"
#include "ngpu/block.h"
#include "ngpu/ctx.h"
#include "ngpu/graphics_state.h"
#include "ngpu/rendertarget.h"
#include "node_texture.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "rtt.h"
#include "ngpu/pgcraft.h"
#include "utils/bits.h"
#include "utils/utils.h"

/* GLSL shaders */
#include "blur_common_vert.h"
#include "blur_downsample_frag.h"
#include "blur_upsample_frag.h"
#include "blur_interpolate_frag.h"

#define _CONSTANT_TO_STR(v) #v
#define CONSTANT_TO_STR(v) _CONSTANT_TO_STR(v)

#define MAX_MIP_LEVELS 16

#define DWS_NAME "nopegl/fast-gaussian-blur-dws"
#define UPS_NAME "nopegl/fast-gaussian-blur-ups"

struct down_up_data_block {
    float offset;
};

struct interpolate_block {
    float lod;
};

struct fgblur_opts {
    struct ngl_node *source;
    struct ngl_node *destination;
    struct ngl_node *bluriness_node;
    float bluriness;
};

struct fgblur_priv {
    int32_t width;
    int32_t height;
    int32_t max_lod;
    float bluriness;

    /* Intermediates Mips used by the blur passses */
    struct ngpu_rendertarget_layout mip_layout;

    struct rtt_ctx *mip;
    struct rtt_ctx *mips[MAX_MIP_LEVELS];

    struct ngpu_block down_up_data_block;

    /* Downsampling pass */
    struct {
        struct ngpu_pgcraft *crafter;
        struct pipeline_compat *pl;
    } dws;

    /* Upsampling pass */
    struct {
        struct ngpu_pgcraft *crafter;
        struct pipeline_compat *pl;
    } ups;

    /* Render target (destination) used by the interpolate pass */
    int dst_is_resizable;
    struct ngpu_rendertarget_layout dst_layout;
    struct rtt_ctx *dst_rtt_ctx;

    /* Interpolate pass */
    struct {
        struct ngpu_block block;
        struct ngpu_pgcraft *crafter;
        struct pipeline_compat *pl;
    } interpolate;
};

#define OFFSET(x) offsetof(struct fgblur_opts, x)
static const struct node_param fgblur_params[] = {
    {"source",            NGLI_PARAM_TYPE_NODE, OFFSET(source),
                          .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                          .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                          .desc=NGLI_DOCSTRING("source to use for the blur")},
    {"destination",       NGLI_PARAM_TYPE_NODE, OFFSET(destination),
                          .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                          .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                          .desc=NGLI_DOCSTRING("destination to use for the blur")},
    {"bluriness",         NGLI_PARAM_TYPE_F32, OFFSET(bluriness_node), {.f32=0.03f},
                          .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                          .desc=NGLI_DOCSTRING("amount of bluriness in the range [0, 1]")},
    {NULL}
};

static int setup_down_up_pipeline(struct ngpu_pgcraft *crafter,
                                  const char *name,
                                  const char *frag_base,
                                  struct pipeline_compat *pipeline,
                                  const struct ngpu_rendertarget_layout *layout,
                                  struct ngpu_block *block)
{
    const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "tex_coord", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_texture textures[] = {
        {
            .name      = "tex",
            .type      = NGPU_PGCRAFT_SHADER_TEX_TYPE_2D,
            .precision = NGPU_PRECISION_HIGH,
            .stage     = NGPU_PROGRAM_SHADER_FRAG,
        },
    };

    const struct ngpu_pgcraft_block blocks[] = {
        {
            .name          = "data",
            .instance_name = "",
            .type          = NGPU_TYPE_UNIFORM_BUFFER,
            .stage         = NGPU_PROGRAM_SHADER_FRAG,
            .block         = &block->block_desc,
            .buffer        = {
                .buffer    = block->buffer,
                .size      = block->block_size,
            },
        }
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = name,
        .vert_base        = blur_common_vert,
        .frag_base        = frag_base,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .blocks           = blocks,
        .nb_blocks        = NGLI_ARRAY_NB(blocks),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    int ret = ngpu_pgcraft_craft(crafter, &crafter_params);
    if (ret < 0)
        return ret;

    const struct pipeline_compat_params params = {
        .type         = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGPU_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = *layout,
            .vertex_state = ngpu_pgcraft_get_vertex_state(crafter),
        },
        .program          = ngpu_pgcraft_get_program(crafter),
        .layout_desc      = ngpu_pgcraft_get_bindgroup_layout_desc(crafter),
        .resources        = ngpu_pgcraft_get_bindgroup_resources(crafter),
        .vertex_resources = ngpu_pgcraft_get_vertex_resources(crafter),
        .compat_info      = ngpu_pgcraft_get_compat_info(crafter),
    };

    ret = ngli_pipeline_compat_init(pipeline, &params);
    if (ret < 0)
        return ret;

    return 0;
}

static int setup_interpolate_pipeline(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct fgblur_priv *s = node->priv_data;

    const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "tex_coord", .type = NGPU_TYPE_VEC2},
    };

    struct ngpu_pgcraft_texture interpolate_textures[] = {
        {
            .name      = "tex0",
            .type      = NGPU_PGCRAFT_SHADER_TEX_TYPE_2D,
            .precision = NGPU_PRECISION_HIGH,
            .stage     = NGPU_PROGRAM_SHADER_FRAG
        }, {
            .name      = "tex1",
            .type      = NGPU_PGCRAFT_SHADER_TEX_TYPE_2D,
            .precision = NGPU_PRECISION_HIGH,
            .stage     = NGPU_PROGRAM_SHADER_FRAG
        },
    };

    const struct ngpu_block_entry interpolate_block_fields[] = {
        NGPU_BLOCK_FIELD(struct interpolate_block, lod, NGPU_TYPE_F32, 0),
    };
    const struct ngpu_block_params interpolate_block_params = {
        .entries    = interpolate_block_fields,
        .nb_entries = NGLI_ARRAY_NB(interpolate_block_fields),
    };
    int ret = ngpu_block_init(gpu_ctx, &s->interpolate.block, &interpolate_block_params);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_block crafter_blocks[] = {
        {
            .name          = "interpolate",
            .type          = NGPU_TYPE_UNIFORM_BUFFER,
            .stage         = NGPU_PROGRAM_SHADER_FRAG,
            .block         = &s->interpolate.block.block_desc,
            .buffer        = {
                .buffer    = s->interpolate.block.buffer,
                .size      = s->interpolate.block.block_size,
            },
        }
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/fast-gaussian-blur-interpolate",
        .vert_base        = blur_common_vert,
        .frag_base        = blur_interpolate_frag,
        .textures         = interpolate_textures,
        .nb_textures      = NGLI_ARRAY_NB(interpolate_textures),
        .blocks           = crafter_blocks,
        .nb_blocks        = NGLI_ARRAY_NB(crafter_blocks),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    ret = ngpu_pgcraft_craft(s->interpolate.crafter, &crafter_params);
    if (ret < 0)
        return ret;

    const struct pipeline_compat_params params = {
        .type         = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGPU_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = s->dst_layout,
            .vertex_state = ngpu_pgcraft_get_vertex_state(s->interpolate.crafter),
        },
        .program          = ngpu_pgcraft_get_program(s->interpolate.crafter),
        .layout_desc      = ngpu_pgcraft_get_bindgroup_layout_desc(s->interpolate.crafter),
        .resources        = ngpu_pgcraft_get_bindgroup_resources(s->interpolate.crafter),
        .vertex_resources = ngpu_pgcraft_get_vertex_resources(s->interpolate.crafter),
        .compat_info      = ngpu_pgcraft_get_compat_info(s->interpolate.crafter),
    };

    ret = ngli_pipeline_compat_init(s->interpolate.pl, &params);
    if (ret < 0)
        return ret;

    return 0;
}

static int fgblur_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct fgblur_priv *s = node->priv_data;
    struct fgblur_opts *o = node->opts;

    /* Disable direct rendering */
    struct texture_info *src_info = o->source->priv_data;
    src_info->supported_image_layouts = NGLI_IMAGE_LAYOUT_DEFAULT_BIT;

    /* Override texture params */
    src_info->params.min_filter = NGPU_FILTER_LINEAR;
    src_info->params.mag_filter = NGPU_FILTER_LINEAR;
    src_info->params.wrap_s     = NGPU_WRAP_MIRRORED_REPEAT,
    src_info->params.wrap_t     = NGPU_WRAP_MIRRORED_REPEAT,

    s->mip_layout.colors[s->mip_layout.nb_colors].format = src_info->params.format;
    s->mip_layout.nb_colors++;

    struct texture_info *dst_info = o->destination->priv_data;
    dst_info->params.usage |= NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    s->dst_is_resizable = (dst_info->params.width == 0 && dst_info->params.height == 0);
    s->dst_layout.colors[0].format = dst_info->params.format;
    s->dst_layout.nb_colors = 1;

    const struct ngpu_block_entry down_up_data_block_fields[] = {
        NGPU_BLOCK_FIELD(struct down_up_data_block, offset, NGPU_TYPE_F32, 0),
    };
    const struct ngpu_block_params down_up_data_block_params = {
        .entries    = down_up_data_block_fields,
        .nb_entries = NGLI_ARRAY_NB(down_up_data_block_fields),
    };
    int ret = ngpu_block_init(gpu_ctx, &s->down_up_data_block, &down_up_data_block_params);
    if (ret < 0)
        return ret;

    ret = ngpu_block_update(&s->down_up_data_block, 0, &(struct down_up_data_block){.offset=1.f});
    if (ret < 0)
        return ret;

    s->dws.crafter = ngpu_pgcraft_create(gpu_ctx);
    s->ups.crafter = ngpu_pgcraft_create(gpu_ctx);
    s->interpolate.crafter = ngpu_pgcraft_create(gpu_ctx);
    if (!s->dws.crafter || !s->ups.crafter || !s->interpolate.crafter)
        return NGL_ERROR_MEMORY;

    s->dws.pl = ngli_pipeline_compat_create(gpu_ctx);
    s->ups.pl = ngli_pipeline_compat_create(gpu_ctx);
    s->interpolate.pl = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->dws.pl || !s->ups.pl || !s->interpolate.pl)
        return NGL_ERROR_MEMORY;

    if ((ret = setup_down_up_pipeline(s->dws.crafter, DWS_NAME, blur_downsample_frag, s->dws.pl, &s->mip_layout, &s->down_up_data_block)) < 0 ||
        (ret = setup_down_up_pipeline(s->ups.crafter, UPS_NAME, blur_upsample_frag, s->ups.pl, &s->mip_layout, &s->down_up_data_block)) < 0)
        return ret;

    ret = setup_interpolate_pipeline(node);
    if (ret < 0)
        return ret;

    return 0;
}

static int resize(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct fgblur_priv *s = node->priv_data;
    struct fgblur_opts *o = node->opts;

    ngli_node_draw(o->source);

    struct texture_info *src_info = o->source->priv_data;
    const int32_t width = src_info->image.params.width;
    const int32_t height = src_info->image.params.height;
    if (s->width == width && s->height == height)
        return 0;

    /* Assert that the source texture format does not change */
    ngli_assert(src_info->params.format == s->mip_layout.colors[0].format);

    /* Assert that the destination texture format does not change */
    struct texture_info *dst_info = o->destination->priv_data;
    ngli_assert(dst_info->params.format == s->dst_layout.colors[0].format);

    struct ngpu_texture_params texture_params = (struct ngpu_texture_params) {
        .type          = NGPU_TEXTURE_TYPE_2D,
        .format        = src_info->params.format,
        .width         = width,
        .height        = height,
        .min_filter    = NGPU_FILTER_LINEAR,
        .mag_filter    = NGPU_FILTER_LINEAR,
        .wrap_s        = NGPU_WRAP_MIRRORED_REPEAT,
        .wrap_t        = NGPU_WRAP_MIRRORED_REPEAT,
        .usage         = NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                         NGPU_TEXTURE_USAGE_SAMPLED_BIT,
    };

    struct rtt_ctx *mip = NULL;
    struct rtt_ctx *mips[MAX_MIP_LEVELS] = {0};

    struct ngpu_texture *dst = NULL;
    struct rtt_ctx *dst_rtt_ctx = NULL;

    mip = ngli_rtt_create(ctx);
    if (!mip)
        return NGL_ERROR_MEMORY;

    ret = ngli_rtt_from_texture_params(mip, &texture_params);
    if (ret < 0)
        goto fail;

    int32_t mip_width = width;
    int32_t mip_height = height;
    for (size_t i = 0; i < MAX_MIP_LEVELS; i++) {
        mips[i] = ngli_rtt_create(ctx);
        if (!mips[i]) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        texture_params.width = mip_width;
        texture_params.height = mip_height;
        ret = ngli_rtt_from_texture_params(mips[i], &texture_params);
        if (ret < 0)
            goto fail;

        mip_width = NGLI_MAX(mip_width >> 1, 1);
        mip_height = NGLI_MAX(mip_height >> 1, 1);
    }

    dst = dst_info->texture;
    if (s->dst_is_resizable) {
        dst = ngpu_texture_create(ctx->gpu_ctx);
        if (!dst) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        struct ngpu_texture_params params = dst_info->params;
        params.width = width;
        params.height = height;
        ret = ngpu_texture_init(dst, &params);
        if (ret < 0)
            goto fail;
    }

    dst_rtt_ctx = ngli_rtt_create(ctx);
    if (!dst_rtt_ctx) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    const struct rtt_params rtt_params = (struct rtt_params) {
        .width  = dst->params.width,
        .height = dst->params.height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = dst,
            .load_op = NGPU_LOAD_OP_CLEAR,
            .store_op = NGPU_STORE_OP_STORE,
        },
    };

    ret = ngli_rtt_init(dst_rtt_ctx, &rtt_params);
    if (ret < 0)
        goto fail;

    ngli_rtt_freep(&s->mip);
    s->mip = mip;

    for (size_t i = 0; i < MAX_MIP_LEVELS; i++) {
        ngli_rtt_freep(&s->mips[i]);
        s->mips[i] = mips[i];
    }

    if (s->dst_is_resizable) {
        ngpu_texture_freep(&dst_info->texture);
        dst_info->texture = dst;
        dst_info->image.params.width = dst->params.width;
        dst_info->image.params.height = dst->params.height;
        dst_info->image.planes[0] = dst;
        dst_info->image.rev = dst_info->image_rev++;
    }

    ngli_rtt_freep(&s->dst_rtt_ctx);
    s->dst_rtt_ctx = dst_rtt_ctx;

    s->width = width;
    s->height = height;

    int32_t nb_mips = ngli_log2(NGLI_MAX(s->width, s->height)) + 1;
    int32_t max_lod = nb_mips - 1;

    /*
     * For a given lod, we need to generate mips[lod] and mips[lod+1], hence,
     * we need to make sure we do not exceed MAX_MIP_LEVELS - 2.
     */
    s->max_lod = NGLI_MIN(max_lod, MAX_MIP_LEVELS - 2);

    return 0;

fail:
    ngli_rtt_freep(&mip);
    for (size_t i = 0; i < MAX_MIP_LEVELS; i++)
        ngli_rtt_freep(&mips[i]);

    ngli_rtt_freep(&dst_rtt_ctx);
    if (s->dst_is_resizable)
        ngpu_texture_freep(&dst);

    LOG(ERROR, "failed to resize blur: %dx%d", width, height);
    return ret;
}

static void execute_down_up_pass(struct ngl_ctx *ctx,
                                 struct rtt_ctx *rtt_ctx,
                                 struct pipeline_compat *pipeline,
                                 const struct image *image)
{
    ngli_rtt_begin(rtt_ctx);
    ngpu_ctx_begin_render_pass(ctx->gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    ngli_pipeline_compat_update_image(pipeline, 0, image);
    ngli_pipeline_compat_draw(pipeline, 3, 1, 0);
    ngli_rtt_end(rtt_ctx);
}

/*
 * Compute the lod level from the radius.
 *
 * The formula used below is the result of a logarithmic fit to a serie of
 * points (x, y) where x represents the blur radius and y the associated lod
 * level of each generated mip.
 *
 * To generate the serie of points, we measured for each lod level the blur
 * radius by comparing visually the corresponding mip and the output of a
 * gaussian blur performed by GIMP at different radii. For reference here are
 * the list of points:
 *   (4.45, 1), (12.92, 2), (22.97, 3), (50, 4), (100, 5)
 * which can be approximated by:
 *   1.34508f * logf(0.406057f * radius) for x > 5.17925
 *   radius / 5.17925 for x <= 5.17925
 *
 * While far from perfect, this approximation is considered good enough for now
 * as it provides close enough results to a regular gaussian blur.
 */
static float compute_lod(float radius)
{
    const float k = 5.17925f;
    if (radius <= k)
        return radius / k;
    return 1.34508f * logf(0.406057f * radius);
}

static void fgblur_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct fgblur_priv *s = node->priv_data;
    struct fgblur_opts *o = node->opts;

    int ret = resize(node);
    if (ret < 0)
        return;

    const float bluriness_raw = *(float *)ngli_node_get_data_ptr(o->bluriness_node, &o->bluriness);
    const float bluriness = NGLI_CLAMP(bluriness_raw, 0.f, 1.f);
    const float diagonal = hypotf((float)s->width, (float)s->height);
    const float radius = bluriness * (float)diagonal / 2.f;
    const float lod = NGLI_MIN(compute_lod(radius), (float)s->max_lod);
    const int32_t lod_i = (int32_t)lod;
    const float lod_f = lod - (float)lod_i;

    /* Downsample source to mips[1] */
    struct texture_info *src_info = o->source->priv_data;
    const struct image *src_image = &src_info->image;
    const struct image *mip = src_image;
    execute_down_up_pass(ctx, s->mips[1], s->dws.pl, mip);

    /* Downsample successively until mips[lod_i+1] is generated */
    for (int32_t i = 2; i <= lod_i + 1; i++)
        execute_down_up_pass(ctx, s->mips[i], s->dws.pl, ngli_rtt_get_image(s->mips[i - 1], 0));

    /*
     * Upsample successively from mips[lod_i] back to full resolution and store
     * the result in mip. If lod == 0, we simply use the source.
     */
    if (lod_i > 0) {
        for (int32_t i = lod_i - 1; i > 0; i--)
            execute_down_up_pass(ctx, s->mips[i], s->ups.pl, ngli_rtt_get_image(s->mips[i + 1], 0));
        execute_down_up_pass(ctx, s->mip, s->ups.pl, ngli_rtt_get_image(s->mips[1], 0));
        mip = ngli_rtt_get_image(s->mip, 0);
    }

    /*
     * Upsample successively from mips[lod_i+1] back to full resolution and
     * store the result in mips[0]
     */
    for (int32_t i = lod_i; i >= 0; i--)
        execute_down_up_pass(ctx, s->mips[i], s->ups.pl, ngli_rtt_get_image(s->mips[i + 1], 0));

    const struct interpolate_block interpolate_block = {.lod = lod_f};
    ngpu_block_update(&s->interpolate.block, 0, &interpolate_block);

    /*
     * Interpolate the two blurred layers, source and mips[0], which
     * corresponds respectively to lod_i and lod_i+1.
     */
    ngli_rtt_begin(s->dst_rtt_ctx);
    ngpu_ctx_begin_render_pass(ctx->gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    ngli_pipeline_compat_update_image(s->interpolate.pl, 0, mip);
    ngli_pipeline_compat_update_image(s->interpolate.pl, 1, ngli_rtt_get_image(s->mips[0], 0));
    ngli_pipeline_compat_draw(s->interpolate.pl, 3, 1, 0);
    ngli_rtt_end(s->dst_rtt_ctx);

    /*
     * The downsample, upscample and interpolate passes do not deal with the
     * texture coordinates at all, thus we need to forward the source
     * coordinates matrix to the destination.
     */
    struct texture_info *dst_info = o->destination->priv_data;
    struct image *dst_image = &dst_info->image;
    memcpy(dst_image->coordinates_matrix, src_image->coordinates_matrix, sizeof(src_image->coordinates_matrix));
}

static void fgblur_release(struct ngl_node *node)
{
    struct fgblur_priv *s = node->priv_data;

    ngli_rtt_freep(&s->mip);
    for (size_t i = 0; i < MAX_MIP_LEVELS; i++)
        ngli_rtt_freep(&s->mips[i]);

    ngli_rtt_freep(&s->dst_rtt_ctx);
}

static void fgblur_uninit(struct ngl_node *node)
{
    struct fgblur_priv *s = node->priv_data;

    ngli_pipeline_compat_freep(&s->dws.pl);
    ngpu_pgcraft_freep(&s->dws.crafter);

    ngli_pipeline_compat_freep(&s->ups.pl);
    ngpu_pgcraft_freep(&s->ups.crafter);

    ngpu_block_reset(&s->down_up_data_block);

    ngli_pipeline_compat_freep(&s->interpolate.pl);
    ngpu_pgcraft_freep(&s->interpolate.crafter);
    ngpu_block_reset(&s->interpolate.block);
}

const struct node_class ngli_fgblur_class = {
    .id        = NGL_NODE_FASTGAUSSIANBLUR,
    .name      = "FastGaussianBlur",
    .init      = fgblur_init,
    .prepare   = ngli_node_prepare_children,
    .update    = ngli_node_update_children,
    .draw      = fgblur_draw,
    .release   = fgblur_release,
    .uninit    = fgblur_uninit,
    .opts_size = sizeof(struct fgblur_opts),
    .priv_size = sizeof(struct fgblur_priv),
    .params    = fgblur_params,
    .file      = __FILE__,
};
