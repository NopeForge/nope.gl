/*
 * Copyright 2021 GoPro Inc.
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

#include "blending.h"
#include "darray.h"
#include "filterschain.h"
#include "geometry.h"
#include "gpu_ctx.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "topology.h"
#include "type.h"
#include "utils.h"

/* GLSL fragments as string */
#include "source_color_frag.h"
#include "source_color_vert.h"
#include "source_gradient_frag.h"
#include "source_gradient_vert.h"
#include "source_gradient4_frag.h"
#include "source_gradient4_vert.h"
#include "source_texture_frag.h"
#include "source_texture_vert.h"

#define VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                            NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT) \

#define GEOMETRY_TYPES_LIST (const int[]){NGL_NODE_CIRCLE,          \
                                          NGL_NODE_GEOMETRY,        \
                                          NGL_NODE_QUAD,            \
                                          NGL_NODE_TRIANGLE,        \
                                          -1}

#define FILTERS_TYPES_LIST (const int[]){NGL_NODE_FILTERALPHA,          \
                                         NGL_NODE_FILTERCONTRAST,       \
                                         NGL_NODE_FILTEREXPOSURE,       \
                                         NGL_NODE_FILTERINVERSEALPHA,   \
                                         NGL_NODE_FILTERLINEAR2SRGB,    \
                                         NGL_NODE_FILTEROPACITY,        \
                                         NGL_NODE_FILTERPREMULT,        \
                                         NGL_NODE_FILTERSATURATION,     \
                                         NGL_NODE_FILTERSRGB2LINEAR,    \
                                         -1}

struct uniform_map {
    int index;
    const void *data;
};

struct pipeline_desc {
    struct pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
    int modelview_matrix_index;
    int projection_matrix_index;
    int aspect_index;
    struct darray uniforms_map; // struct uniform_map
    struct darray uniforms; // struct pgcraft_uniform
};

struct render_common_opts {
    int blending;
    struct ngl_node *geometry;
    struct ngl_node **filters;
    int nb_filters;
};

struct render_common {
    uint32_t helpers;
    void (*draw)(struct render_common *s, const struct render_common_opts *o, struct pipeline_compat *pl_compat);
    struct filterschain *filterschain;
    char *combined_fragment;
    struct pgcraft_attribute position_attr;
    struct pgcraft_attribute uvcoord_attr;
    struct buffer *vertices;
    struct buffer *uvcoords;
    int nb_vertices;
    int topology;
    struct darray pipeline_descs;
};

struct rendercolor_opts {
    struct ngl_node *color_node;
    float color[3];
    struct ngl_node *opacity_node;
    float opacity;
    struct render_common_opts common;
};

struct rendercolor_priv {
    struct render_common common;
};

struct rendergradient_opts {
    struct ngl_node *color0_node;
    float color0[3];
    struct ngl_node *color1_node;
    float color1[3];
    struct ngl_node *opacity0_node;
    float opacity0;
    struct ngl_node *opacity1_node;
    float opacity1;
    struct ngl_node *pos0_node;
    float pos0[2];
    struct ngl_node *pos1_node;
    float pos1[2];
    int mode;
    struct ngl_node *linear_node;
    int linear;
    struct render_common_opts common;
};

struct rendergradient_priv {
    struct render_common common;
};

struct rendergradient4_opts {
    struct ngl_node *color_tl_node;
    float color_tl[3];
    struct ngl_node *color_tr_node;
    float color_tr[3];
    struct ngl_node *color_br_node;
    float color_br[3];
    struct ngl_node *color_bl_node;
    float color_bl[3];
    struct ngl_node *opacity_tl_node;
    float opacity_tl;
    struct ngl_node *opacity_tr_node;
    float opacity_tr;
    struct ngl_node *opacity_br_node;
    float opacity_br;
    struct ngl_node *opacity_bl_node;
    float opacity_bl;
    struct ngl_node *linear_node;
    int linear;
    struct render_common_opts common;
};

struct rendergradient4_priv {
    struct render_common common;
};

struct rendertexture_opts {
    struct ngl_node *texture_node;
    struct render_common_opts common;
};

struct rendertexture_priv {
    struct render_common common;
};

#define OFFSET(x) offsetof(struct rendercolor_opts, x)
static const struct node_param rendercolor_params[] = {
    {"color",    NGLI_PARAM_TYPE_VEC3, OFFSET(color_node), {.vec={1.f, 1.f, 1.f}},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("color of the shape")},
    {"opacity",  NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=1.f},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("opacity of the color")},
    {"blending", NGLI_PARAM_TYPE_SELECT, OFFSET(common.blending),
                 .choices=&ngli_blending_choices,
                 .desc=NGLI_DOCSTRING("define how this node and the current frame buffer are blending together")},
    {"geometry", NGLI_PARAM_TYPE_NODE, OFFSET(common.geometry),
                 .node_types=GEOMETRY_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("geometry to be rasterized")},
    {"filters",  NGLI_PARAM_TYPE_NODELIST, OFFSET(common.filters),
                 .node_types=FILTERS_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("filter chain to apply on top of this source")},
    {NULL}
};
#undef OFFSET

#define GRADIENT_MODE_RAMP   0
#define GRADIENT_MODE_RADIAL 1

static const struct param_choices gradient_mode_choices = {
    .name = "gradient_mode",
    .consts = {
        {"ramp",   GRADIENT_MODE_RAMP,   .desc=NGLI_DOCSTRING("straight line gradient, uniform perpendicularly to the line between the points")},
        {"radial", GRADIENT_MODE_RADIAL, .desc=NGLI_DOCSTRING("distance between the points spread circularly")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct rendergradient_opts, x)
static const struct node_param rendergradient_params[] = {
    {"color0",   NGLI_PARAM_TYPE_VEC3, OFFSET(color0_node), {.vec={0.f, 0.f, 0.f}},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("color of the first point")},
    {"color1",   NGLI_PARAM_TYPE_VEC3, OFFSET(color1_node), {.vec={1.f, 1.f, 1.f}},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("color of the second point")},
    {"opacity0", NGLI_PARAM_TYPE_F32, OFFSET(opacity0_node), {.f32=1.f},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("opacity of the first color")},
    {"opacity1", NGLI_PARAM_TYPE_F32, OFFSET(opacity1_node), {.f32=1.f},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("opacity of the second color")},
    {"pos0",     NGLI_PARAM_TYPE_VEC2, OFFSET(pos0_node), {.vec={0.f, 0.5f}},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("position of the first point (in UV coordinates)")},
    {"pos1",     NGLI_PARAM_TYPE_VEC2, OFFSET(pos1_node), {.vec={1.f, 0.5f}},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("position of the second point (in UV coordinates)")},
    {"mode",     NGLI_PARAM_TYPE_SELECT, OFFSET(mode), {.i32=GRADIENT_MODE_RAMP},
                 .choices=&gradient_mode_choices,
                 .desc=NGLI_DOCSTRING("mode of interpolation between the two points")},
    {"linear",   NGLI_PARAM_TYPE_BOOL, OFFSET(linear_node), {.i32=1},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("interpolate colors linearly")},
    {"blending", NGLI_PARAM_TYPE_SELECT, OFFSET(common.blending),
                 .choices=&ngli_blending_choices,
                 .desc=NGLI_DOCSTRING("define how this node and the current frame buffer are blending together")},
    {"geometry", NGLI_PARAM_TYPE_NODE, OFFSET(common.geometry),
                 .node_types=GEOMETRY_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("geometry to be rasterized")},
    {"filters",  NGLI_PARAM_TYPE_NODELIST, OFFSET(common.filters),
                 .node_types=FILTERS_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("filter chain to apply on top of this source")},
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct rendergradient4_opts, x)
static const struct node_param rendergradient4_params[] = {
    {"color_tl",   NGLI_PARAM_TYPE_VEC3, OFFSET(color_tl_node), {.vec={1.f, .5f, 0.f}}, /* orange */
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("top-left color")},
    {"color_tr",   NGLI_PARAM_TYPE_VEC3, OFFSET(color_tr_node), {.vec={0.f, 1.f, 0.f}}, /* green */
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("top-right color")},
    {"color_br",   NGLI_PARAM_TYPE_VEC3, OFFSET(color_br_node), {.vec={0.f, .5f, 1.f}}, /* azure */
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("bottom-right color")},
    {"color_bl",   NGLI_PARAM_TYPE_VEC3, OFFSET(color_bl_node), {.vec={1.f, .0f, 1.f}}, /* magenta */
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("bottom-left color")},
    {"opacity_tl", NGLI_PARAM_TYPE_F32, OFFSET(opacity_tl_node), {.f32=1.f},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("opacity of the top-left color")},
    {"opacity_tr", NGLI_PARAM_TYPE_F32, OFFSET(opacity_tr_node), {.f32=1.f},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("opacity of the top-right color")},
    {"opacity_br", NGLI_PARAM_TYPE_F32, OFFSET(opacity_br_node), {.f32=1.f},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("opacity of the bottom-right color")},
    {"opacity_bl", NGLI_PARAM_TYPE_F32, OFFSET(opacity_bl_node), {.f32=1.f},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("opacity of the bottol-left color")},
    {"linear",     NGLI_PARAM_TYPE_BOOL, OFFSET(linear_node), {.i32=1},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("interpolate colors linearly")},
    {"blending",   NGLI_PARAM_TYPE_SELECT, OFFSET(common.blending),
                   .choices=&ngli_blending_choices,
                   .desc=NGLI_DOCSTRING("define how this node and the current frame buffer are blending together")},
    {"geometry",   NGLI_PARAM_TYPE_NODE, OFFSET(common.geometry),
                   .node_types=GEOMETRY_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("geometry to be rasterized")},
    {"filters",    NGLI_PARAM_TYPE_NODELIST, OFFSET(common.filters),
                   .node_types=FILTERS_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("filter chain to apply on top of this source")},
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct rendertexture_opts, x)
static const struct node_param rendertexture_params[] = {
    {"texture",  NGLI_PARAM_TYPE_NODE, OFFSET(texture_node),
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("texture to render")},
    {"blending", NGLI_PARAM_TYPE_SELECT, OFFSET(common.blending),
                 .choices=&ngli_blending_choices,
                 .desc=NGLI_DOCSTRING("define how this node and the current frame buffer are blending together")},
    {"geometry", NGLI_PARAM_TYPE_NODE, OFFSET(common.geometry),
                 .node_types=GEOMETRY_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("geometry to be rasterized")},
    {"filters",  NGLI_PARAM_TYPE_NODELIST, OFFSET(common.filters),
                 .node_types=FILTERS_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("filter chain to apply on top of this source")},
    {NULL}
};
#undef OFFSET

static const float default_vertices[] = {
   -1.f,-1.f, 0.f,
    1.f,-1.f, 0.f,
   -1.f, 1.f, 0.f,
    1.f, 1.f, 0.f,
};

static const float default_uvcoords[] = {
    0.f, 1.f,
    1.f, 1.f,
    0.f, 0.f,
    1.f, 0.f,
};

static int combine_filters_code(struct render_common *s, const struct render_common_opts *o,
                                const char *base_name, const char *base_fragment)
{
    s->filterschain = ngli_filterschain_create();
    if (!s->filterschain)
        return NGL_ERROR_MEMORY;

    int ret = ngli_filterschain_init(s->filterschain, base_name, base_fragment, s->helpers);
    if (ret < 0)
        return ret;

    for (int i = 0; i < o->nb_filters; i++) {
        const struct ngl_node *filter_node = o->filters[i];
        const struct filter *filter = filter_node->priv_data;
        ret = ngli_filterschain_add_filter(s->filterschain, filter);
        if (ret < 0)
            return ret;
    }

    s->combined_fragment = ngli_filterschain_get_combination(s->filterschain);
    if (!s->combined_fragment)
        return NGL_ERROR_MEMORY;

    return 0;
}

static void draw_simple(struct render_common *s,  const struct render_common_opts *o,
                        struct pipeline_compat *pl_compat)
{
    ngli_pipeline_compat_draw(pl_compat, s->nb_vertices, 1);
}

static void draw_indexed(struct render_common *s,  const struct render_common_opts *o,
                         struct pipeline_compat *pl_compat)
{
    const struct geometry *geom = *(struct geometry **)o->geometry->priv_data;
    ngli_pipeline_compat_draw_indexed(pl_compat,
                                      geom->indices_buffer,
                                      geom->indices_layout.format,
                                      geom->indices_layout.count, 1);
}

static int init(struct ngl_node *node,
                struct render_common *s, const struct render_common_opts *o,
                const char *base_name, const char *base_fragment)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    snprintf(s->position_attr.name, sizeof(s->position_attr.name), "position");
    s->position_attr.type   = NGLI_TYPE_VEC3;
    s->position_attr.format = NGLI_FORMAT_R32G32B32_SFLOAT;

    snprintf(s->uvcoord_attr.name, sizeof(s->uvcoord_attr.name), "uvcoord");
    s->uvcoord_attr.type   = NGLI_TYPE_VEC2;
    s->uvcoord_attr.format = NGLI_FORMAT_R32G32_SFLOAT;

    if (!o->geometry) {
        s->uvcoords = ngli_buffer_create(gpu_ctx);
        s->vertices = ngli_buffer_create(gpu_ctx);
        if (!s->uvcoords || !s->vertices)
            return NGL_ERROR_MEMORY;

        int ret;
        if ((ret = ngli_buffer_init(s->vertices, sizeof(default_vertices), VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->uvcoords, sizeof(default_uvcoords), VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_upload(s->vertices, default_vertices, sizeof(default_vertices), 0)) < 0 ||
            (ret = ngli_buffer_upload(s->uvcoords, default_uvcoords, sizeof(default_uvcoords), 0)) < 0)
            return ret;

        s->position_attr.stride = 3 * 4;
        s->position_attr.buffer = s->vertices;

        s->uvcoord_attr.stride = 2 * 4;
        s->uvcoord_attr.buffer = s->uvcoords;

        s->nb_vertices = 4;
        s->topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        s->draw = draw_simple;
    } else {
        struct geometry *geom_node = *(struct geometry **)o->geometry->priv_data;
        struct buffer *vertices = geom_node->vertices_buffer;
        struct buffer *uvcoords = geom_node->uvcoords_buffer;
        struct buffer_layout vertices_layout = geom_node->vertices_layout;
        struct buffer_layout uvcoords_layout = geom_node->uvcoords_layout;

        if (!uvcoords) {
            LOG(ERROR, "the specified geometry is missing UV coordinates");
            return NGL_ERROR_INVALID_USAGE;
        }

        if (vertices_layout.type != NGLI_TYPE_VEC3) {
            LOG(ERROR, "only geometry with vec3 vertices are supported");
            return NGL_ERROR_UNSUPPORTED;
        }

        if (uvcoords && uvcoords_layout.type != NGLI_TYPE_VEC2) {
            LOG(ERROR, "only geometry with vec2 uvcoords are supported");
            return NGL_ERROR_UNSUPPORTED;
        }

        s->position_attr.stride = vertices_layout.stride;
        s->position_attr.offset = vertices_layout.offset;
        s->position_attr.buffer = vertices;

        s->uvcoord_attr.stride = uvcoords_layout.stride;
        s->uvcoord_attr.offset = uvcoords_layout.offset;
        s->uvcoord_attr.buffer = uvcoords;

        s->nb_vertices = vertices_layout.count;
        s->topology = geom_node->topology;
        s->draw = geom_node->indices_buffer ? draw_indexed : draw_simple;
    }

    return combine_filters_code(s, o, base_name, base_fragment);
}

static int rendercolor_init(struct ngl_node *node)
{
    struct rendercolor_priv *s = node->priv_data;
    struct rendercolor_opts *o = node->opts;
    return init(node, &s->common, &o->common, "source_color", source_color_frag);
}

static int rendergradient_init(struct ngl_node *node)
{
    struct rendergradient_priv *s = node->priv_data;
    struct rendergradient_opts *o = node->opts;
    s->common.helpers = NGLI_FILTER_HELPER_LINEAR2SRGB | NGLI_FILTER_HELPER_SRGB2LINEAR;
    return init(node, &s->common, &o->common, "source_gradient", source_gradient_frag);
}

static int rendergradient4_init(struct ngl_node *node)
{
    struct rendergradient4_priv *s = node->priv_data;
    struct rendergradient4_opts *o = node->opts;
    s->common.helpers = NGLI_FILTER_HELPER_LINEAR2SRGB | NGLI_FILTER_HELPER_SRGB2LINEAR;
    return init(node, &s->common, &o->common, "source_gradient4", source_gradient4_frag);
}

static int rendertexture_init(struct ngl_node *node)
{
    struct rendertexture_priv *s = node->priv_data;
    struct rendertexture_opts *o = node->opts;
    return init(node, &s->common, &o->common, "source_texture", source_texture_frag);
}

static int init_desc(struct ngl_node *node, struct render_common *s,
                     const struct pgcraft_uniform *uniforms, int nb_uniforms)
{
    struct rnode *rnode = node->ctx->rnode_pos;

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    rnode->id = ngli_darray_count(&s->pipeline_descs) - 1;

    memset(desc, 0, sizeof(*desc));

    ngli_darray_init(&desc->uniforms, sizeof(struct pgcraft_uniform), 0);
    ngli_darray_init(&desc->uniforms_map, sizeof(struct uniform_map), 0);

    /* register source uniforms */
    for (int i = 0; i < nb_uniforms; i++)
        if (!ngli_darray_push(&desc->uniforms, &uniforms[i]))
            return NGL_ERROR_MEMORY;

    /* register filters uniforms */
    const struct darray *comb_uniforms_array = ngli_filterschain_get_resources(s->filterschain);
    const struct pgcraft_uniform *comb_uniforms = ngli_darray_data(comb_uniforms_array);
    for (int i = 0; i < ngli_darray_count(comb_uniforms_array); i++)
        if (!ngli_darray_push(&desc->uniforms, &comb_uniforms[i]))
            return NGL_ERROR_MEMORY;

    return 0;
}

static int build_uniforms_map(struct pipeline_desc *desc)
{
    const struct pgcraft_uniform *uniforms = ngli_darray_data(&desc->uniforms);
    for (int i = 0; i < ngli_darray_count(&desc->uniforms); i++) {
        const struct pgcraft_uniform *uniform = &uniforms[i];
        const int index = ngli_pgcraft_get_uniform_index(desc->crafter, uniform->name, uniform->stage);

        /* The following can happen if the driver makes optimisation (MESA is
         * typically able to optimize several passes of the same filter) */
        if (index < 0)
            continue;

        /* This skips unwanted uniforms such as modelview and projection which
         * are handled separately */
        if (!uniform->data)
            continue;

        const struct uniform_map map = {.index=index, .data=uniform->data};
        if (!ngli_darray_push(&desc->uniforms_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int finalize_pipeline(struct ngl_node *node,
                             struct render_common *s, const struct render_common_opts *o,
                             const struct pgcraft_params *crafter_params)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[rnode->id];

    struct graphicstate state = rnode->graphicstate;
    int ret = ngli_blending_apply_preset(&state, o->blending);
    if (ret < 0)
        return ret;

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcraft_craft(desc->crafter, crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!desc->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_params pipeline_params = {
        .type = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics = {
            .topology = s->topology,
            .state    = state,
            .rt_desc  = rnode->rendertarget_desc,
        },
        .program = ngli_pgcraft_get_program(desc->crafter),
        .layout = ngli_pgcraft_get_pipeline_layout(desc->crafter),
    };

    const struct pipeline_resources pipeline_resources = ngli_pgcraft_get_pipeline_resources(desc->crafter);
    const struct pgcraft_compat_info *compat_info = ngli_pgcraft_get_compat_info(desc->crafter);

    const struct pipeline_compat_params params = {
        .params = &pipeline_params,
        .resources = &pipeline_resources,
        .compat_info = compat_info,
    };

    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    ret = build_uniforms_map(desc);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->aspect_index = ngli_pgcraft_get_uniform_index(desc->crafter, "aspect", NGLI_PROGRAM_SHADER_FRAG);
    return 0;
}

static void *get_data_ptr(struct ngl_node *var_node, void *data_fallback)
{
    if (!var_node)
        return data_fallback;
    struct variable_info *var = var_node->priv_data;
    return var->data;
}

static int rendercolor_prepare(struct ngl_node *node)
{
    struct rendercolor_priv *s = node->priv_data;
    struct rendercolor_opts *o = node->opts;
    const struct pgcraft_uniform uniforms[] = {
        {.name="modelview_matrix",  .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="color",             .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color_node, o->color)},
        {.name="opacity",           .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity_node, &o->opacity)},
    };

    struct render_common *c = &s->common;
    int ret = init_desc(node, c, uniforms, NGLI_ARRAY_NB(uniforms));
    if (ret < 0)
        return ret;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGLI_TYPE_VEC2},
    };

    const struct pipeline_desc *descs = ngli_darray_data(&c->pipeline_descs);
    const struct pipeline_desc *desc = &descs[node->ctx->rnode_pos->id];
    const struct pgcraft_attribute attributes[] = {c->position_attr, c->uvcoord_attr};
    const struct pgcraft_params crafter_params = {
        .vert_base        = source_color_vert,
        .frag_base        = c->combined_fragment,
        .uniforms         = ngli_darray_data(&desc->uniforms),
        .nb_uniforms      = ngli_darray_count(&desc->uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    const struct render_common_opts *co = &o->common;
    return finalize_pipeline(node, c, co, &crafter_params);
}

static int rendergradient_prepare(struct ngl_node *node)
{
    struct rendergradient_priv *s = node->priv_data;
    struct rendergradient_opts *o = node->opts;
    const struct pgcraft_uniform uniforms[] = {
        {.name="modelview_matrix",  .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="aspect",            .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG},
        {.name="color0",            .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color0_node, o->color0)},
        {.name="color1",            .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color1_node, o->color1)},
        {.name="opacity0",          .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity0_node, &o->opacity0)},
        {.name="opacity1",          .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity1_node, &o->opacity1)},
        {.name="pos0",              .type=NGLI_TYPE_VEC2,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->pos0_node, o->pos0)},
        {.name="pos1",              .type=NGLI_TYPE_VEC2,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->pos1_node, o->pos1)},
        {.name="mode",              .type=NGLI_TYPE_INT,   .stage=NGLI_PROGRAM_SHADER_FRAG, .data=&o->mode},
        {.name="linear",            .type=NGLI_TYPE_BOOL,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->linear_node, &o->linear)},
    };

    struct render_common *c = &s->common;
    int ret = init_desc(node, c, uniforms, NGLI_ARRAY_NB(uniforms));
    if (ret < 0)
        return ret;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGLI_TYPE_VEC2},
    };

    const struct pipeline_desc *descs = ngli_darray_data(&c->pipeline_descs);
    const struct pipeline_desc *desc = &descs[node->ctx->rnode_pos->id];
    const struct pgcraft_attribute attributes[] = {c->position_attr, c->uvcoord_attr};
    const struct pgcraft_params crafter_params = {
        .vert_base        = source_gradient_vert,
        .frag_base        = c->combined_fragment,
        .uniforms         = ngli_darray_data(&desc->uniforms),
        .nb_uniforms      = ngli_darray_count(&desc->uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    const struct render_common_opts *co = &o->common;
    return finalize_pipeline(node, c, co, &crafter_params);
}

static int rendergradient4_prepare(struct ngl_node *node)
{
    struct rendergradient4_priv *s = node->priv_data;
    struct rendergradient4_opts *o = node->opts;
    const struct pgcraft_uniform uniforms[] = {
        {.name="modelview_matrix",  .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="color_tl",          .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color_tl_node, o->color_tl)},
        {.name="color_tr",          .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color_tr_node, o->color_tr)},
        {.name="color_br",          .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color_br_node, o->color_br)},
        {.name="color_bl",          .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->color_bl_node, o->color_bl)},
        {.name="opacity_tl",        .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity_tl_node, &o->opacity_tl)},
        {.name="opacity_tr",        .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity_tr_node, &o->opacity_tr)},
        {.name="opacity_br",        .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity_br_node, &o->opacity_br)},
        {.name="opacity_bl",        .type=NGLI_TYPE_FLOAT, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->opacity_bl_node, &o->opacity_bl)},
        {.name="linear",            .type=NGLI_TYPE_BOOL,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=get_data_ptr(o->linear_node, &o->linear)},
    };

    struct render_common *c = &s->common;
    int ret = init_desc(node, c, uniforms, NGLI_ARRAY_NB(uniforms));
    if (ret < 0)
        return ret;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGLI_TYPE_VEC2},
    };

    const struct pipeline_desc *descs = ngli_darray_data(&c->pipeline_descs);
    const struct pipeline_desc *desc = &descs[node->ctx->rnode_pos->id];
    const struct pgcraft_attribute attributes[] = {c->position_attr, c->uvcoord_attr};
    const struct pgcraft_params crafter_params = {
        .vert_base        = source_gradient4_vert,
        .frag_base        = c->combined_fragment,
        .uniforms         = ngli_darray_data(&desc->uniforms),
        .nb_uniforms      = ngli_darray_count(&desc->uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    const struct render_common_opts *co = &o->common;
    return finalize_pipeline(node, c, co, &crafter_params);
}

static int rendertexture_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rendertexture_priv *s = node->priv_data;
    struct rendertexture_opts *o = node->opts;

    static const struct pgcraft_uniform uniforms[] = {
        {.name="modelview_matrix",  .type=NGLI_TYPE_MAT4, .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGLI_TYPE_MAT4, .stage=NGLI_PROGRAM_SHADER_VERT},
    };

    struct render_common *c = &s->common;
    int ret = init_desc(node, c, uniforms, NGLI_ARRAY_NB(uniforms));
    if (ret < 0)
        return ret;

    struct texture_priv *texture_priv = o->texture_node->priv_data;
    struct texture_opts *texture_opts = o->texture_node->opts;
    struct pgcraft_texture textures[] = {
        {
            .name        = "tex",
            .stage       = NGLI_PROGRAM_SHADER_FRAG,
            .image       = &texture_priv->image,
            .format      = texture_priv->params.format,
            .clamp_video = texture_opts->clamp_video,
        },
    };

    if (texture_opts->data_src && texture_opts->data_src->cls->id == NGL_NODE_MEDIA)
        textures[0].type = NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO;
    else
        textures[0].type = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D;

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "uv",        .type = NGLI_TYPE_VEC2},
        {.name = "tex_coord", .type = NGLI_TYPE_VEC2},
    };

    const struct pipeline_desc *descs = ngli_darray_data(&c->pipeline_descs);
    const struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    const struct pgcraft_attribute attributes[] = {c->position_attr, c->uvcoord_attr};
    const struct pgcraft_params crafter_params = {
        .vert_base        = source_texture_vert,
        .frag_base        = c->combined_fragment,
        .uniforms         = ngli_darray_data(&desc->uniforms),
        .nb_uniforms      = ngli_darray_count(&desc->uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    const struct render_common_opts *co = &o->common;
    return finalize_pipeline(node, c, co, &crafter_params);
}

static void renderother_draw(struct ngl_node *node, struct render_common *s, const struct render_common_opts *o)
{
    struct ngl_ctx *ctx = node->ctx;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline_compat *pl_compat = desc->pipeline_compat;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_compat_update_uniform(pl_compat, desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(pl_compat, desc->projection_matrix_index, projection_matrix);

    if (desc->aspect_index >= 0) {
        int viewport[4] = {0};
        ngli_gpu_ctx_get_viewport(ctx->gpu_ctx, viewport);
        const float aspect = viewport[2] / (float)viewport[3];
        ngli_pipeline_compat_update_uniform(pl_compat, desc->aspect_index, &aspect);
    }

    const struct uniform_map *map = ngli_darray_data(&desc->uniforms_map);
    for (int i = 0; i < ngli_darray_count(&desc->uniforms_map); i++)
        ngli_pipeline_compat_update_uniform(pl_compat, map[i].index, map[i].data);

    if (node->cls->id == NGL_NODE_RENDERTEXTURE) {
        const struct darray *texture_infos_array = ngli_pgcraft_get_texture_infos(desc->crafter);
        const struct pgcraft_texture_info *info = ngli_darray_data(texture_infos_array);
        ngli_pipeline_compat_update_texture_info(pl_compat, info);
    }

    if (!ctx->render_pass_started) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->render_pass_started = 1;
    }

    s->draw(s, o, desc->pipeline_compat);
}

static void renderother_uninit(struct ngl_node *node, struct render_common *s)
{
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const int nb_descs = ngli_darray_count(&s->pipeline_descs);
    for (int i = 0; i < nb_descs; i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pgcraft_freep(&desc->crafter);
        ngli_pipeline_compat_freep(&desc->pipeline_compat);
        ngli_darray_reset(&desc->uniforms);
        ngli_darray_reset(&desc->uniforms_map);
    }
    ngli_freep(&s->combined_fragment);
    ngli_filterschain_freep(&s->filterschain);
    ngli_darray_reset(&s->pipeline_descs);
    ngli_buffer_freep(&s->vertices);
    ngli_buffer_freep(&s->uvcoords);
}

#define DECLARE_RENDEROTHER(type, cls_id, cls_name) \
static void type##_draw(struct ngl_node *node)      \
{                                                   \
    struct type##_priv *s = node->priv_data;        \
    const struct type##_opts *o = node->opts;       \
    renderother_draw(node, &s->common, &o->common); \
}                                                   \
                                                    \
static void type##_uninit(struct ngl_node *node)    \
{                                                   \
    struct type##_priv *s = node->priv_data;        \
    renderother_uninit(node, &s->common);           \
}                                                   \
                                                    \
const struct node_class ngli_##type##_class = {     \
    .id        = cls_id,                            \
    .category  = NGLI_NODE_CATEGORY_RENDER,         \
    .name      = cls_name,                          \
    .init      = type##_init,                       \
    .prepare   = type##_prepare,                    \
    .update    = ngli_node_update_children,         \
    .draw      = type##_draw,                       \
    .uninit    = type##_uninit,                     \
    .opts_size = sizeof(struct type##_opts),        \
    .priv_size = sizeof(struct type##_priv),        \
    .params    = type##_params,                     \
    .file      = __FILE__,                          \
};

DECLARE_RENDEROTHER(rendercolor,     NGL_NODE_RENDERCOLOR,     "RenderColor")
DECLARE_RENDEROTHER(rendergradient,  NGL_NODE_RENDERGRADIENT,  "RenderGradient")
DECLARE_RENDEROTHER(rendergradient4, NGL_NODE_RENDERGRADIENT4, "RenderGradient4")
DECLARE_RENDEROTHER(rendertexture,   NGL_NODE_RENDERTEXTURE,   "RenderTexture")
