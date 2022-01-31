/*
 * Copyright 2019 GoPro Inc.
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
#include "drawutils.h"
#include "gpu_ctx.h"
#include "log.h"
#include "math_utils.h"
#include "pgcache.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "type.h"
#include "topology.h"
#include "utils.h"


#define VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                            NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT) \

#define INDEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                           NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT)  \

#define DYNAMIC_VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_DYNAMIC_BIT | VERTEX_USAGE_FLAGS)
#define DYNAMIC_INDEX_USAGE_FLAGS  (NGLI_BUFFER_USAGE_DYNAMIC_BIT | INDEX_USAGE_FLAGS)

struct pipeline_subdesc {
    struct pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
    int modelview_matrix_index;
    int projection_matrix_index;
    int color_index;
    int opacity_index;
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
    int padding;
    float font_scale;
    int valign, halign;
    int aspect_ratio[2];
};

struct text_priv {
    struct text_opts opts;
    struct buffer *vertices;
    struct buffer *uvcoords;
    struct buffer *indices;
    int nb_indices;

    struct buffer *bg_vertices;

    struct darray pipeline_descs;
    int live_changed;
};

#define VALIGN_CENTER 0
#define VALIGN_TOP    1
#define VALIGN_BOTTOM 2

#define HALIGN_CENTER 0
#define HALIGN_RIGHT  1
#define HALIGN_LEFT   2

static const struct param_choices valign_choices = {
    .name = "valign",
    .consts = {
        {"center", VALIGN_CENTER, .desc=NGLI_DOCSTRING("vertically centered")},
        {"bottom", VALIGN_BOTTOM, .desc=NGLI_DOCSTRING("bottom positioned")},
        {"top",    VALIGN_TOP,    .desc=NGLI_DOCSTRING("top positioned")},
        {NULL}
    }
};

static const struct param_choices halign_choices = {
    .name = "halign",
    .consts = {
        {"center", HALIGN_CENTER, .desc=NGLI_DOCSTRING("horizontally centered")},
        {"right",  HALIGN_RIGHT,  .desc=NGLI_DOCSTRING("right positioned")},
        {"left",   HALIGN_LEFT,   .desc=NGLI_DOCSTRING("left positioned")},
        {NULL}
    }
};

static int set_live_changed(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    s->live_changed = 1;
    return 0;
}

#define OFFSET(x) offsetof(struct text_priv, opts.x)
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
    {"valign",       NGLI_PARAM_TYPE_SELECT, OFFSET(valign), {.i32=VALIGN_CENTER},
                     .choices=&valign_choices,
                     .desc=NGLI_DOCSTRING("vertical alignment of the text in the box")},
    {"halign",       NGLI_PARAM_TYPE_SELECT, OFFSET(halign), {.i32=HALIGN_CENTER},
                     .choices=&halign_choices,
                     .desc=NGLI_DOCSTRING("horizontal alignment of the text in the box")},
    {"aspect_ratio", NGLI_PARAM_TYPE_RATIONAL, OFFSET(aspect_ratio),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("box aspect ratio")},
    {NULL}
};

static const char * const bg_vertex_data =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    ngl_out_pos = projection_matrix * modelview_matrix * vec4(position, 1.0);"     "\n"
    "}";

static const char * const bg_fragment_data =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    ngl_out_color = vec4(color, 1.0) * opacity;"                                   "\n"
    "}";

static const char * const vertex_data =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    ngl_out_pos = projection_matrix * modelview_matrix * vec4(position, 1.0);"     "\n"
    "    var_tex_coord = uvcoord;"                                                      "\n"
    "}";

static const char * const fragment_data =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    float v = ngl_tex2d(tex, var_tex_coord).r;"                                    "\n"
    "    ngl_out_color = vec4(color, 1.0) * opacity * v;"                               "\n"
    "}";

static const struct pgcraft_iovar vert_out_vars[] = {
    {.name = "var_tex_coord", .type = NGLI_TYPE_VEC2},
};

#define BC(index) o->box_corner[index]
#define BW(index) o->box_width[index]
#define BH(index) o->box_height[index]

#define C(index) chr_corner[index]
#define W(index) chr_width[index]
#define H(index) chr_height[index]

static void get_char_box_dim(const char *s, int *wp, int *hp, int *np)
{
    int w = 0, h = 1;
    int cur_w = 0;
    int n = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') {
            cur_w = 0;
            h++;
        } else {
            cur_w++;
            w = NGLI_MAX(w, cur_w);
            n++;
        }
    }
    *wp = w;
    *hp = h;
    *np = n;
}

static int update_character_geometries(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;
    struct text_opts *o = &s->opts;

    int ret = 0;
    const char *str = o->live.val.s;

    int text_cols, text_rows, text_nbchr;
    get_char_box_dim(str, &text_cols, &text_rows, &text_nbchr);
    if (!text_nbchr) {
        ngli_buffer_freep(&s->vertices);
        ngli_buffer_freep(&s->uvcoords);
        ngli_buffer_freep(&s->indices);
        s->nb_indices = 0;
        return 0;
    }

    const int nb_vertices = text_nbchr * 4 * 3;
    const int nb_uvcoords = text_nbchr * 4 * 2;
    const int nb_indices  = text_nbchr * 6;
    float *vertices = ngli_calloc(nb_vertices, sizeof(*vertices));
    float *uvcoords = ngli_calloc(nb_uvcoords, sizeof(*uvcoords));
    short *indices  = ngli_calloc(nb_indices, sizeof(*indices));
    if (!vertices || !uvcoords || !indices) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    /* Text/Box ratio */
    const float box_width_len  = ngli_vec3_length(o->box_width);
    const float box_height_len = ngli_vec3_length(o->box_height);
    static const int default_ar[2] = {1, 1};
    const int *ar = o->aspect_ratio[1] ? o->aspect_ratio : default_ar;
    const float box_ratio = ar[0] * box_width_len / (float)(ar[1] * box_height_len);

    const int text_width   = text_cols * NGLI_FONT_W + 2 * o->padding;
    const int text_height  = text_rows * NGLI_FONT_H + 2 * o->padding;
    const float text_ratio = text_width / (float)(text_height);

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

    /* User padding */
    float padw[3];
    float padh[3];
    ngli_vec3_scale(padw, width,  o->padding / (float)text_width);
    ngli_vec3_scale(padh, height, o->padding / (float)text_height);

    /* Width and height of 1 character */
    const float chr_width[3] = {
        (width[0] - 2 * padw[0]) / (float)text_cols,
        (width[1] - 2 * padw[1]) / (float)text_cols,
        (width[2] - 2 * padw[2]) / (float)text_cols,
    };
    const float chr_height[3] = {
        (height[0] - 2 * padh[0]) / (float)text_rows,
        (height[1] - 2 * padh[1]) / (float)text_rows,
        (height[2] - 2 * padh[2]) / (float)text_rows,
    };

    /* Adjust text position according to alignment settings */
    float align_padw[3];
    float align_padh[3];
    ngli_vec3_sub(align_padw, o->box_width,  width);
    ngli_vec3_sub(align_padh, o->box_height, height);

    const float spx = (o->halign == HALIGN_CENTER ? .5f :
                       o->halign == HALIGN_RIGHT  ? 1.f :
                       0.f);
    const float spy = (o->valign == VALIGN_CENTER ? .5f :
                       o->valign == VALIGN_TOP    ? 1.f :
                       0.f);

    float corner[3] = {
        BC(0) + align_padw[0] * spx + align_padh[0] * spy + padw[0] + padh[0],
        BC(1) + align_padw[1] * spx + align_padh[1] * spy + padw[1] + padh[1],
        BC(2) + align_padw[2] * spx + align_padh[2] * spy + padw[2] + padh[2],
    };

    int px = 0, py = 0;
    int n = 0;

    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            py++;
            px = 0;
            continue;
        }

        /* quad vertices */
        const float chr_corner[3] = {
            corner[0] + chr_width[0] * px + chr_height[0] * (text_rows - py - 1),
            corner[1] + chr_width[1] * px + chr_height[1] * (text_rows - py - 1),
            corner[2] + chr_width[2] * px + chr_height[2] * (text_rows - py - 1),
        };
        const float chr_vertices[] = {
            C(0),               C(1),               C(2),
            C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
            C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
            C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
        };
        memcpy(vertices + 4 * 3 * n, chr_vertices, sizeof(chr_vertices));

        /* focus uvcoords on the character in the atlas texture */
        ngli_drawutils_get_atlas_uvcoords(str[i], uvcoords + 4 * 2 * n);

        /* quad for each character is made of 2 triangles */
        const short chr_indices[] = { n*4 + 0, n*4 + 1, n*4 + 2, n*4 + 0, n*4 + 2, n*4 + 3 };
        memcpy(indices + n * NGLI_ARRAY_NB(chr_indices), chr_indices, sizeof(chr_indices));

        n++;
        px++;
    }

    if (nb_indices > s->nb_indices) { // need re-alloc
        ngli_buffer_freep(&s->vertices);
        ngli_buffer_freep(&s->uvcoords);
        ngli_buffer_freep(&s->indices);

        s->vertices = ngli_buffer_create(gpu_ctx);
        s->uvcoords = ngli_buffer_create(gpu_ctx);
        s->indices  = ngli_buffer_create(gpu_ctx);
        if (!s->vertices || !s->uvcoords || !s->indices) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }

        if ((ret = ngli_buffer_init(s->vertices, nb_vertices * sizeof(*vertices), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->uvcoords, nb_uvcoords * sizeof(*uvcoords), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->indices,  nb_indices  * sizeof(*indices),  DYNAMIC_INDEX_USAGE_FLAGS)) < 0)
            goto end;

        struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
        const int nb_descs = ngli_darray_count(&s->pipeline_descs);
        for (int i = 0; i < nb_descs; i++) {
            struct pipeline_subdesc *desc = &descs[i].fg;

            ngli_pipeline_compat_update_attribute(desc->pipeline_compat, 0, s->vertices);
            ngli_pipeline_compat_update_attribute(desc->pipeline_compat, 1, s->uvcoords);
        }
    }

    if ((ret = ngli_buffer_upload(s->vertices, vertices, nb_vertices * sizeof(*vertices), 0)) < 0 ||
        (ret = ngli_buffer_upload(s->uvcoords, uvcoords, nb_uvcoords * sizeof(*uvcoords), 0)) < 0 ||
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
    const struct text_opts *o = &s->opts;

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

static int atlas_create(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    if (ctx->font_atlas)
        return 0;

    struct canvas canvas = {0};
    int ret = ngli_drawutils_get_font_atlas(&canvas);
    if (ret < 0)
        goto end;

    struct texture_params tex_params = {
        .type          = NGLI_TEXTURE_TYPE_2D,
        .width         = canvas.w,
        .height        = canvas.h,
        .format        = NGLI_FORMAT_R8_UNORM,
        .min_filter    = NGLI_FILTER_LINEAR,
        .mag_filter    = NGLI_FILTER_NEAREST,
        .mipmap_filter = NGLI_MIPMAP_FILTER_LINEAR,
        .usage         = NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT
                       | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT
                       | NGLI_TEXTURE_USAGE_SAMPLED_BIT,
    };

    ctx->font_atlas = ngli_texture_create(gpu_ctx); // freed at context reconfiguration/destruction
    if (!ctx->font_atlas) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    ret = ngli_texture_init(ctx->font_atlas, &tex_params);
    if (ret < 0)
        goto end;

    ret = ngli_texture_upload(ctx->font_atlas, canvas.buf, 0);
    if (ret < 0)
        goto end;

end:
    ngli_free(canvas.buf);
    return ret;
}

static int text_init(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;

    int ret = atlas_create(node);
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
    const struct text_opts *o = &s->opts;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "color",             .type = NGLI_TYPE_VEC3, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = o->bg_color},
        {.name = "opacity",           .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = &o->bg_opacity},
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
        .vert_base        = bg_vertex_data,
        .frag_base        = bg_fragment_data,
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
    const struct text_opts *o = &s->opts;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "color",             .type = NGLI_TYPE_VEC3, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = o->fg_color},
        {.name = "opacity",           .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = &o->fg_opacity},
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
        .vert_base        = vertex_data,
        .frag_base        = fragment_data,
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

    ngli_assert(!strcmp("position", pipeline_params.layout.attributes_desc[0].name));
    ngli_assert(!strcmp("uvcoord", pipeline_params.layout.attributes_desc[1].name));

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
    const struct text_opts *o = &s->opts;

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
        ngli_pipeline_compat_draw_indexed(fg_desc->pipeline_compat, s->indices, NGLI_FORMAT_R16_UNORM, s->nb_indices, 1);
    }
}

static void text_uninit(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const int nb_descs = ngli_darray_count(&s->pipeline_descs);
    for (int i = 0; i < nb_descs; i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_compat_freep(&desc->bg.pipeline_compat);
        ngli_pipeline_compat_freep(&desc->fg.pipeline_compat);
        ngli_pgcraft_freep(&desc->bg.crafter);
        ngli_pgcraft_freep(&desc->fg.crafter);
    }
    ngli_darray_reset(&s->pipeline_descs);
    ngli_buffer_freep(&s->bg_vertices);
    ngli_buffer_freep(&s->vertices);
    ngli_buffer_freep(&s->uvcoords);
    ngli_buffer_freep(&s->indices);
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
    .priv_size      = sizeof(struct text_priv),
    .params         = text_params,
    .flags          = NGLI_NODE_FLAG_LIVECTL,
    .livectl_offset = OFFSET(live),
    .file           = __FILE__,
};
