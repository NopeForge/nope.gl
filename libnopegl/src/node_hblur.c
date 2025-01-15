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

#include "gpu_format.h"
#include "gpu_block.h"
#include "gpu_ctx.h"
#include "gpu_graphics_state.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "node_texture.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "gpu_rendertarget.h"
#include "rtt.h"
#include "utils.h"

/* GLSL shaders */
#include "blur_hexagonal_vert.h"
#include "blur_hexagonal_pass1_frag.h"
#include "blur_hexagonal_pass2_frag.h"

struct blur_params_block {
    int32_t radius;
    int32_t nb_samples;
};

struct hblur_opts {
    struct ngl_node *source;
    struct ngl_node *destination;
    struct ngl_node *amount_node;
    float amount;
    struct ngl_node *map;
};

struct hblur_priv {
    int32_t width;
    int32_t height;

    struct image *image;
    size_t image_rev;

    struct gpu_texture *dummy_map;
    struct image dummy_map_image;

    struct image *map_image;
    size_t map_rev;

    struct gpu_block blur_params_block;

    int preferred_format;
    struct gpu_texture *tex0;
    struct gpu_texture *tex1;

    struct {
        struct gpu_rendertarget_layout layout;
        struct rtt_ctx *rtt_ctx;
        struct pgcraft *crafter;
        struct pipeline_compat *pl;
    } pass1;

    int dst_is_resizeable;

    struct {
        struct gpu_rendertarget_layout layout;
        struct rtt_ctx *rtt_ctx;
        struct pgcraft *crafter;
        struct pipeline_compat *pl;
    } pass2;
};

#define OFFSET(x) offsetof(struct hblur_opts, x)
static const struct node_param hblur_params[] = {
    {"source",      NGLI_PARAM_TYPE_NODE, OFFSET(source),
                    .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                    .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                    .desc=NGLI_DOCSTRING("source to use for the blur")},
    {"destination", NGLI_PARAM_TYPE_NODE, OFFSET(destination),
                    .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                    .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                    .desc=NGLI_DOCSTRING("destination to use for the blur")},
    {"amount",      NGLI_PARAM_TYPE_F32, OFFSET(amount_node),
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("amount of bluriness in the range [0,1]")},
    {"map",         NGLI_PARAM_TYPE_NODE, OFFSET(map),
                    .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                    .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                    .desc=NGLI_DOCSTRING("blur map providing the CoC (circle of confusion) for each pixels (only the red channel is used)")},
    {NULL}
};

#define RENDER_TEXTURE_FEATURES (NGLI_GPU_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |               \
                                 NGLI_GPU_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | \
                                 NGLI_GPU_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)

static int get_preferred_format(struct gpu_ctx *gpu_ctx)
{
    static const int formats[] = {
        NGLI_GPU_FORMAT_R32G32B32A32_SFLOAT,
        NGLI_GPU_FORMAT_R16G16B16A16_SFLOAT,
        NGLI_GPU_FORMAT_R8G8B8A8_UNORM,
    };
    for (size_t i = 0; i < NGLI_ARRAY_NB(formats); i++) {
        const uint32_t features = ngli_gpu_ctx_get_format_features(gpu_ctx, formats[i]);
        if (NGLI_HAS_ALL_FLAGS(features, RENDER_TEXTURE_FEATURES))
            return formats[i];
    }
    ngli_assert(0);
}

#define DUMMY_MAP_SIZE 2

static int setup_dummy_map(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct hblur_priv *s = node->priv_data;

    s->dummy_map = ngli_gpu_texture_create(gpu_ctx);
    if (!s->dummy_map)
        return NGL_ERROR_MEMORY;

    const struct gpu_texture_params params = {
        .type    = NGLI_GPU_TEXTURE_TYPE_2D,
        .format  = NGLI_GPU_FORMAT_R8_UNORM,
        .width   = DUMMY_MAP_SIZE,
        .height  = DUMMY_MAP_SIZE,
        .usage   = NGLI_GPU_TEXTURE_USAGE_SAMPLED_BIT |
                   NGLI_GPU_TEXTURE_USAGE_TRANSFER_DST_BIT,
    };

    int ret = ngli_gpu_texture_init(s->dummy_map, &params);
    if (ret < 0)
        return ret;

    uint8_t buf[DUMMY_MAP_SIZE*DUMMY_MAP_SIZE];
    memset(buf, 255, sizeof(buf));
    ret = ngli_gpu_texture_upload(s->dummy_map, buf, 0);
    if (ret < 0)
        return ret;

    const struct image_params image_params = {
        .width  = DUMMY_MAP_SIZE,
        .height = DUMMY_MAP_SIZE,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .color_scale = 1.f,
        .color_info = {
            .space     = NMD_COL_SPC_BT709,
            .range     = NMD_COL_RNG_UNSPECIFIED,
            .primaries = NMD_COL_PRI_BT709,
            .transfer  = NMD_COL_TRC_IEC61966_2_1, // sRGB
        },
    };
    ngli_image_init(&s->dummy_map_image, &image_params, &s->dummy_map);

    return 0;
}

static int setup_pass1_pipeline(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct hblur_priv *s = node->priv_data;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "tex_coord", .type = NGLI_TYPE_VEC2},
        {.name = "map_coord", .type = NGLI_TYPE_VEC2},
    };

    static const struct pgcraft_texture textures[] = {
        {
            .name      = "tex",
            .type      = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage     = NGLI_GPU_PROGRAM_SHADER_FRAG,
        }, {
            .name      = "map",
            .type      = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage     = NGLI_GPU_PROGRAM_SHADER_FRAG,
        }
    };

    const struct pgcraft_block blocks[] = {
        {
            .name          = "blur",
            .type          = NGLI_TYPE_UNIFORM_BUFFER,
            .stage         = NGLI_GPU_PROGRAM_SHADER_FRAG,
            .block         = &s->blur_params_block.block,
            .buffer        = {
                .buffer    = s->blur_params_block.buffer,
                .size      = s->blur_params_block.block_size,
            },
        }
    };

    s->pass1.crafter = ngli_pgcraft_create(ctx);
    if (!s->pass1.crafter)
        return NGL_ERROR_MEMORY;

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/hexagonal-blur-pass1",
        .vert_base        = blur_hexagonal_vert,
        .frag_base        = blur_hexagonal_pass1_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .blocks           = blocks,
        .nb_blocks        = NGLI_ARRAY_NB(blocks),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
        .nb_frag_output   = 2,
    };

    int ret = ngli_pgcraft_craft(s->pass1.crafter, &crafter_params);
    if (ret < 0)
        return ret;

    s->pass1.layout = (struct gpu_rendertarget_layout) {
        .nb_colors        = 2,
        .colors[0].format = s->preferred_format,
        .colors[1].format = s->preferred_format,
    };

    const struct pipeline_compat_params params = {
        .type         = NGLI_GPU_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGLI_GPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGLI_GPU_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = s->pass1.layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(s->pass1.crafter),
        },
        .program      = ngli_pgcraft_get_program(s->pass1.crafter),
        .layout       = ngli_pgcraft_get_pipeline_layout(s->pass1.crafter),
        .resources    = ngli_pgcraft_get_pipeline_resources(s->pass1.crafter),
        .compat_info  = ngli_pgcraft_get_compat_info(s->pass1.crafter),
    };

    s->pass1.pl = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->pass1.pl)
        return NGL_ERROR_MEMORY;

    ret = ngli_pipeline_compat_init(s->pass1.pl, &params);
    if (ret < 0)
        return ret;

    ngli_pipeline_compat_update_texture(s->pass1.pl, 1, s->dummy_map);

    return 0;
}

static int setup_pass2_pipeline(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct hblur_priv *s = node->priv_data;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "tex_coord", .type = NGLI_TYPE_VEC2},
        {.name = "map_coord", .type = NGLI_TYPE_VEC2},
    };

    static const struct pgcraft_texture textures[] = {
        {
            .name      = "tex0",
            .type      = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage     = NGLI_GPU_PROGRAM_SHADER_FRAG,
        }, {
            .name      = "tex1",
            .type      = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage     = NGLI_GPU_PROGRAM_SHADER_FRAG,
        }, {
            .name      = "map",
            .type      = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage     = NGLI_GPU_PROGRAM_SHADER_FRAG,
        }
    };

    const struct pgcraft_block crafter_blocks[] = {
        {
            .name          = "blur",
            .type          = NGLI_TYPE_UNIFORM_BUFFER,
            .stage         = NGLI_GPU_PROGRAM_SHADER_FRAG,
            .block         = &s->blur_params_block.block,
            .buffer        = {
                .buffer = s->blur_params_block.buffer,
                .size   = s->blur_params_block.block_size,
            },
        },
    };

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/hexagonal-blur-pass2",
        .vert_base        = blur_hexagonal_vert,
        .frag_base        = blur_hexagonal_pass2_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .blocks           = crafter_blocks,
        .nb_blocks        = NGLI_ARRAY_NB(crafter_blocks),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    s->pass2.crafter = ngli_pgcraft_create(ctx);
    if (!s->pass2.crafter)
        return NGL_ERROR_MEMORY;

    int ret = ngli_pgcraft_craft(s->pass2.crafter, &crafter_params);
    if (ret < 0)
        return ret;

    s->pass2.pl = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->pass2.pl)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type         = NGLI_GPU_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGLI_GPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGLI_GPU_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = s->pass2.layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(s->pass2.crafter),
        },
        .program      = ngli_pgcraft_get_program(s->pass2.crafter),
        .layout       = ngli_pgcraft_get_pipeline_layout(s->pass2.crafter),
        .resources    = ngli_pgcraft_get_pipeline_resources(s->pass2.crafter),
        .compat_info  = ngli_pgcraft_get_compat_info(s->pass2.crafter),
    };

    ret = ngli_pipeline_compat_init(s->pass2.pl, &params);
    if (ret < 0)
        return ret;

    ngli_pipeline_compat_update_texture(s->pass2.pl, 2, s->dummy_map);

    return 0;
}

static int hblur_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct hblur_priv *s = node->priv_data;
    struct hblur_opts *o = node->opts;

    struct texture_info *src_info = o->source->priv_data;
    s->image = &src_info->image;
    s->image_rev = SIZE_MAX;

    /* Disable direct rendering */
    src_info->supported_image_layouts = NGLI_IMAGE_LAYOUT_DEFAULT_BIT;

    /* Override texture params */
    src_info->params.min_filter = NGLI_GPU_FILTER_LINEAR;
    src_info->params.mag_filter = NGLI_GPU_FILTER_LINEAR;
    src_info->params.mipmap_filter = NGLI_GPU_MIPMAP_FILTER_LINEAR;

    s->map_image = &s->dummy_map_image;
    s->map_rev = SIZE_MAX;
    if (o->map) {
        struct texture_info *map_info = o->map->priv_data;

        /* Disable direct rendering */
        map_info->supported_image_layouts = NGLI_IMAGE_LAYOUT_DEFAULT_BIT;

        /* Override gpu_texture params */
        map_info->params.min_filter = NGLI_GPU_FILTER_LINEAR;
        map_info->params.mag_filter = NGLI_GPU_FILTER_LINEAR;
        s->map_image = &map_info->image;
    }

    s->preferred_format = get_preferred_format(ctx->gpu_ctx);

    struct texture_info *dst_info = o->destination->priv_data;
    dst_info->params.usage |= NGLI_GPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    s->dst_is_resizeable = (dst_info->params.width == 0 && dst_info->params.height == 0);
    s->pass2.layout.colors[0].format = dst_info->params.format;
    s->pass2.layout.nb_colors = 1;

    const struct gpu_block_field block_fields[] = {
        NGLI_GPU_BLOCK_FIELD(struct blur_params_block, radius, NGLI_TYPE_I32, 0),
        NGLI_GPU_BLOCK_FIELD(struct blur_params_block, nb_samples, NGLI_TYPE_I32, 0),
    };

    const struct gpu_block_params block_params = {
        .fields    = block_fields,
        .nb_fields = NGLI_ARRAY_NB(block_fields),
    };

    int ret = ngli_gpu_block_init(gpu_ctx, &s->blur_params_block, &block_params);
    if (ret < 0)
        return ret;

    if ((ret = setup_dummy_map(node)) < 0 ||
        (ret = setup_pass1_pipeline(node)) < 0 ||
        (ret = setup_pass2_pipeline(node)) < 0)
        return ret;

    return 0;
}

static int resize(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct hblur_priv *s = node->priv_data;
    struct hblur_opts *o = node->opts;

    ngli_node_draw(o->source);
    if (o->map)
        ngli_node_draw(o->map);

    struct texture_info *src_info = o->source->priv_data;
    const int32_t width = src_info->image.params.width;
    const int32_t height = src_info->image.params.height;
    if (s->width == width && s->height == height)
        return 0;

    struct gpu_texture *dst = NULL;
    struct gpu_texture *tex0 = ngli_gpu_texture_create(ctx->gpu_ctx);
    struct gpu_texture *tex1 = ngli_gpu_texture_create(ctx->gpu_ctx);
    struct rtt_ctx *pass1_rtt_ctx = ngli_rtt_create(ctx);
    struct rtt_ctx *pass2_rtt_ctx = ngli_rtt_create(ctx);
    if (!tex0 || !tex1 || !pass1_rtt_ctx || !pass2_rtt_ctx) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    struct gpu_texture_params texture_params = {
        .type          = NGLI_GPU_TEXTURE_TYPE_2D,
        .format        = s->preferred_format,
        .width         = width,
        .height        = height,
        .min_filter    = NGLI_GPU_FILTER_LINEAR,
        .mag_filter    = NGLI_GPU_FILTER_LINEAR,
        .wrap_s        = NGLI_GPU_WRAP_CLAMP_TO_EDGE,
        .wrap_t        = NGLI_GPU_WRAP_CLAMP_TO_EDGE,
        .usage         = NGLI_GPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                         NGLI_GPU_TEXTURE_USAGE_SAMPLED_BIT,
    };

    ret = ngli_gpu_texture_init(tex0, &texture_params);
    if (ret < 0)
        goto fail;

    ret = ngli_gpu_texture_init(tex1, &texture_params);
    if (ret < 0)
        goto fail;

    struct rtt_params pass1_rtt_params = {
        .width = width,
        .height = height,
        .nb_colors = 2,
        .colors[0] = {
            .attachment = tex0,
            .store_op = NGLI_GPU_STORE_OP_STORE
        },
        .colors[1] = {
            .attachment = tex1,
            .store_op = NGLI_GPU_STORE_OP_STORE
        },
    };

    ret = ngli_rtt_init(pass1_rtt_ctx, &pass1_rtt_params);
    if (ret < 0)
        goto fail;

    /* Assert that the destination texture format does not change */
    struct texture_info *dst_info = o->destination->priv_data;
    ngli_assert(dst_info->params.format == s->pass2.layout.colors[0].format);

    dst = dst_info->texture;
    if (s->dst_is_resizeable) {
        dst = ngli_gpu_texture_create(ctx->gpu_ctx);
        if (!dst) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        struct gpu_texture_params params = dst_info->params;
        params.width = width;
        params.height = height;
        ret = ngli_gpu_texture_init(dst, &params);
        if (ret < 0)
            goto fail;
    }

    const struct rtt_params pass2_rtt_params = {
        .width  = dst->params.width,
        .height = dst->params.height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = dst,
            .load_op = NGLI_GPU_LOAD_OP_CLEAR,
            .store_op = NGLI_GPU_STORE_OP_STORE,
        },
    };

    ret = ngli_rtt_init(pass2_rtt_ctx, &pass2_rtt_params);
    if (ret < 0)
        goto fail;

    ngli_rtt_freep(&s->pass1.rtt_ctx);
    s->pass1.rtt_ctx = pass1_rtt_ctx;

    ngli_gpu_texture_freep(&s->tex0);
    s->tex0 = tex0;

    ngli_gpu_texture_freep(&s->tex1);
    s->tex1 = tex1;

    ngli_rtt_freep(&s->pass2.rtt_ctx);
    s->pass2.rtt_ctx = pass2_rtt_ctx;

    ngli_pipeline_compat_update_image(s->pass2.pl, 0, ngli_rtt_get_image(s->pass1.rtt_ctx, 0));
    ngli_pipeline_compat_update_image(s->pass2.pl, 1, ngli_rtt_get_image(s->pass1.rtt_ctx, 1));

    if (s->dst_is_resizeable) {
        ngli_gpu_texture_freep(&dst_info->texture);
        dst_info->texture = dst;
        dst_info->image.params.width = dst->params.width;
        dst_info->image.params.height = dst->params.height;
        dst_info->image.planes[0] = dst;
        dst_info->image.rev = dst_info->image_rev++;
    }

    s->width = width;
    s->height = height;

    return 0;

fail:
    ngli_gpu_texture_freep(&tex0);
    ngli_gpu_texture_freep(&tex1);
    ngli_rtt_freep(&pass1_rtt_ctx);

    ngli_rtt_freep(&pass2_rtt_ctx);
    if (s->dst_is_resizeable)
        ngli_gpu_texture_freep(&dst);

    LOG(ERROR, "failed to resize blur: %dx%d", width, height);
    return ret;
}

#define MAX_SAMPLES 32

static void hblur_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct hblur_priv *s = node->priv_data;
    struct hblur_opts *o = node->opts;

    int ret = resize(node);
    if (ret < 0)
        return;

    const float amount_raw = *(float *)ngli_node_get_data_ptr(o->amount_node, &o->amount);
    float amount = NGLI_CLAMP(amount_raw, 0.f, 1.f);
    const float diagonal = hypotf((float)s->width, (float)s->height);
    const int32_t radius = (int32_t)(amount * (float)(diagonal) * 0.05f);
    const int32_t nb_samples = NGLI_MIN(radius, MAX_SAMPLES);

    ngli_gpu_block_update(&s->blur_params_block, 0, &(struct blur_params_block) {
        .radius = radius,
        .nb_samples = nb_samples,
    });

    ngli_rtt_begin(s->pass1.rtt_ctx);
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    if (s->image_rev != s->image->rev) {
        ngli_pipeline_compat_update_image(s->pass1.pl, 0, s->image);
        s->image_rev = s->image->rev;
    }
    if (s->map_rev != s->map_image->rev) {
        ngli_pipeline_compat_update_image(s->pass1.pl, 1, s->map_image);
        s->image_rev = s->map_image->rev;
    }
    ngli_pipeline_compat_draw(s->pass1.pl, 3, 1, 0);
    ngli_rtt_end(s->pass1.rtt_ctx);

    ngli_rtt_begin(s->pass2.rtt_ctx);
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    if (s->map_rev != s->map_image->rev) {
        ngli_pipeline_compat_update_image(s->pass2.pl, 2, s->map_image);
        s->image_rev = s->map_image->rev;
    }
    ngli_pipeline_compat_draw(s->pass2.pl, 3, 1, 0);
    ngli_rtt_end(s->pass2.rtt_ctx);

    /*
     * The blur render passes do not deal with the texture coordinates at all,
     * thus we need to forward the source coordinates matrix to the
     * destination.
     */
    struct texture_info *dst_info = o->destination->priv_data;
    struct image *dst_image = &dst_info->image;
    memcpy(dst_image->coordinates_matrix, s->image->coordinates_matrix, sizeof(s->image->coordinates_matrix));
}

static void hblur_release(struct ngl_node *node)
{
    struct hblur_priv *s = node->priv_data;

    ngli_gpu_texture_freep(&s->tex0);
    ngli_gpu_texture_freep(&s->tex1);
    ngli_rtt_freep(&s->pass1.rtt_ctx);
    ngli_rtt_freep(&s->pass2.rtt_ctx);
}

static void hblur_uninit(struct ngl_node *node)
{
    struct hblur_priv *s = node->priv_data;

    ngli_gpu_block_reset(&s->blur_params_block);
    ngli_gpu_texture_freep(&s->dummy_map);
    ngli_pipeline_compat_freep(&s->pass2.pl);
    ngli_pipeline_compat_freep(&s->pass1.pl);
    ngli_pgcraft_freep(&s->pass1.crafter);
    ngli_pgcraft_freep(&s->pass2.crafter);
}

const struct node_class ngli_hblur_class = {
    .id        = NGL_NODE_HEXAGONALBLUR,
    .name      = "HexagonalBlur",
    .init      = hblur_init,
    .prepare   = ngli_node_prepare_children,
    .update    = ngli_node_update_children,
    .draw      = hblur_draw,
    .release   = hblur_release,
    .uninit    = hblur_uninit,
    .opts_size = sizeof(struct hblur_opts),
    .priv_size = sizeof(struct hblur_priv),
    .params    = hblur_params,
    .file      = __FILE__,
};
