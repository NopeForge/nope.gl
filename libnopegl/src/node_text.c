/*
 * Copyright 2019-2022 GoPro Inc.
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

#include <stddef.h>
#include <string.h>

#include "memory.h"
#include "internal.h"
#include "darray.h"
#include "gpu_ctx.h"
#include "log.h"
#include "math_utils.h"
#include "pgcache.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "text.h"
#include "type.h"
#include "topology.h"
#include "utils.h"

/* GLSL fragments as string */
#include "text_bg_frag.h"
#include "text_bg_vert.h"
#include "text_chars_frag.h"
#include "text_chars_vert.h"

#define VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                            NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT) \

#define INDEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                           NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT)  \

#define DYNAMIC_VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_DYNAMIC_BIT | VERTEX_USAGE_FLAGS)
#define DYNAMIC_INDEX_USAGE_FLAGS  (NGLI_BUFFER_USAGE_DYNAMIC_BIT | INDEX_USAGE_FLAGS)

struct pipeline_subdesc {
    struct pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
    int32_t modelview_matrix_index;
    int32_t projection_matrix_index;
    int32_t color_index;
    int32_t opacity_index;
};

struct pipeline_desc {
    struct pipeline_subdesc bg; /* Background (bounding box) */
    struct pipeline_subdesc fg; /* Foreground (characters) */
};

struct text_opts {
    struct livectl live;
    float fg_color[3];
    float fg_opacity;
    float bg_color[3];
    float bg_opacity;
    float box_corner[3];
    float box_width[3];
    float box_height[3];
    int32_t padding;
    float font_scale;
    int valign, halign;
    int32_t aspect_ratio[2];
};

struct text_priv {
    struct text *text_ctx;
    struct buffer *vertices;
    struct buffer *uvcoords;
    struct buffer *indices;
    size_t nb_indices;

    struct buffer *bg_vertices;

    struct darray pipeline_descs;
    int live_changed;
};

static const struct param_choices valign_choices = {
    .name = "valign",
    .consts = {
        {"center", NGLI_TEXT_VALIGN_CENTER, .desc=NGLI_DOCSTRING("vertically centered")},
        {"bottom", NGLI_TEXT_VALIGN_BOTTOM, .desc=NGLI_DOCSTRING("bottom positioned")},
        {"top",    NGLI_TEXT_VALIGN_TOP,    .desc=NGLI_DOCSTRING("top positioned")},
        {NULL}
    }
};

static const struct param_choices halign_choices = {
    .name = "halign",
    .consts = {
        {"center", NGLI_TEXT_HALIGN_CENTER, .desc=NGLI_DOCSTRING("horizontally centered")},
        {"right",  NGLI_TEXT_HALIGN_RIGHT,  .desc=NGLI_DOCSTRING("right positioned")},
        {"left",   NGLI_TEXT_HALIGN_LEFT,   .desc=NGLI_DOCSTRING("left positioned")},
        {NULL}
    }
};

static int set_live_changed(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    s->live_changed = 1;
    return 0;
}

#define OFFSET(x) offsetof(struct text_opts, x)
static const struct node_param text_params[] = {
    {"text",         NGLI_PARAM_TYPE_STR, OFFSET(live.val.s), {.str=""},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_NON_NULL,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("text string to rasterize")},
    {"live_id",      NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                     .desc=NGLI_DOCSTRING("live control identifier")},
    {"fg_color",     NGLI_PARAM_TYPE_VEC3, OFFSET(fg_color), {.vec={1.0, 1.0, 1.0}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("foreground text color")},
    {"fg_opacity",   NGLI_PARAM_TYPE_F32, OFFSET(fg_opacity), {.f32=1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("foreground text opacity")},
    {"bg_color",     NGLI_PARAM_TYPE_VEC3, OFFSET(bg_color), {.vec={0.0, 0.0, 0.0}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("background text color")},
    {"bg_opacity",   NGLI_PARAM_TYPE_F32, OFFSET(bg_opacity), {.f32=.8f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("background text opacity")},
    {"box_corner",   NGLI_PARAM_TYPE_VEC3, OFFSET(box_corner), {.vec={-1.0, -1.0, 0.0}},
                     .desc=NGLI_DOCSTRING("origin coordinates of `box_width` and `box_height` vectors")},
    {"box_width",    NGLI_PARAM_TYPE_VEC3, OFFSET(box_width), {.vec={2.0, 0.0, 0.0}},
                     .desc=NGLI_DOCSTRING("box width vector")},
    {"box_height",   NGLI_PARAM_TYPE_VEC3, OFFSET(box_height), {.vec={0.0, 2.0, 0.0}},
                     .desc=NGLI_DOCSTRING("box height vector")},
    {"padding",      NGLI_PARAM_TYPE_I32, OFFSET(padding), {.i32=3},
                     .desc=NGLI_DOCSTRING("pixel padding around the text")},
    {"font_scale",   NGLI_PARAM_TYPE_F32, OFFSET(font_scale), {.f32=1.f},
                     .desc=NGLI_DOCSTRING("scaling of the font")},
    {"valign",       NGLI_PARAM_TYPE_SELECT, OFFSET(valign), {.i32=NGLI_TEXT_VALIGN_CENTER},
                     .choices=&valign_choices,
                     .desc=NGLI_DOCSTRING("vertical alignment of the text in the box")},
    {"halign",       NGLI_PARAM_TYPE_SELECT, OFFSET(halign), {.i32=NGLI_TEXT_HALIGN_CENTER},
                     .choices=&halign_choices,
                     .desc=NGLI_DOCSTRING("horizontal alignment of the text in the box")},
    {"aspect_ratio", NGLI_PARAM_TYPE_RATIONAL, OFFSET(aspect_ratio),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("box aspect ratio")},
    {NULL}
};

static const struct pgcraft_iovar vert_out_vars[] = {
    {.name = "tex_coord", .type = NGLI_TYPE_VEC2},
};

#define BC(index) o->box_corner[index]
#define BW(index) o->box_width[index]
#define BH(index) o->box_height[index]

#define C(index) chr_corner[index]
#define W(index) chr_width[index]
#define H(index) chr_height[index]

static void destroy_characters_resources(struct text_priv *s)
{
    ngli_buffer_freep(&s->vertices);
    ngli_buffer_freep(&s->uvcoords);
    ngli_buffer_freep(&s->indices);
    s->nb_indices = 0;
}

static int update_character_geometries(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;
    struct text_opts *o = node->opts;

    const char *str = o->live.val.s;
    struct text *text = s->text_ctx;

    int ret = ngli_text_set_string(s->text_ctx, str);
    if (ret < 0)
        return ret;

    const size_t text_nbchr = ngli_darray_count(&text->chars);
    if (!text_nbchr) {
        destroy_characters_resources(s);
        return 0;
    }

    const size_t nb_vertices = text_nbchr * 4;
    const size_t nb_uvcoords = text_nbchr * 4;
    const size_t nb_indices  = text_nbchr * 6;
    if (nb_indices > INT32_MAX)
        return NGL_ERROR_LIMIT_EXCEEDED;

    float *vertices = ngli_calloc(nb_vertices, 3 * sizeof(*vertices));
    float *uvcoords = ngli_calloc(nb_uvcoords, 2 * sizeof(*uvcoords));
    int16_t *indices = ngli_calloc(nb_indices, sizeof(*indices));
    if (!vertices || !uvcoords || !indices) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    /* Text/Box ratio */
    const float box_width_len  = ngli_vec3_length(o->box_width);
    const float box_height_len = ngli_vec3_length(o->box_height);
    static const int32_t default_ar[2] = {1, 1};
    const int32_t *ar = o->aspect_ratio[1] ? o->aspect_ratio : default_ar;
    const float box_ratio = (float)ar[0] * box_width_len / ((float)ar[1] * box_height_len);
    const float text_ratio = (float)text->width / (float)text->height;

    float ratio_w, ratio_h;
    if (text_ratio < box_ratio) {
        ratio_w = text_ratio / box_ratio;
        ratio_h = 1.0;
    } else {
        ratio_w = 1.0;
        ratio_h = box_ratio / text_ratio;
    }

    /* Apply aspect ratio and font scaling */
    float width[3];
    float height[3];
    ngli_vec3_scale(width, o->box_width, ratio_w * o->font_scale);
    ngli_vec3_scale(height, o->box_height, ratio_h * o->font_scale);

    /* Adjust text position according to alignment settings */
    const float align_padw[3] = NGLI_VEC3_SUB(o->box_width, width);
    const float align_padh[3] = NGLI_VEC3_SUB(o->box_height, height);

    const float spx = (o->halign == NGLI_TEXT_HALIGN_CENTER ? .5f :
                       o->halign == NGLI_TEXT_HALIGN_RIGHT  ? 1.f :
                       0.f);
    const float spy = (o->valign == NGLI_TEXT_VALIGN_CENTER ? .5f :
                       o->valign == NGLI_TEXT_VALIGN_TOP    ? 1.f :
                       0.f);

    const float corner[3] = {
        BC(0) + align_padw[0] * spx + align_padh[0] * spy,
        BC(1) + align_padw[1] * spx + align_padh[1] * spy,
        BC(2) + align_padw[2] * spx + align_padh[2] * spy,
    };

    const struct char_info *chars = ngli_darray_data(&text->chars);
    for (size_t n = 0; n < text_nbchr; n++) {
        const struct char_info *chr = &chars[n];
        float chr_width[3], chr_height[3];

        /* character dimension and position */
        ngli_vec3_scale(chr_width, width, chr->w);
        ngli_vec3_scale(chr_height, height, chr->h);
        const float chr_corner[3] = {
            corner[0] + width[0] * chr->x + height[0] * chr->y,
            corner[1] + width[1] * chr->x + height[1] * chr->y,
            corner[2] + width[2] * chr->x + height[2] * chr->y,
        };

        /* deduce quad vertices from chr_{widht,height,corner} */
        const float chr_vertices[] = {
            C(0),               C(1),               C(2),
            C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
            C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
            C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
        };
        memcpy(vertices + 4 * 3 * n, chr_vertices, sizeof(chr_vertices));

        /* uvcoords of the character in the atlas texture */
        memcpy(uvcoords + 4 * 2 * n, chr->uvcoords, sizeof(chr->uvcoords));

        /* quad for each character is made of 2 triangles */
        const int16_t n4 = (int16_t)n * 4;
        const int16_t chr_indices[] = { n4 + 0, n4 + 1, n4 + 2, n4 + 1, n4 + 3, n4 + 2 };
        memcpy(indices + n * NGLI_ARRAY_NB(chr_indices), chr_indices, sizeof(chr_indices));
    }

    if (nb_indices > s->nb_indices) { // need re-alloc
        destroy_characters_resources(s);

        s->vertices = ngli_buffer_create(gpu_ctx);
        s->uvcoords = ngli_buffer_create(gpu_ctx);
        s->indices  = ngli_buffer_create(gpu_ctx);
        if (!s->vertices || !s->uvcoords || !s->indices) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }

        if ((ret = ngli_buffer_init(s->vertices, nb_vertices * 3 * sizeof(*vertices), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->uvcoords, nb_uvcoords * 2 * sizeof(*uvcoords), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->indices,  nb_indices  * sizeof(*indices),  DYNAMIC_INDEX_USAGE_FLAGS)) < 0)
            goto end;

        struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
        for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
            struct pipeline_subdesc *desc = &descs[i].fg;

            ngli_pipeline_compat_update_attribute(desc->pipeline_compat, 0, s->vertices);
            ngli_pipeline_compat_update_attribute(desc->pipeline_compat, 1, s->uvcoords);
        }
    }

    if ((ret = ngli_buffer_upload(s->vertices, vertices, nb_vertices * 3 * sizeof(*vertices), 0)) < 0 ||
        (ret = ngli_buffer_upload(s->uvcoords, uvcoords, nb_uvcoords * 2 * sizeof(*uvcoords), 0)) < 0 ||
        (ret = ngli_buffer_upload(s->indices, indices, nb_indices * sizeof(*indices), 0)) < 0)
        goto end;

    s->nb_indices = nb_indices;

end:
    ngli_free(vertices);
    ngli_free(uvcoords);
    ngli_free(indices);
    return ret;
}

static int init_bounding_box_geometry(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    const float vertices[] = {
        BC(0),                 BC(1),                 BC(2),
        BC(0) + BW(0),         BC(1) + BW(1),         BC(2) + BW(2),
        BC(0) + BH(0),         BC(1) + BH(1),         BC(2) + BH(2),
        BC(0) + BH(0) + BW(0), BC(1) + BH(1) + BW(1), BC(2) + BH(2) + BW(2),
    };

    s->bg_vertices = ngli_buffer_create(gpu_ctx);
    if (!s->bg_vertices)
        return NGL_ERROR_MEMORY;

    int ret;
    if ((ret = ngli_buffer_init(s->bg_vertices, sizeof(vertices), VERTEX_USAGE_FLAGS)) < 0 ||
        (ret = ngli_buffer_upload(s->bg_vertices, vertices, sizeof(vertices), 0)) < 0)
        return ret;

    return 0;
}

static int text_init(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    s->text_ctx = ngli_text_create(node->ctx);
    if (!s->text_ctx)
        return NGL_ERROR_MEMORY;

    const struct text_config config = {
        .padding = o->padding,
    };

    int ret = ngli_text_init(s->text_ctx, &config);
    if (ret < 0)
        return ret;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    ret = init_bounding_box_geometry(node);
    if (ret < 0)
        return ret;

    ret = update_character_geometries(node);
    if (ret < 0)
        return ret;

    return 0;
}

static int init_subdesc(struct ngl_node *node,
                        struct pipeline_subdesc *desc,
                        struct pipeline_params *pipeline_params,
                        const struct pgcraft_params *crafter_params)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    int ret = ngli_pgcraft_craft(desc->crafter, crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!desc->pipeline_compat)
        return NGL_ERROR_MEMORY;

    pipeline_params->program = ngli_pgcraft_get_program(desc->crafter);
    pipeline_params->layout = ngli_pgcraft_get_pipeline_layout(desc->crafter);

    const struct pipeline_resources pipeline_resources = ngli_pgcraft_get_pipeline_resources(desc->crafter);
    const struct pgcraft_compat_info *compat_info = ngli_pgcraft_get_compat_info(desc->crafter);

    const struct pipeline_compat_params params = {
        .params = pipeline_params,
        .resources = &pipeline_resources,
        .compat_info = compat_info,
    };

    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->color_index = ngli_pgcraft_get_uniform_index(desc->crafter, "color", NGLI_PROGRAM_SHADER_FRAG);
    desc->opacity_index = ngli_pgcraft_get_uniform_index(desc->crafter, "opacity", NGLI_PROGRAM_SHADER_FRAG);

    return 0;
}

static int bg_prepare(struct ngl_node *node, struct pipeline_subdesc *desc)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "color",             .type = NGLI_TYPE_VEC3, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = o->bg_color},
        {.name = "opacity",           .type = NGLI_TYPE_F32,  .stage = NGLI_PROGRAM_SHADER_FRAG, .data = &o->bg_opacity},
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC3,
            .format   = NGLI_FORMAT_R32G32B32_SFLOAT,
            .stride   = 3 * 4,
            .buffer   = s->bg_vertices,
        },
    };

    /* This controls how the background blends onto the current framebuffer */
    struct graphicstate state = rnode->graphicstate;
    state.blend = 1;
    state.blend_src_factor   = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor   = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.blend_src_factor_a = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology       = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state          = state,
            .rt_desc        = rnode->rendertarget_desc,
        }
    };

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/text-bg",
        .vert_base        = text_bg_vert,
        .frag_base        = text_bg_frag,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
    };

    return init_subdesc(node, desc, &pipeline_params, &crafter_params);
}

static int fg_prepare(struct ngl_node *node, struct pipeline_subdesc *desc)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "color",             .type = NGLI_TYPE_VEC3, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = o->fg_color},
        {.name = "opacity",           .type = NGLI_TYPE_F32,  .stage = NGLI_PROGRAM_SHADER_FRAG, .data = &o->fg_opacity},
    };

    const struct pgcraft_texture textures[] = {
        {
            .name     = "tex",
            .type     = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage    = NGLI_PROGRAM_SHADER_FRAG,
            .texture  = ctx->font_atlas,
        },
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC3,
            .format   = NGLI_FORMAT_R32G32B32_SFLOAT,
            .stride   = 3 * 4,
            .buffer   = s->vertices,
        },
        {
            .name     = "uvcoord",
            .type     = NGLI_TYPE_VEC2,
            .format   = NGLI_FORMAT_R32G32_SFLOAT,
            .stride   = 2 * 4,
            .buffer   = s->uvcoords,
        },
    };

    /* This controls how the characters blend onto the background */
    struct graphicstate state = rnode->graphicstate;
    state.blend = 1;
    state.blend_src_factor   = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor   = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.blend_src_factor_a = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology       = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state          = state,
            .rt_desc        = rnode->rendertarget_desc,
        }
    };

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/text-fg",
        .vert_base        = text_chars_vert,
        .frag_base        = text_chars_frag,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    int ret = init_subdesc(node, desc, &pipeline_params, &crafter_params);
    if (ret < 0)
        return ret;

    ngli_assert(!strcmp("position", pipeline_params.layout.attribute_descs[0].name));
    ngli_assert(!strcmp("uvcoord", pipeline_params.layout.attribute_descs[1].name));

    return 0;
}

static int text_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct text_priv *s = node->priv_data;

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    ctx->rnode_pos->id = ngli_darray_count(&s->pipeline_descs) - 1;

    memset(desc, 0, sizeof(*desc));

    int ret = bg_prepare(node, &desc->bg);
    if (ret < 0)
        return ret;

    ret = fg_prepare(node, &desc->fg);
    if (ret < 0)
        return ret;

    return 0;
}

static int text_update(struct ngl_node *node, double t)
{
    struct text_priv *s = node->priv_data;

    if (s->live_changed) {
        int ret = update_character_geometries(node);
        if (ret < 0)
            return ret;
        s->live_changed = 0;
    }
    return 0;
}

static void text_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];

    if (!ctx->render_pass_started) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->render_pass_started = 1;
    }

    struct pipeline_subdesc *bg_desc = &desc->bg;
    ngli_pipeline_compat_update_uniform(bg_desc->pipeline_compat, bg_desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(bg_desc->pipeline_compat, bg_desc->projection_matrix_index, projection_matrix);
    ngli_pipeline_compat_update_uniform(bg_desc->pipeline_compat, bg_desc->color_index, o->bg_color);
    ngli_pipeline_compat_update_uniform(bg_desc->pipeline_compat, bg_desc->opacity_index, &o->bg_opacity);
    ngli_pipeline_compat_draw(bg_desc->pipeline_compat, 4, 1);

    if (s->nb_indices) {
        struct pipeline_subdesc *fg_desc = &desc->fg;
        ngli_pipeline_compat_update_uniform(fg_desc->pipeline_compat, fg_desc->modelview_matrix_index, modelview_matrix);
        ngli_pipeline_compat_update_uniform(fg_desc->pipeline_compat, fg_desc->projection_matrix_index, projection_matrix);
        ngli_pipeline_compat_update_uniform(fg_desc->pipeline_compat, fg_desc->color_index, o->fg_color);
        ngli_pipeline_compat_update_uniform(fg_desc->pipeline_compat, fg_desc->opacity_index, &o->fg_opacity);
        ngli_pipeline_compat_draw_indexed(fg_desc->pipeline_compat, s->indices, NGLI_FORMAT_R16_UNORM, (int)s->nb_indices, 1);
    }
}

static void text_uninit(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_compat_freep(&desc->bg.pipeline_compat);
        ngli_pipeline_compat_freep(&desc->fg.pipeline_compat);
        ngli_pgcraft_freep(&desc->bg.crafter);
        ngli_pgcraft_freep(&desc->fg.crafter);
    }
    ngli_darray_reset(&s->pipeline_descs);
    ngli_buffer_freep(&s->bg_vertices);
    destroy_characters_resources(s);
    ngli_text_freep(&s->text_ctx);
}

const struct node_class ngli_text_class = {
    .id             = NGL_NODE_TEXT,
    .category       = NGLI_NODE_CATEGORY_RENDER,
    .name           = "Text",
    .init           = text_init,
    .prepare        = text_prepare,
    .update         = text_update,
    .draw           = text_draw,
    .uninit         = text_uninit,
    .opts_size      = sizeof(struct text_opts),
    .priv_size      = sizeof(struct text_priv),
    .params         = text_params,
    .flags          = NGLI_NODE_FLAG_LIVECTL,
    .livectl_offset = OFFSET(live),
    .file           = __FILE__,
};
