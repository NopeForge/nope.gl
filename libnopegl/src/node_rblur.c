/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023-2024 Nope Forge
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

#include "gpu_block.h"
#include "gpu_ctx.h"
#include "graphics_state.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "nopegl.h"
#include "rendertarget.h"
#include "rtt.h"
#include "topology.h"
#include "utils.h"

/* GLSL shaders */
#include "blur_common_vert.h"
#include "blur_radial_zoom_frag.h"

struct params_block {
    float amount;
    float center[2];
};

struct rblur_opts {
    struct ngl_node *source;
    struct ngl_node *destination;
    struct ngl_node *amount_node;
    float amount;
    struct ngl_node *center_node;
    float center[2];
};

struct rblur_priv {
    int32_t width;
    int32_t height;

    struct image *image;
    size_t image_rev;

    int dst_is_resizeable;
    struct rendertarget_layout dst_layout;
    struct rtt_ctx *dst_rtt_ctx;

    struct gpu_block blur_params;
    struct pgcraft *crafter;
    struct pipeline_compat *pl_blur_r;
};

#define OFFSET(x) offsetof(struct rblur_opts, x)
static const struct node_param rblur_params[] = {
    {"source",            NGLI_PARAM_TYPE_NODE, OFFSET(source),
                          .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                          .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                          .desc=NGLI_DOCSTRING("source to use for the blur")},
    {"destination",       NGLI_PARAM_TYPE_NODE, OFFSET(destination),
                          .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                          .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                          .desc=NGLI_DOCSTRING("destination to use for the blur")},
    {"amount",            NGLI_PARAM_TYPE_F32, OFFSET(amount_node),
                          .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                          .desc=NGLI_DOCSTRING("amount of bluriness in the range [-1,1]")},
    {"center",            NGLI_PARAM_TYPE_VEC2, OFFSET(center_node),
                          .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                          .desc=NGLI_DOCSTRING("center of the radial blur")},
    {NULL}
};

static int rblur_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rblur_priv *s = node->priv_data;
    struct rblur_opts *o = node->opts;

    struct texture_priv *src_priv = o->source->priv_data;
    s->image = &src_priv->image;
    s->image_rev = SIZE_MAX;

    /* Disable direct rendering */
    src_priv->supported_image_layouts = 1U << NGLI_IMAGE_LAYOUT_DEFAULT;

    /* Override texture params */
    src_priv->params.min_filter = NGLI_FILTER_LINEAR;
    src_priv->params.min_filter = NGLI_FILTER_LINEAR;
    src_priv->params.mipmap_filter = NGLI_MIPMAP_FILTER_LINEAR;

    struct texture_priv *dst_priv = o->destination->priv_data;
    dst_priv->params.usage |= NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    s->dst_is_resizeable = (dst_priv->params.width == 0 && dst_priv->params.height == 0);
    s->dst_layout.colors[0].format = dst_priv->params.format;
    s->dst_layout.nb_colors = 1;

    const struct gpu_block_field params_fields[] = {
        NGLI_GPU_BLOCK_FIELD(struct params_block, amount, NGLI_TYPE_F32, 0),
        NGLI_GPU_BLOCK_FIELD(struct params_block, center, NGLI_TYPE_VEC2, 0),
    };
    const struct gpu_block_params blur_params = {
        .count     = 1,
        .fields    = params_fields,
        .nb_fields = NGLI_ARRAY_NB(params_fields),
    };

    int ret = ngli_gpu_block_init(gpu_ctx, &s->blur_params, &blur_params);
    if (ret < 0)
        return ret;

    const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "tex_coord", .type = NGLI_TYPE_VEC2},
    };

    const struct pgcraft_texture textures[] = {
        {
            .name = "tex",
            .type = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .precision = NGLI_PRECISION_HIGH,
            .stage = NGLI_PROGRAM_SHADER_FRAG
        },
    };

    const struct pgcraft_block crafter_blocks[] = {
        {
            .name          = "blur_params",
            .instance_name = "",
            .type          = NGLI_TYPE_UNIFORM_BUFFER,
            .stage         = NGLI_PROGRAM_SHADER_FRAG,
            .block         = &s->blur_params.block,
            .buffer        = {
                .buffer = s->blur_params.buffer,
                .size   = s->blur_params.block_size,
            },
        },
    };

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/radial-blur",
        .vert_base        = blur_common_vert,
        .frag_base        = blur_radial_zoom_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .blocks           = crafter_blocks,
        .nb_blocks        = NGLI_ARRAY_NB(crafter_blocks),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    s->crafter = ngli_pgcraft_create(ctx);
    if (!s->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcraft_craft(s->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    s->pl_blur_r = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->pl_blur_r)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type         = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGLI_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = s->dst_layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(s->crafter),
        },
        .program      = ngli_pgcraft_get_program(s->crafter),
        .layout       = ngli_pgcraft_get_pipeline_layout(s->crafter),
        .resources    = ngli_pgcraft_get_pipeline_resources(s->crafter),
        .compat_info  = ngli_pgcraft_get_compat_info(s->crafter),
    };

    ret = ngli_pipeline_compat_init(s->pl_blur_r, &params);
    if (ret < 0)
        return ret;

    const int32_t index = ngli_pgcraft_get_uniform_index(s->crafter, "tex_coord_matrix", NGLI_PROGRAM_SHADER_VERT);
    ngli_assert(index >= 0);

    NGLI_ALIGNED_MAT(tmp_coord_matrix) = NGLI_MAT4_IDENTITY;
    ngli_pipeline_compat_update_uniform(s->pl_blur_r, index, tmp_coord_matrix);

    return 0;
}

static int resize(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct rblur_priv *s = node->priv_data;
    struct rblur_opts *o = node->opts;

    ngli_node_draw(o->source);

    struct texture_priv *src_priv = o->source->priv_data;
    const int32_t width = src_priv->image.params.width;
    const int32_t height = src_priv->image.params.height;
    if (s->width == width && s->height == height)
        return 0;

    /* Assert that the destination texture format does not change */
    struct texture_priv *dst_priv = o->destination->priv_data;
    ngli_assert(dst_priv->params.format == s->dst_layout.colors[0].format);

    struct texture *dst = NULL;
    struct rtt_ctx *dst_rtt_ctx = NULL;

    dst = dst_priv->texture;
    if (s->dst_is_resizeable) {
        dst = ngli_texture_create(ctx->gpu_ctx);
        if (!dst) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        struct texture_params params = dst_priv->params;
        params.width = width;
        params.height = height;
        ret = ngli_texture_init(dst, &params);
        if (ret < 0)
            goto fail;
    }

    if (s->dst_is_resizeable) {
        ngli_texture_freep(&dst_priv->texture);
        dst_priv->texture = dst;
        dst_priv->image.params.width = dst->params.width;
        dst_priv->image.params.height = dst->params.height;
        dst_priv->image.planes[0] = dst;
        dst_priv->image.rev = dst_priv->image_rev++;
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
            .load_op = NGLI_LOAD_OP_CLEAR,
            .store_op = NGLI_STORE_OP_STORE,
        },
    };

    ret = ngli_rtt_init(dst_rtt_ctx, &rtt_params);
    if (ret < 0)
        goto fail;

    ngli_rtt_freep(&s->dst_rtt_ctx);
    s->dst_rtt_ctx = dst_rtt_ctx;

    s->width = width;
    s->height = height;

    return 0;

fail:
    ngli_rtt_freep(&dst_rtt_ctx);
    if (s->dst_is_resizeable)
        ngli_texture_freep(&dst);

    LOG(ERROR, "failed to resize blur: %dx%d", width, height);
    return ret;
}

static void rblur_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rblur_priv *s = node->priv_data;
    struct rblur_opts *o = node->opts;

    int ret = resize(node);
    if (ret < 0)
        return;

    const float amount_raw = *(float *)ngli_node_get_data_ptr(o->amount_node, &o->amount);
    float amount = NGLI_CLAMP(amount_raw, -1.f, 1.f);

    const float *center = (float *)ngli_node_get_data_ptr(o->center_node, &o->center);

    ngli_gpu_block_update(&s->blur_params, 0, &(struct params_block) {
        .amount      = amount,
        .center      = {center[0], center[1]},
    });

    ngli_rtt_begin(s->dst_rtt_ctx);
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    if (s->image_rev != s->image->rev) {
        ngli_pipeline_compat_update_image(s->pl_blur_r, 0, s->image);
        s->image_rev = s->image->rev;
    }
    ngli_pipeline_compat_draw(s->pl_blur_r, 3, 1);
    ngli_rtt_end(s->dst_rtt_ctx);

    /*
     * The blur render pass does not deal with the texture coordinates at all,
     * thus we need to forward the source coordinates matrix to the
     * destination.
     */
    struct texture_priv *dst_priv = (struct texture_priv *)o->destination->priv_data;
    struct image *dst_image = &dst_priv->image;
    memcpy(dst_image->coordinates_matrix, s->image->coordinates_matrix, sizeof(s->image->coordinates_matrix));
}

static void rblur_release(struct ngl_node *node)
{
    struct rblur_priv *s = node->priv_data;

    ngli_rtt_freep(&s->dst_rtt_ctx);
}

static void rblur_uninit(struct ngl_node *node)
{
    struct rblur_priv *s = node->priv_data;

    ngli_gpu_block_reset(&s->blur_params);
    ngli_pipeline_compat_freep(&s->pl_blur_r);
    ngli_pgcraft_freep(&s->crafter);
}

const struct node_class ngli_rblur_class = {
    .id        = NGL_NODE_RADIALBLUR,
    .name      = "RadialBlur",
    .init      = rblur_init,
    .prepare   = ngli_node_prepare_children,
    .update    = ngli_node_update_children,
    .draw      = rblur_draw,
    .release   = rblur_release,
    .uninit    = rblur_uninit,
    .opts_size = sizeof(struct rblur_opts),
    .priv_size = sizeof(struct rblur_priv),
    .params    = rblur_params,
    .file      = __FILE__,
};
