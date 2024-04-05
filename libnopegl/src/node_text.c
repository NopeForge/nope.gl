/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
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

struct pipeline_desc_common {
    struct pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
    int32_t modelview_matrix_index;
    int32_t projection_matrix_index;
};

struct pipeline_desc_bg {
    struct pipeline_desc_common common;
    int32_t color_index;
    int32_t opacity_index;
};

struct pipeline_desc_fg {
    struct pipeline_desc_common common;
    int32_t transform_index;
    int32_t atlas_coords_index;
    int32_t user_transform_index;
    int32_t color_index;
    int32_t outline_index;
    int32_t glow_index;
    int32_t blur_index;
};

struct pipeline_desc {
    struct pipeline_desc_bg bg; /* Background (bounding box) */
    struct pipeline_desc_fg fg; /* Foreground (characters) */
};

struct text_opts {
    struct livectl live;
    float fg_color[3];
    float fg_opacity;
    float bg_color[3];
    float bg_opacity;
    float box[4];
    struct ngl_node **font_faces;
    size_t nb_font_faces;
    int32_t padding;
    int32_t pt_size;
    int32_t dpi;
    float font_scale;
    int scale_mode;
    struct ngl_node **effect_nodes;
    size_t nb_effect_nodes;
    int valign, halign;
    int writing_mode;
};

struct text_priv {
    /* characters */
    struct text *text_ctx;
    struct buffer *transforms;
    struct buffer *atlas_coords;
    struct buffer *user_transforms;
    struct buffer *colors;
    struct buffer *outlines;
    struct buffer *glows;
    struct buffer *blurs;
    size_t nb_chars;

    /* background box */
    struct buffer *bg_vertices;

    struct darray pipeline_descs;
    int live_changed;
    struct viewport viewport;
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

static const struct param_choices writing_mode_choices = {
    .name = "writing_mode",
    .consts = {
        {"horizontal-tb", NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB,
                          .desc=NGLI_DOCSTRING("left-to-right flow then top-to-bottom per line")},
        {"vertical-rl",   NGLI_TEXT_WRITING_MODE_VERTICAL_RL,
                          .desc=NGLI_DOCSTRING("top-to-bottom flow then right-to-left per line")},
        {"vertical-lr",   NGLI_TEXT_WRITING_MODE_VERTICAL_LR,
                          .desc=NGLI_DOCSTRING("top-to-bottom flow then left-to-right per line")},
        {NULL}
    }
};

#define SCALE_MODE_AUTO  0
#define SCALE_MODE_FIXED 1

static const struct param_choices scale_mode_choices = {
    .name = "scale_mode",
    .consts = {
        {"auto",  SCALE_MODE_AUTO,  .desc=NGLI_DOCSTRING("automatic size by fitting the specified bounding box")},
        {"fixed", SCALE_MODE_FIXED, .desc=NGLI_DOCSTRING("fixed character size (bounding box ignored for scaling)")},
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
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("foreground text color")},
    {"fg_opacity",   NGLI_PARAM_TYPE_F32, OFFSET(fg_opacity), {.f32=1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("foreground text opacity")},
    {"bg_color",     NGLI_PARAM_TYPE_VEC3, OFFSET(bg_color), {.vec={0.0, 0.0, 0.0}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("background text color")},
    {"bg_opacity",   NGLI_PARAM_TYPE_F32, OFFSET(bg_opacity), {.f32=.8f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("background text opacity")},
    {"box",          NGLI_PARAM_TYPE_VEC4, OFFSET(box), {.vec={-1.f, -1.f, 2.f, 2.f}},
                     .desc=NGLI_DOCSTRING("geometry box relative to screen (x, y, width, height)")},
    {"font_faces",   NGLI_PARAM_TYPE_NODELIST, OFFSET(font_faces),
                     .node_types=(const uint32_t[]){NGL_NODE_FONTFACE, NGLI_NODE_NONE},
                     .desc=NGLI_DOCSTRING("font faces in order of preferences (require build with external text libraries)")},
    {"padding",      NGLI_PARAM_TYPE_I32, OFFSET(padding), {.i32=4},
                     .desc=NGLI_DOCSTRING("padding around the text, in point units")},
    {"pt_size",      NGLI_PARAM_TYPE_I32, OFFSET(pt_size), {.i32=54},
                     .desc=NGLI_DOCSTRING("characters size in point (nominal size, 1pt = 1/72 inch)")},
    {"dpi",          NGLI_PARAM_TYPE_I32, OFFSET(dpi), {.i32=96},
                     .desc=NGLI_DOCSTRING("resolution (dot per inch)")},
    {"font_scale",   NGLI_PARAM_TYPE_F32, OFFSET(font_scale), {.f32=1.f},
                     .desc=NGLI_DOCSTRING("scaling of the font")},
    {"scale_mode",   NGLI_PARAM_TYPE_SELECT, OFFSET(scale_mode), {.i32=SCALE_MODE_AUTO},
                     .choices=&scale_mode_choices,
                     .desc=NGLI_DOCSTRING("scaling behaviour for the characters")},
    {"effects",      NGLI_PARAM_TYPE_NODELIST, OFFSET(effect_nodes),
                     .node_types=(const uint32_t[]){NGL_NODE_TEXTEFFECT, NGLI_NODE_NONE},
                     .desc=NGLI_DOCSTRING("stack of effects")},
    {"valign",       NGLI_PARAM_TYPE_SELECT, OFFSET(valign), {.i32=NGLI_TEXT_VALIGN_CENTER},
                     .choices=&valign_choices,
                     .desc=NGLI_DOCSTRING("vertical alignment of the text in the box")},
    {"halign",       NGLI_PARAM_TYPE_SELECT, OFFSET(halign), {.i32=NGLI_TEXT_HALIGN_CENTER},
                     .choices=&halign_choices,
                     .desc=NGLI_DOCSTRING("horizontal alignment of the text in the box")},
    {"writing_mode", NGLI_PARAM_TYPE_SELECT, OFFSET(writing_mode), {.i32=NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB},
                     .choices=&writing_mode_choices,
                     .desc=NGLI_DOCSTRING("direction flow per character and line")},
    {NULL}
};

static void destroy_characters_resources(struct text_priv *s)
{
    ngli_buffer_freep(&s->transforms);
    ngli_buffer_freep(&s->atlas_coords);
    ngli_buffer_freep(&s->user_transforms);
    ngli_buffer_freep(&s->colors);
    ngli_buffer_freep(&s->outlines);
    ngli_buffer_freep(&s->glows);
    ngli_buffer_freep(&s->blurs);
    s->nb_chars = 0;
}

static int refresh_geometry(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;
    struct text_opts *o = node->opts;

    struct text *text = s->text_ctx;

    const size_t text_nbchr = ngli_darray_count(&text->chars);
    if (!text_nbchr) {
        destroy_characters_resources(s);
        return 0;
    }

    float *transforms = ngli_calloc(text_nbchr, 4 * sizeof(*transforms));
    float *atlas_coords = ngli_calloc(text_nbchr, 4 * sizeof(*atlas_coords));
    if (!transforms || !atlas_coords) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    /* Text/Box ratio */
    const struct ngli_box box = {NGLI_ARG_VEC4(o->box)};
    const struct viewport viewport = ngli_gpu_ctx_get_viewport(ctx->gpu_ctx);
    const int32_t ar[] = {viewport.width, viewport.height};
    const float box_ratio = (float)ar[0] * box.w / ((float)ar[1] * box.h);
    const float text_ratio = (float)text->width / (float)text->height;

    /* Apply aspect ratio and font scaling */
    float width  = box.w * o->font_scale;
    float height = box.h * o->font_scale;
    float ratio_w, ratio_h;
    if (o->scale_mode == SCALE_MODE_FIXED) {
        const float tw = (float)text->width / (float)s->viewport.width;
        const float th = (float)text->height / (float)s->viewport.height;
        ratio_w = tw / box.w;
        ratio_h = th / box.h;
    } else {
        if (text_ratio < box_ratio) {
            ratio_w = text_ratio / box_ratio;
            ratio_h = 1.0;
        } else {
            ratio_w = 1.0;
            ratio_h = box_ratio / text_ratio;
        }
    }
    width  *= ratio_w;
    height *= ratio_h;

    /* Adjust text position according to alignment settings */
    const float align_padw = box.w - width;
    const float align_padh = box.h - height;

    const float spx = (o->halign == NGLI_TEXT_HALIGN_CENTER ? .5f :
                       o->halign == NGLI_TEXT_HALIGN_RIGHT  ? 1.f :
                       0.f);
    const float spy = (o->valign == NGLI_TEXT_VALIGN_CENTER ? .5f :
                       o->valign == NGLI_TEXT_VALIGN_TOP    ? 1.f :
                       0.f);

    const float corner_x = box.x + align_padw * spx;
    const float corner_y = box.y + align_padh * spy;

    const struct char_info *chars = ngli_darray_data(&text->chars);
    for (size_t n = 0; n < text_nbchr; n++) {
        const struct char_info *chr = &chars[n];

        /* character dimension and position */
        const float chr_width  = width  * chr->geom.w;
        const float chr_height = height * chr->geom.h;
        const float chr_corner_x = corner_x + width  * chr->geom.x;
        const float chr_corner_y = corner_y + height * chr->geom.y;
        const float transform[] = {chr_corner_x, chr_corner_y, chr_width, chr_height};
        memcpy(transforms + 4 * n, transform, sizeof(transform));

        /* register atlas identifier */
        memcpy(atlas_coords + 4 * n, chr->atlas_coords, sizeof(chr->atlas_coords));
    }

    if (text_nbchr > s->nb_chars) { // need re-alloc
        destroy_characters_resources(s);

        /* The content of these buffers will remain constant until the next text content update */
        s->transforms = ngli_buffer_create(gpu_ctx);
        s->atlas_coords = ngli_buffer_create(gpu_ctx);
        if (!s->transforms || !s->atlas_coords) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }

        /* The content of these buffers will be updated later using the effects data (see apply_effects()) */
        s->user_transforms = ngli_buffer_create(gpu_ctx);
        s->colors          = ngli_buffer_create(gpu_ctx);
        s->outlines        = ngli_buffer_create(gpu_ctx);
        s->glows           = ngli_buffer_create(gpu_ctx);
        s->blurs           = ngli_buffer_create(gpu_ctx);
        if (!s->user_transforms || !s->colors || !s->outlines || !s->glows  || !s->blurs) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }

        if ((ret = ngli_buffer_init(s->transforms,      text_nbchr     * 4 * sizeof(*transforms),   DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->atlas_coords,    text_nbchr     * 4 * sizeof(*atlas_coords), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->user_transforms, text_nbchr * 4 * 4 * sizeof(float),         DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->colors,          text_nbchr     * 4 * sizeof(float),         DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->outlines,        text_nbchr     * 4 * sizeof(float),         DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->glows,           text_nbchr     * 4 * sizeof(float),         DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->blurs,           text_nbchr         * sizeof(float),         DYNAMIC_VERTEX_USAGE_FLAGS)) < 0)
            goto end;

        struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
        for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
            struct pipeline_desc_fg *desc_fg = &descs[i].fg;
            struct pipeline_desc_common *desc = &desc_fg->common;

            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->transform_index, s->transforms);
            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->atlas_coords_index, s->atlas_coords);
            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->user_transform_index, s->user_transforms);
            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->color_index, s->colors);
            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->outline_index, s->outlines);
            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->glow_index, s->glows);
            ngli_pipeline_compat_update_vertex_buffer(desc->pipeline_compat, desc_fg->blur_index, s->blurs);
        }
    }

    if (text->cls->flags & NGLI_TEXT_FLAG_MUTABLE_ATLAS) {
        struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
        for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
            struct pipeline_desc_fg *desc_fg = &descs[i].fg;
            struct pipeline_desc_common *desc = &desc_fg->common;
            int ret = ngli_pipeline_compat_update_texture(desc->pipeline_compat, 0, text->atlas_texture);
            if (ret < 0)
                goto end;
        }
    }

    if ((ret = ngli_buffer_upload(s->transforms, transforms, 0, text_nbchr * 4 * sizeof(*transforms))) < 0 ||
        (ret = ngli_buffer_upload(s->atlas_coords, atlas_coords, 0, text_nbchr * 4 * sizeof(*atlas_coords))) < 0)
        goto end;

    s->nb_chars = text_nbchr;

end:
    ngli_free(transforms);
    ngli_free(atlas_coords);
    return ret;
}

static int update_text_content(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    int ret = ngli_text_set_string(s->text_ctx, o->live.val.s);
    if (ret < 0)
        return ret;

    return refresh_geometry(node);
}

/* Update the GPU buffers using the updated effects data */
static int apply_effects(struct text_priv *s)
{
    int ret;
    struct text *text = s->text_ctx;

    const size_t text_nbchr = ngli_darray_count(&text->chars);
    if (!text_nbchr)
        return 0;

    const struct text_effects_pointers *ptrs = &text->data_ptrs;
    if ((ret = ngli_buffer_upload(s->user_transforms, ptrs->transform,  0, text_nbchr * 4 * 4 * sizeof(*ptrs->transform))) < 0 ||
        (ret = ngli_buffer_upload(s->colors,          ptrs->color,      0, text_nbchr     * 4 * sizeof(*ptrs->color)))     < 0 ||
        (ret = ngli_buffer_upload(s->outlines,        ptrs->outline,    0, text_nbchr     * 4 * sizeof(*ptrs->outline)))   < 0 ||
        (ret = ngli_buffer_upload(s->glows,           ptrs->glow,       0, text_nbchr     * 4 * sizeof(*ptrs->glow)))      < 0 ||
        (ret = ngli_buffer_upload(s->blurs,           ptrs->blur,       0, text_nbchr         * sizeof(*ptrs->blur)))      < 0)
        return ret;

    return 0;
}

static int init_bounding_box_geometry(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    const struct ngli_box box = {NGLI_ARG_VEC4(o->box)};
    const float vertices[] = {
        box.x,         box.y,
        box.x + box.w, box.y,
        box.x,         box.y + box.h,
        box.x + box.w, box.y + box.h,
    };

    s->bg_vertices = ngli_buffer_create(gpu_ctx);
    if (!s->bg_vertices)
        return NGL_ERROR_MEMORY;

    int ret;
    if ((ret = ngli_buffer_init(s->bg_vertices, sizeof(vertices), VERTEX_USAGE_FLAGS)) < 0 ||
        (ret = ngli_buffer_upload(s->bg_vertices, vertices, 0, sizeof(vertices))) < 0)
        return ret;

    return 0;
}

static int text_init(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    const struct text_opts *o = node->opts;

    s->viewport = ngli_gpu_ctx_get_viewport(node->ctx->gpu_ctx);

    s->text_ctx = ngli_text_create(node->ctx);
    if (!s->text_ctx)
        return NGL_ERROR_MEMORY;

    const struct text_config config = {
        .font_faces = o->font_faces,
        .nb_font_faces = o->nb_font_faces,
        .pt_size = o->pt_size,
        .dpi = o->dpi,
        .padding = o->padding,
        .valign = o->valign,
        .halign = o->halign,
        .writing_mode = o->writing_mode,
        .effect_nodes = o->effect_nodes,
        .nb_effect_nodes = o->nb_effect_nodes,
        .defaults = {
            .color = {NGLI_ARG_VEC3(o->fg_color)},
            .opacity = o->fg_opacity,
        }
    };

    int ret = ngli_text_init(s->text_ctx, &config);
    if (ret < 0)
        return ret;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    ret = init_bounding_box_geometry(node);
    if (ret < 0)
        return ret;

    ret = update_text_content(node);
    if (ret < 0)
        return ret;

    return 0;
}

static int init_subdesc(struct ngl_node *node,
                        struct pipeline_desc_common *desc,
                        const struct graphics_state *graphics_state,
                        const struct pgcraft_params *crafter_params)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rnode *rnode = ctx->rnode_pos;

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    int ret = ngli_pgcraft_craft(desc->crafter, crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!desc->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology     = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state        = *graphics_state,
            .rt_layout    = rnode->rendertarget_layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(desc->crafter),
        },
        .program     = ngli_pgcraft_get_program(desc->crafter),
        .layout      = ngli_pgcraft_get_pipeline_layout(desc->crafter),
        .resources   = ngli_pgcraft_get_pipeline_resources(desc->crafter),
        .compat_info = ngli_pgcraft_get_compat_info(desc->crafter),
    };

    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);

    return 0;
}

static int bg_prepare(struct ngl_node *node, struct pipeline_desc_bg *desc)
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
            .type     = NGLI_TYPE_VEC2,
            .format   = NGLI_FORMAT_R32G32_SFLOAT,
            .stride   = 2 * sizeof(float),
            .buffer   = s->bg_vertices,
        },
    };

    /* This controls how the background blends onto the current framebuffer */
    struct graphics_state state = rnode->graphics_state;
    state.blend = 1;
    state.blend_src_factor   = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor   = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.blend_src_factor_a = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/text-bg",
        .vert_base        = text_bg_vert,
        .frag_base        = text_bg_frag,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
    };

    int ret = init_subdesc(node, &desc->common, &state, &crafter_params);
    if (ret < 0)
        return ret;

    desc->color_index = ngli_pgcraft_get_uniform_index(desc->common.crafter, "color", NGLI_PROGRAM_SHADER_FRAG);
    desc->opacity_index = ngli_pgcraft_get_uniform_index(desc->common.crafter, "opacity", NGLI_PROGRAM_SHADER_FRAG);

    return 0;
}

static int fg_prepare(struct ngl_node *node, struct pipeline_desc_fg *desc)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct text_priv *s = node->priv_data;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
    };

    const struct pgcraft_texture textures[] = {
        {
            .name     = "tex",
            .type     = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage    = NGLI_PROGRAM_SHADER_FRAG,
            .texture  = s->text_ctx->atlas_texture,
        },
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "transform",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * sizeof(float),
            .buffer   = s->transforms,
            .rate     = 1,
        }, {
            .name     = "atlas_coords",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * sizeof(float),
            .buffer   = s->atlas_coords,
            .rate     = 1,
        }, {
            .name     = "user_transform",
            .type     = NGLI_TYPE_MAT4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * 4 * sizeof(float),
            .buffer   = s->user_transforms,
            .rate     = 1,
        }, {
            .name     = "frag_color",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * sizeof(float),
            .buffer   = s->colors,
            .rate     = 1,
        }, {
            .name     = "frag_outline",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * sizeof(float),
            .buffer   = s->outlines,
            .rate     = 1,
        }, {
            .name     = "frag_glow",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * sizeof(float),
            .buffer   = s->glows,
            .rate     = 1,
        }, {
            .name     = "frag_blur",
            .type     = NGLI_TYPE_F32,
            .format   = NGLI_FORMAT_R32_SFLOAT,
            .stride   = sizeof(float),
            .buffer   = s->blurs,
            .rate     = 1,
        },
    };

    /* This controls how the characters blend onto the background */
    struct graphics_state state = rnode->graphics_state;
    state.blend = 1;
    state.blend_src_factor   = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor   = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.blend_src_factor_a = NGLI_BLEND_FACTOR_ONE;
    state.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "uv",     .type = NGLI_TYPE_VEC2},
        {.name = "coords", .type = NGLI_TYPE_VEC4},
        {.name = "color",  .type = NGLI_TYPE_VEC4},
        {.name = "outline",.type = NGLI_TYPE_VEC4},
        {.name = "glow",   .type = NGLI_TYPE_VEC4},
        {.name = "blur",   .type = NGLI_TYPE_F32},
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

    int ret = init_subdesc(node, &desc->common, &state, &crafter_params);
    if (ret < 0)
        return ret;

    desc->transform_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "transform");
    desc->atlas_coords_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "atlas_coords");
    desc->user_transform_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "user_transform");
    desc->color_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "frag_color");
    desc->outline_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "frag_outline");
    desc->glow_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "frag_glow");
    desc->blur_index = ngli_pgcraft_get_vertex_buffer_index(desc->common.crafter, "frag_blur");

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
    const struct text_opts *o = node->opts;

    if (s->live_changed) {
        const struct text_effects_defaults defaults = {
            .color = {NGLI_ARG_VEC3(o->fg_color)},
            .opacity = o->fg_opacity,
        };
        ngli_text_update_effects_defaults(s->text_ctx, &defaults);

        int ret = update_text_content(node);
        if (ret < 0)
            return ret;
        s->live_changed = 0;
    }

    const struct viewport viewport = ngli_gpu_ctx_get_viewport(node->ctx->gpu_ctx);
    if (memcmp(&s->viewport, &viewport, sizeof(viewport))) {
        memcpy(&s->viewport, &viewport, sizeof(viewport));
        int ret = refresh_geometry(node);
        if (ret < 0)
            return ret;
    }

    int ret = ngli_text_set_time(s->text_ctx, t);
    if (ret < 0)
        return ret;

    ret = apply_effects(s);
    if (ret < 0)
        return ret;

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

    struct pipeline_desc_bg *bg_desc = &desc->bg;
    ngli_pipeline_compat_update_uniform(bg_desc->common.pipeline_compat, bg_desc->common.modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(bg_desc->common.pipeline_compat, bg_desc->common.projection_matrix_index, projection_matrix);
    ngli_pipeline_compat_update_uniform(bg_desc->common.pipeline_compat, bg_desc->color_index, o->bg_color);
    ngli_pipeline_compat_update_uniform(bg_desc->common.pipeline_compat, bg_desc->opacity_index, &o->bg_opacity);
    ngli_pipeline_compat_draw(bg_desc->common.pipeline_compat, 4, 1);

    if (s->nb_chars) {
        struct pipeline_desc_fg *fg_desc = &desc->fg;
        ngli_pipeline_compat_update_uniform(fg_desc->common.pipeline_compat, fg_desc->common.modelview_matrix_index, modelview_matrix);
        ngli_pipeline_compat_update_uniform(fg_desc->common.pipeline_compat, fg_desc->common.projection_matrix_index, projection_matrix);
        ngli_pipeline_compat_draw(fg_desc->common.pipeline_compat, 4, (int)s->nb_chars);
    }
}

static void text_uninit(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_compat_freep(&desc->bg.common.pipeline_compat);
        ngli_pipeline_compat_freep(&desc->fg.common.pipeline_compat);
        ngli_pgcraft_freep(&desc->bg.common.crafter);
        ngli_pgcraft_freep(&desc->fg.common.crafter);
    }
    ngli_darray_reset(&s->pipeline_descs);
    ngli_buffer_freep(&s->bg_vertices);
    destroy_characters_resources(s);
    ngli_text_freep(&s->text_ctx);
}

const struct node_class ngli_text_class = {
    .id             = NGL_NODE_TEXT,
    .category       = NGLI_NODE_CATEGORY_DRAW,
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
