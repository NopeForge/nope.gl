/*
 * Copyright 2023 Nope Forge
 * Copyright 2021-2022 GoPro Inc.
 * Copyright 2021-2024 Clément Bœsch <u pkh.me>
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
#include "filterschain.h"
#include "geometry.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/ctx.h"
#include "ngpu/type.h"
#include "node_block.h"
#include "node_buffer.h"
#include "node_texture.h"
#include "node_uniform.h"
#include "pipeline_compat.h"
#include "ngpu/pgcraft.h"
#include "transforms.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"

/* GLSL fragments as string */
#include "source_color_frag.h"
#include "source_color_vert.h"
#include "source_displace_frag.h"
#include "source_displace_vert.h"
#include "source_gradient_frag.h"
#include "source_gradient_vert.h"
#include "source_gradient4_frag.h"
#include "source_gradient4_vert.h"
#include "source_histogram_frag.h"
#include "source_histogram_vert.h"
#include "source_mask_frag.h"
#include "source_mask_vert.h"
#include "source_noise_frag.h"
#include "source_noise_vert.h"
#include "source_texture_frag.h"
#include "source_texture_vert.h"
#include "source_waveform_frag.h"
#include "source_waveform_vert.h"

#define VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                            NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT) \

#define GEOMETRY_TYPES_LIST (const uint32_t[]){NGL_NODE_CIRCLE,          \
                                               NGL_NODE_GEOMETRY,        \
                                               NGL_NODE_QUAD,            \
                                               NGL_NODE_TRIANGLE,        \
                                               NGLI_NODE_NONE}

#define FILTERS_TYPES_LIST (const uint32_t[]){NGL_NODE_FILTERALPHA,          \
                                              NGL_NODE_FILTERCOLORMAP,       \
                                              NGL_NODE_FILTERCONTRAST,       \
                                              NGL_NODE_FILTEREXPOSURE,       \
                                              NGL_NODE_FILTERINVERSEALPHA,   \
                                              NGL_NODE_FILTERLINEAR2SRGB,    \
                                              NGL_NODE_FILTEROPACITY,        \
                                              NGL_NODE_FILTERPREMULT,        \
                                              NGL_NODE_FILTERSATURATION,     \
                                              NGL_NODE_FILTERSELECTOR,       \
                                              NGL_NODE_FILTERSRGB2LINEAR,    \
                                              NGLI_NODE_NONE}

struct uniform_map {
    int32_t index;
    const void *data;
};

struct resource_map {
    int32_t index;
    const struct block_info *info;
    size_t buffer_rev;
};

struct texture_map {
    const struct image *image;
    size_t image_rev;
};

struct pipeline_desc {
    struct pipeline_compat *pipeline_compat;
    struct darray blocks_map; // struct resource_map
    struct darray textures_map; // struct texture_map
    struct darray reframing_nodes; // struct ngl_node *
};

struct draw_common_opts {
    int blending;
    struct ngl_node *geometry;
    struct ngl_node **filters;
    size_t nb_filters;
};

struct draw_common {
    uint32_t helpers;
    void (*draw)(struct draw_common *s, struct pipeline_compat *pl_compat);
    struct filterschain *filterschain;
    char *combined_fragment;
    struct ngpu_pgcraft_attribute position_attr;
    struct ngpu_pgcraft_attribute uvcoord_attr;
    uint32_t nb_vertices;
    enum ngpu_primitive_topology topology;
    struct geometry *geometry;
    int own_geometry;
    struct darray pipeline_descs;
    struct darray uniforms; // struct pgcraft_uniform
    struct ngpu_pgcraft *crafter;
    int32_t modelview_matrix_index;
    int32_t projection_matrix_index;
    int32_t aspect_index;
    struct darray uniforms_map; // struct uniform_map
};

struct drawcolor_opts {
    struct ngl_node *color_node;
    float color[3];
    struct ngl_node *opacity_node;
    float opacity;
    struct draw_common_opts common;
};

struct drawcolor_priv {
    struct draw_common common;
};

struct drawdisplace_opts {
    struct ngl_node *source_node;
    struct ngl_node *displacement_node;
    struct draw_common_opts common;
};

struct drawdisplace_priv {
    struct draw_common common;
};

struct drawgradient_opts {
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
    struct draw_common_opts common;
};

struct drawgradient_priv {
    struct draw_common common;
};

struct drawgradient4_opts {
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
    struct draw_common_opts common;
};

struct drawgradient4_priv {
    struct draw_common common;
};

struct drawhistogram_opts {
    struct ngl_node *stats;
    int mode;
    struct draw_common_opts common;
};

struct drawhistogram_priv {
    struct draw_common common;
};

struct drawmask_opts {
    struct ngl_node *content;
    struct ngl_node *mask;
    int inverted;
    struct draw_common_opts common;
};

struct drawmask_priv {
    struct draw_common common;
};

struct drawtexture_opts {
    struct ngl_node *texture_node;
    struct draw_common_opts common;
};

struct drawnoise_priv {
    struct draw_common common;
};

struct drawnoise_opts {
    int type;
    struct ngl_node *amplitude_node;
    float amplitude;
    uint32_t octaves;
    struct ngl_node *lacunarity_node;
    float lacunarity;
    struct ngl_node *gain_node;
    float gain;
    struct ngl_node *seed_node;
    uint32_t seed;
    struct ngl_node *scale_node;
    float scale[2];
    struct ngl_node *evolution_node;
    float evolution;
    struct draw_common_opts common;
};

struct drawtexture_priv {
    struct draw_common common;
};

struct drawwaveform_opts {
    struct ngl_node *stats;
    int mode;
    struct draw_common_opts common;
};

struct drawwaveform_priv {
    struct draw_common common;
};

#define COMMON_PARAMS                                                                                                \
    {"blending", NGLI_PARAM_TYPE_SELECT, OFFSET(common.blending),                                                    \
                 .choices=&ngli_blending_choices,                                                                    \
                 .desc=NGLI_DOCSTRING("define how this node and the current frame buffer are blending together")},   \
    {"geometry", NGLI_PARAM_TYPE_NODE, OFFSET(common.geometry),                                                      \
                 .node_types=GEOMETRY_TYPES_LIST,                                                                    \
                 .desc=NGLI_DOCSTRING("geometry to be rasterized")},                                                 \
    {"filters",  NGLI_PARAM_TYPE_NODELIST, OFFSET(common.filters),                                                   \
                 .node_types=FILTERS_TYPES_LIST,                                                                     \
                 .desc=NGLI_DOCSTRING("filter chain to apply on top of this source")},                               \

#define OFFSET(x) offsetof(struct drawcolor_opts, x)
static const struct node_param drawcolor_params[] = {
    {"color",    NGLI_PARAM_TYPE_VEC3, OFFSET(color_node), {.vec={1.f, 1.f, 1.f}},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("color of the shape")},
    {"opacity",  NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=1.f},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("opacity of the color")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct drawdisplace_opts, x)
static const struct node_param drawdisplace_params[] = {
    {"source",       NGLI_PARAM_TYPE_NODE, OFFSET(source_node),
                     .node_types=(const uint32_t[]){TRANSFORM_TYPES_ARGS, NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                     .flags=NGLI_PARAM_FLAG_NON_NULL,
                     .desc=NGLI_DOCSTRING("source texture to displace")},
    {"displacement", NGLI_PARAM_TYPE_NODE, OFFSET(displacement_node),
                     .node_types=(const uint32_t[]){TRANSFORM_TYPES_ARGS, NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                     .flags=NGLI_PARAM_FLAG_NON_NULL,
                     .desc=NGLI_DOCSTRING("displacement vectors stored in a texture")},
    COMMON_PARAMS
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

#define OFFSET(x) offsetof(struct drawgradient_opts, x)
static const struct node_param drawgradient_params[] = {
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
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct drawgradient4_opts, x)
static const struct node_param drawgradient4_params[] = {
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
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

enum {
    SCOPE_MODE_MIXED,
    SCOPE_MODE_PARADE,
    SCOPE_MODE_LUMA_ONLY,
};

const struct param_choices scope_mode_choices = {
    .name = "scope_mode",
    .consts = {
        {"mixed",     SCOPE_MODE_MIXED,     .desc=NGLI_DOCSTRING("R, G and B channels overlap on each others")},
        {"parade",    SCOPE_MODE_PARADE,    .desc=NGLI_DOCSTRING("split R, G and B channels")},
        {"luma_only", SCOPE_MODE_LUMA_ONLY, .desc=NGLI_DOCSTRING("only the luma channel")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct drawhistogram_opts, x)
static const struct node_param drawhistogram_params[] = {
    {"stats",    NGLI_PARAM_TYPE_NODE, OFFSET(stats),
                 .node_types=(const uint32_t[]){NGL_NODE_COLORSTATS, NGLI_NODE_NONE},
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("texture to render")},
    {"mode",     NGLI_PARAM_TYPE_SELECT, OFFSET(mode),
                 .choices=&scope_mode_choices,
                 .desc=NGLI_DOCSTRING("define how to represent the data")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct drawmask_opts, x)
static const struct node_param drawmask_params[] = {
    {"content",   NGLI_PARAM_TYPE_NODE, OFFSET(content),
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .node_types=(const uint32_t[]){TRANSFORM_TYPES_ARGS, NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                 .desc=NGLI_DOCSTRING("content texture being masked")},
    {"mask",      NGLI_PARAM_TYPE_NODE, OFFSET(mask),
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .node_types=(const uint32_t[]){TRANSFORM_TYPES_ARGS, NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                 .desc=NGLI_DOCSTRING("texture serving as mask (only the red channel is used)")},
    {"inverted",  NGLI_PARAM_TYPE_BOOL, OFFSET(inverted),
                 .desc=NGLI_DOCSTRING("whether to dig into or keep")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define NOISE_TYPE_BLOCKY 0
#define NOISE_TYPE_PERLIN 1

const struct param_choices noise_type_choices = {
    .name = "noise_type",
    .consts = {
        {"blocky",    NOISE_TYPE_BLOCKY,  .desc=NGLI_DOCSTRING("blocky noise")},
        {"perlin",    NOISE_TYPE_PERLIN,  .desc=NGLI_DOCSTRING("perlin noise")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct drawnoise_opts, x)
static const struct node_param drawnoise_params[] = {
    {"type",        NGLI_PARAM_TYPE_SELECT, OFFSET(type),
                    .choices=&noise_type_choices,
                    .desc=NGLI_DOCSTRING("noise type")},
    {"amplitude",   NGLI_PARAM_TYPE_F32, OFFSET(amplitude_node), {.f32=1.f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("by how much it oscillates")},
    {"octaves",     NGLI_PARAM_TYPE_U32, OFFSET(octaves), {.u32=3},
                    .desc=NGLI_DOCSTRING("number of accumulated noise layers (controls the level of details), must in [1,8]")},
    {"lacunarity",  NGLI_PARAM_TYPE_F32, OFFSET(lacunarity_node), {.f32=2.f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("frequency multiplier per octave")},
    {"gain",        NGLI_PARAM_TYPE_F32, OFFSET(gain_node), {.f32=0.5f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("amplitude multiplier per octave (also known as persistence)")},
    {"seed",        NGLI_PARAM_TYPE_U32, OFFSET(seed_node), {.u32=0},
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("random base seed")},
    {"scale",       NGLI_PARAM_TYPE_VEC2, OFFSET(scale_node), {.vec = {32.f, 32.f}},
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("size of the grid in lattice units")},
    {"evolution",   NGLI_PARAM_TYPE_F32, OFFSET(evolution_node), {.f32 = 0.0},
                    .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                    .desc=NGLI_DOCSTRING("evolution of the 3rd non-spatial dimension, time if unspecified")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct drawtexture_opts, x)
static const struct node_param drawtexture_params[] = {
    {"texture",  NGLI_PARAM_TYPE_NODE, OFFSET(texture_node),
                 .node_types=(const uint32_t[]){TRANSFORM_TYPES_ARGS, NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("texture to render")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct drawwaveform_opts, x)
static const struct node_param drawwaveform_params[] = {
    {"stats",    NGLI_PARAM_TYPE_NODE, OFFSET(stats),
                 .node_types=(const uint32_t[]){NGL_NODE_COLORSTATS, NGLI_NODE_NONE},
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("texture to render")},
    {"mode",     NGLI_PARAM_TYPE_SELECT, OFFSET(mode),
                 .choices=&scope_mode_choices,
                 .desc=NGLI_DOCSTRING("define how to represent the data")},
    COMMON_PARAMS
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

static int combine_filters_code(struct draw_common *s, const struct draw_common_opts *o,
                                const char *base_name, const char *base_fragment)
{
    s->filterschain = ngli_filterschain_create();
    if (!s->filterschain)
        return NGL_ERROR_MEMORY;

    int ret = ngli_filterschain_init(s->filterschain, base_name, base_fragment, s->helpers);
    if (ret < 0)
        return ret;

    for (size_t i = 0; i < o->nb_filters; i++) {
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

static void draw_simple(struct draw_common *s, struct pipeline_compat *pl_compat)
{
    ngli_pipeline_compat_draw(pl_compat, s->nb_vertices, 1, 0);
}

static void draw_indexed(struct draw_common *s, struct pipeline_compat *pl_compat)
{
    ngli_pipeline_compat_draw_indexed(pl_compat,
                                      s->geometry->indices_buffer,
                                      s->geometry->indices_layout.format,
                                      (uint32_t)s->geometry->indices_layout.count, 1);
}

static void reset_pipeline_desc(void *user_arg, void *data)
{
    struct pipeline_desc *desc = data;
    ngli_pipeline_compat_freep(&desc->pipeline_compat);
    ngli_darray_reset(&desc->blocks_map);
    ngli_darray_reset(&desc->textures_map);
    ngli_darray_reset(&desc->reframing_nodes);
}

static int init(struct ngl_node *node,
                struct draw_common *s, const struct draw_common_opts *o,
                const char *base_name, const char *base_fragment)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);
    ngli_darray_set_free_func(&s->pipeline_descs, reset_pipeline_desc, NULL);

    snprintf(s->position_attr.name, sizeof(s->position_attr.name), "position");
    s->position_attr.type   = NGPU_TYPE_VEC3;
    s->position_attr.format = NGPU_FORMAT_R32G32B32_SFLOAT;

    snprintf(s->uvcoord_attr.name, sizeof(s->uvcoord_attr.name), "uvcoord");
    s->uvcoord_attr.type   = NGPU_TYPE_VEC2;
    s->uvcoord_attr.format = NGPU_FORMAT_R32G32_SFLOAT;

    if (!o->geometry) {
        s->own_geometry = 1;

        s->geometry = ngli_geometry_create(gpu_ctx);
        if (!s->geometry)
            return NGL_ERROR_MEMORY;

        int ret;
        if ((ret = ngli_geometry_set_vertices(s->geometry, 4, default_vertices)) < 0 ||
            (ret = ngli_geometry_set_uvcoords(s->geometry, 4, default_uvcoords)) < 0 ||
            (ret = ngli_geometry_init(s->geometry, NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) < 0))
            return ret;
    } else {
        s->geometry = *(struct geometry **)o->geometry->priv_data;
    }

    struct ngpu_buffer *vertices = s->geometry->vertices_buffer;
    struct ngpu_buffer *uvcoords = s->geometry->uvcoords_buffer;
    struct buffer_layout vertices_layout = s->geometry->vertices_layout;
    struct buffer_layout uvcoords_layout = s->geometry->uvcoords_layout;

    if (!uvcoords) {
        LOG(ERROR, "the specified geometry is missing UV coordinates");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (vertices_layout.type != NGPU_TYPE_VEC3) {
        LOG(ERROR, "only geometry with vec3 vertices are supported");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (uvcoords && uvcoords_layout.type != NGPU_TYPE_VEC2) {
        LOG(ERROR, "only geometry with vec2 uvcoords are supported");
        return NGL_ERROR_UNSUPPORTED;
    }

    s->position_attr.stride = vertices_layout.stride;
    s->position_attr.offset = vertices_layout.offset;
    s->position_attr.buffer = vertices;

    s->uvcoord_attr.stride = uvcoords_layout.stride;
    s->uvcoord_attr.offset = uvcoords_layout.offset;
    s->uvcoord_attr.buffer = uvcoords;

    s->nb_vertices = (uint32_t)vertices_layout.count;
    s->topology = s->geometry->topology;
    s->draw = s->geometry->indices_buffer ? draw_indexed : draw_simple;

    return combine_filters_code(s, o, base_name, base_fragment);
}

static int register_uniforms(struct draw_common *desc,
                             const struct ngpu_pgcraft_uniform *uniforms, size_t nb_uniforms,
                             struct filterschain *filterschain)
{
    ngli_darray_init(&desc->uniforms, sizeof(struct ngpu_pgcraft_uniform), 0);

    /* register common uniforms */
    const struct ngpu_pgcraft_uniform common_uniforms[] = {
        {.name="modelview_matrix",  .type=NGPU_TYPE_MAT4,  .stage=NGPU_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGPU_TYPE_MAT4,  .stage=NGPU_PROGRAM_SHADER_VERT},
        {.name="aspect",            .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG},
    };
    for (size_t i = 0; i < NGLI_ARRAY_NB(common_uniforms); i++)
        if (!ngli_darray_push(&desc->uniforms, &common_uniforms[i]))
            return NGL_ERROR_MEMORY;

    /* register source uniforms */
    for (size_t i = 0; i < nb_uniforms; i++)
        if (!ngli_darray_push(&desc->uniforms, &uniforms[i]))
            return NGL_ERROR_MEMORY;

    /* register filters uniforms */
    const struct darray *comb_uniforms_array = ngli_filterschain_get_resources(filterschain);
    const struct ngpu_pgcraft_uniform *comb_uniforms = ngli_darray_data(comb_uniforms_array);
    for (size_t i = 0; i < ngli_darray_count(comb_uniforms_array); i++)
        if (!ngli_darray_push(&desc->uniforms, &comb_uniforms[i]))
            return NGL_ERROR_MEMORY;

    return 0;
}

static int build_uniforms_map(struct draw_common *draw_common)
{
    ngli_darray_init(&draw_common->uniforms_map, sizeof(struct uniform_map), 0);

    const struct ngpu_pgcraft_uniform *uniforms = ngli_darray_data(&draw_common->uniforms);
    for (size_t i = 0; i < ngli_darray_count(&draw_common->uniforms); i++) {
        const struct ngpu_pgcraft_uniform *uniform = &uniforms[i];
        const int32_t index = ngpu_pgcraft_get_uniform_index(draw_common->crafter, uniform->name, uniform->stage);

        /* The following can happen if the driver makes optimisation (MESA is
         * typically able to optimize several passes of the same filter) */
        if (index < 0)
            continue;

        /* This skips unwanted uniforms such as modelview and projection which
         * are handled separately */
        if (!uniform->data)
            continue;

        const struct uniform_map map = {.index=index, .data=uniform->data};
        if (!ngli_darray_push(&draw_common->uniforms_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int finalize_init(struct ngl_node *node, struct draw_common *draw_common, const struct ngpu_pgcraft_params *crafter_params)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;

    draw_common->crafter = ngpu_pgcraft_create(gpu_ctx);
    if (!draw_common->crafter)
        return NGL_ERROR_MEMORY;

    int ret = ngpu_pgcraft_craft(draw_common->crafter, crafter_params);
    if (ret < 0)
        return ret;

    draw_common->modelview_matrix_index = ngpu_pgcraft_get_uniform_index(
        draw_common->crafter, "modelview_matrix", NGPU_PROGRAM_SHADER_VERT);
    draw_common->projection_matrix_index = ngpu_pgcraft_get_uniform_index(
        draw_common->crafter, "projection_matrix", NGPU_PROGRAM_SHADER_VERT);
    draw_common->aspect_index = ngpu_pgcraft_get_uniform_index(draw_common->crafter, "aspect", NGPU_PROGRAM_SHADER_FRAG);

    ret = build_uniforms_map(draw_common);
    if (ret < 0)
        return ret;

    return 0;
}

static int drawcolor_init(struct ngl_node *node)
{
    struct drawcolor_priv *s = node->priv_data;
    struct drawcolor_opts *o = node->opts;

    int ret = init(node, &s->common, &o->common, "source_color", source_color_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="color",             .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_node, o->color)},
        {.name="opacity",           .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_node, &o->opacity)},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr,
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawcolor",
        .vert_base        = source_color_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawdisplace_init(struct ngl_node *node)
{
    struct drawdisplace_priv *s = node->priv_data;
    struct drawdisplace_opts *o = node->opts;

    const struct ngl_node *source_node = ngli_transform_get_leaf_node(o->source_node);
    if (!source_node) {
        LOG(ERROR, "no source texture found at the end of the transform chain");
        return NGL_ERROR_INVALID_USAGE;
    }

    const struct ngl_node *displacement_node = ngli_transform_get_leaf_node(o->displacement_node);
    if (!displacement_node) {
        LOG(ERROR, "no source texture found at the end of the transform chain");
        return NGL_ERROR_INVALID_USAGE;
    }

    int ret = init(node, &s->common, &o->common, "source_displace", source_displace_frag);
    if (ret < 0)
        return ret;

    ret = register_uniforms(&s->common, NULL, 0, s->common.filterschain);
    if (ret < 0)
        return ret;

    struct texture_info *source_info = o->source_node->priv_data;
    struct texture_info *displacement_info = o->displacement_node->priv_data;
    const struct ngpu_pgcraft_texture textures[] = {
        {
            .name        = "source",
            .type        = ngli_node_texture_get_pgcraft_shader_tex_type(o->source_node),
            .stage       = NGPU_PROGRAM_SHADER_FRAG,
            .image       = &source_info->image,
            .format      = source_info->params.format,
            .clamp_video = source_info->clamp_video,
        }, {
            .name        = "displacement",
            .type        = ngli_node_texture_get_pgcraft_shader_tex_type(o->displacement_node),
            .stage       = NGPU_PROGRAM_SHADER_FRAG,
            .image       = &displacement_info->image,
            .format      = displacement_info->params.format,
            .clamp_video = displacement_info->clamp_video,
        },
    };

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv",                 .type = NGPU_TYPE_VEC2},
        {.name = "source_coord",       .type = NGPU_TYPE_VEC2},
        {.name = "displacement_coord", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr,
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawdisplace",
        .vert_base        = source_displace_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawgradient_init(struct ngl_node *node)
{
    struct drawgradient_priv *s = node->priv_data;
    struct drawgradient_opts *o = node->opts;
    s->common.helpers = NGLI_FILTER_HELPER_SRGB;
    int ret = init(node, &s->common, &o->common, "source_gradient", source_gradient_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="color0",            .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color0_node, o->color0)},
        {.name="color1",            .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color1_node, o->color1)},
        {.name="opacity0",          .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity0_node, &o->opacity0)},
        {.name="opacity1",          .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity1_node, &o->opacity1)},
        {.name="pos0",              .type=NGPU_TYPE_VEC2,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->pos0_node, o->pos0)},
        {.name="pos1",              .type=NGPU_TYPE_VEC2,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->pos1_node, o->pos1)},
        {.name="mode",              .type=NGPU_TYPE_I32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=&o->mode},
        {.name="linear",            .type=NGPU_TYPE_BOOL,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->linear_node, &o->linear)},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr,
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawgradient",
        .vert_base        = source_gradient_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawgradient4_init(struct ngl_node *node)
{
    struct drawgradient4_priv *s = node->priv_data;
    struct drawgradient4_opts *o = node->opts;

    s->common.helpers = NGLI_FILTER_HELPER_SRGB;

    int ret = init(node, &s->common, &o->common, "source_gradient4", source_gradient4_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="color_tl",          .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_tl_node, o->color_tl)},
        {.name="color_tr",          .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_tr_node, o->color_tr)},
        {.name="color_br",          .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_br_node, o->color_br)},
        {.name="color_bl",          .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_bl_node, o->color_bl)},
        {.name="opacity_tl",        .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_tl_node, &o->opacity_tl)},
        {.name="opacity_tr",        .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_tr_node, &o->opacity_tr)},
        {.name="opacity_br",        .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_br_node, &o->opacity_br)},
        {.name="opacity_bl",        .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_bl_node, &o->opacity_bl)},
        {.name="linear",            .type=NGPU_TYPE_BOOL,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->linear_node, &o->linear)},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr,
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawgradient4",
        .vert_base        = source_gradient4_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawhistogram_init(struct ngl_node *node)
{
    struct drawhistogram_priv *s = node->priv_data;
    struct drawhistogram_opts *o = node->opts;

    int ret = init(node, &s->common, &o->common, "source_histogram", source_histogram_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="mode", .type=NGPU_TYPE_I32, .stage=NGPU_PROGRAM_SHADER_FRAG, .data=&o->mode},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct block_info *block_info = o->stats->priv_data;
    const struct ngpu_pgcraft_block crafter_block = {
        .name     = "stats",
        .type     = NGPU_TYPE_STORAGE_BUFFER,
        .stage    = NGPU_PROGRAM_SHADER_FRAG,
        .block    = &block_info->block,
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawhistogram",
        .vert_base        = source_histogram_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .blocks           = &crafter_block,
        .nb_blocks        = 1,
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    ngli_node_block_extend_usage(o->stats, NGPU_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawmask_init(struct ngl_node *node)
{
    struct drawmask_priv *s = node->priv_data;
    struct drawmask_opts *o = node->opts;

    const struct ngl_node *content = ngli_transform_get_leaf_node(o->content);
    if (!content) {
        LOG(ERROR, "no content texture found at the end of the transform chain");
        return NGL_ERROR_INVALID_USAGE;
    }

    const struct ngl_node *mask = ngli_transform_get_leaf_node(o->mask);
    if (!mask) {
        LOG(ERROR, "no mask texture found at the end of the transform chain");
        return NGL_ERROR_INVALID_USAGE;
    }

    int ret = init(node, &s->common, &o->common, "source_mask", source_mask_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="inverted", .type=NGPU_TYPE_BOOL, .stage=NGPU_PROGRAM_SHADER_FRAG, .data=&o->inverted},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    struct texture_info *content_info = o->content->priv_data;
    struct texture_info *mask_info = o->mask->priv_data;
    struct ngpu_pgcraft_texture textures[] = {
        {
            .name        = "content",
            .type        = ngli_node_texture_get_pgcraft_shader_tex_type(o->content),
            .stage       = NGPU_PROGRAM_SHADER_FRAG,
            .image       = &content_info->image,
            .format      = content_info->params.format,
            .clamp_video = content_info->clamp_video,
        }, {
            .name        = "mask",
            .type        = ngli_node_texture_get_pgcraft_shader_tex_type(o->mask),
            .stage       = NGPU_PROGRAM_SHADER_FRAG,
            .image       = &mask_info->image,
            .format      = mask_info->params.format,
            .clamp_video = mask_info->clamp_video,
        },
    };

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv",            .type = NGPU_TYPE_VEC2},
        {.name = "content_coord", .type = NGPU_TYPE_VEC2},
        {.name = "mask_coord",    .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr
    };
    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawmask",
        .vert_base        = source_mask_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawnoise_init(struct ngl_node *node)
{
    struct drawnoise_priv *s = node->priv_data;
    struct drawnoise_opts *o = node->opts;
    if (o->octaves < 1 || o->octaves > 8) {
        LOG(ERROR, "octaves must be in [1,8]");
        return NGL_ERROR_INVALID_ARG;
    }

    s->common.helpers = NGLI_FILTER_HELPER_MISC_UTILS | NGLI_FILTER_HELPER_NOISE;
    int ret = init(node, &s->common, &o->common, "source_noise", source_noise_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="type",              .type=NGPU_TYPE_I32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=&o->type},
        {.name="amplitude",         .type=NGPU_TYPE_F32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->amplitude_node, &o->amplitude)},
        {.name="octaves",           .type=NGPU_TYPE_U32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=&o->octaves},
        {.name="lacunarity",        .type=NGPU_TYPE_F32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->lacunarity_node, &o->lacunarity)},
        {.name="gain",              .type=NGPU_TYPE_F32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->gain_node, &o->gain)},
        {.name="seed",              .type=NGPU_TYPE_U32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->seed_node, &o->seed)},
        {.name="scale",             .type=NGPU_TYPE_VEC2, .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->scale_node, o->scale)},
        {.name="evolution",         .type=NGPU_TYPE_F32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->evolution_node, &o->evolution)},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawnoise",
        .vert_base        = source_noise_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawtexture_init(struct ngl_node *node)
{
    struct drawtexture_priv *s = node->priv_data;
    struct drawtexture_opts *o = node->opts;

    const struct ngl_node *texture_node = ngli_transform_get_leaf_node(o->texture_node);
    if (!texture_node) {
        LOG(ERROR, "no texture found at the end of the transform chain");
        return NGL_ERROR_INVALID_USAGE;
    }

    int ret = init(node, &s->common, &o->common, "source_texture", source_texture_frag);
    if (ret < 0)
        return ret;

    ret = register_uniforms(&s->common, NULL, 0, s->common.filterschain);
    if (ret < 0)
        return ret;

    struct texture_info *texture_info = texture_node->priv_data;
    struct ngpu_pgcraft_texture textures[] = {
        {
            .name        = "tex",
            .type        = ngli_node_texture_get_pgcraft_shader_tex_type(texture_node),
            .stage       = NGPU_PROGRAM_SHADER_FRAG,
            .image       = &texture_info->image,
            .format      = texture_info->params.format,
            .clamp_video = texture_info->clamp_video,
        },
    };

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv",        .type = NGPU_TYPE_VEC2},
        {.name = "tex_coord", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr
    };
    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawtexture",
        .vert_base        = source_texture_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_init(node, &s->common, &crafter_params);
}

static int drawwaveform_init(struct ngl_node *node)
{
    struct drawwaveform_priv *s = node->priv_data;
    struct drawwaveform_opts *o = node->opts;

    int ret = init(node, &s->common, &o->common, "source_waveform", source_waveform_frag);
    if (ret < 0)
        return ret;

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="mode", .type=NGPU_TYPE_I32,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=&o->mode},
    };

    ret = register_uniforms(&s->common, uniforms, NGLI_ARRAY_NB(uniforms), s->common.filterschain);
    if (ret < 0)
        return ret;

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct block_info *block_info = o->stats->priv_data;
    const struct ngpu_pgcraft_block crafter_block = {
        .name     = "stats",
        .type     = NGPU_TYPE_STORAGE_BUFFER,
        .stage    = NGPU_PROGRAM_SHADER_FRAG,
        .block    = &block_info->block,
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        s->common.position_attr,
        s->common.uvcoord_attr
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/drawwaveform",
        .vert_base        = source_waveform_vert,
        .frag_base        = s->common.combined_fragment,
        .uniforms         = ngli_darray_data(&s->common.uniforms),
        .nb_uniforms      = ngli_darray_count(&s->common.uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .blocks           = &crafter_block,
        .nb_blocks        = 1,
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    ngli_node_block_extend_usage(o->stats, NGPU_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    ret = finalize_init(node, &s->common, &crafter_params);
    if (ret < 0)
        return ret;

    return 0;
}

static struct pipeline_desc *create_pipeline_desc(struct ngl_node *node)
{
    struct drawwaveform_priv *s = node->priv_data;
    struct rnode *rnode = node->ctx->rnode_pos;

    struct pipeline_desc *desc = ngli_darray_push(&s->common.pipeline_descs, NULL);
    if (!desc)
        return NULL;

    rnode->id = ngli_darray_count(&s->common.pipeline_descs) - 1;

    ngli_darray_init(&desc->blocks_map, sizeof(struct resource_map), 0);
    ngli_darray_init(&desc->textures_map, sizeof(struct texture_map), 0);
    ngli_darray_init(&desc->reframing_nodes, sizeof(struct ngl_node *), 0);

    return desc;
}

static int build_texture_map(struct draw_common *draw_common, struct pipeline_desc *desc)
{
    const struct ngpu_pgcraft_compat_info *info = ngpu_pgcraft_get_compat_info(draw_common->crafter);
    for (size_t i = 0; i < info->nb_texture_infos; i++) {
        const struct texture_map map = {.image = info->images[i], .image_rev = SIZE_MAX};
        if (!ngli_darray_push(&desc->textures_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int init_pipeline_desc(struct ngl_node *node, struct pipeline_desc *desc, int blending)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct draw_common *s = node->priv_data;

    struct ngpu_graphics_state state = rnode->graphics_state;
    int ret = ngli_blending_apply_preset(&state, blending);
    if (ret < 0)
        return ret;

    desc->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!desc->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics = {
            .topology     = s->topology,
            .state        = state,
            .rt_layout    = rnode->rendertarget_layout,
            .vertex_state = ngpu_pgcraft_get_vertex_state(s->crafter),
        },
        .program          = ngpu_pgcraft_get_program(s->crafter),
        .layout_desc      = ngpu_pgcraft_get_bindgroup_layout_desc(s->crafter),
        .resources        = ngpu_pgcraft_get_bindgroup_resources(s->crafter),
        .vertex_resources = ngpu_pgcraft_get_vertex_resources(s->crafter),
        .compat_info      = ngpu_pgcraft_get_compat_info(s->crafter),
    };

    ret = ngli_node_prepare_children(node);
    if (ret < 0)
        return ret;

    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    ret = build_texture_map(s, desc);
    if (ret < 0)
        return ret;

    return 0;
}

static int drawcolor_prepare(struct ngl_node *node)
{
    struct drawcolor_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    return init_pipeline_desc(node, desc, o->common.blending);
}

static int drawdisplace_prepare(struct ngl_node *node)
{
    struct drawdisplace_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    int ret = init_pipeline_desc(node, desc, o->common.blending);
    if (ret < 0)
        return ret;

    if (!ngli_darray_push(&desc->reframing_nodes, &o->source_node) ||
        !ngli_darray_push(&desc->reframing_nodes, &o->displacement_node))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int drawgradient_prepare(struct ngl_node *node)
{
    struct drawgradient_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    return init_pipeline_desc(node, desc, o->common.blending);
}

static int drawgradient4_prepare(struct ngl_node *node)
{
    struct drawgradient4_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    return init_pipeline_desc(node, desc, o->common.blending);
}

static int drawhistogram_prepare(struct ngl_node *node)
{
    struct drawhistogram_priv *s = node->priv_data;
    struct drawhistogram_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    int ret = init_pipeline_desc(node, desc, o->common.blending);
    if (ret < 0)
        return ret;

    const struct block_info *block_info = o->stats->priv_data;
    const int32_t index = ngpu_pgcraft_get_block_index(s->common.crafter, "stats", NGPU_PROGRAM_SHADER_FRAG);
    const struct resource_map map = {.index = index, .info = block_info, .buffer_rev = SIZE_MAX};
    if (!ngli_darray_push(&desc->blocks_map, &map))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int drawmask_prepare(struct ngl_node *node)
{
    struct drawmask_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    int ret = init_pipeline_desc(node, desc, o->common.blending);
    if (ret < 0)
        return ret;

    ngli_darray_init(&desc->reframing_nodes, sizeof(struct ngl_node *), 0);
    if (!ngli_darray_push(&desc->reframing_nodes, &o->content) ||
        !ngli_darray_push(&desc->reframing_nodes, &o->mask))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int drawnoise_prepare(struct ngl_node *node)
{
    struct drawnoise_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    return init_pipeline_desc(node, desc, o->common.blending);
}

static int drawtexture_prepare(struct ngl_node *node)
{
    struct drawtexture_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    int ret = init_pipeline_desc(node, desc, o->common.blending);
    if (ret < 0)
        return ret;

    if (!ngli_darray_push(&desc->reframing_nodes, &o->texture_node))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int drawwaveform_prepare(struct ngl_node *node)
{
    struct drawwaveform_priv *s = node->priv_data;
    struct drawwaveform_opts *o = node->opts;

    struct pipeline_desc *desc = create_pipeline_desc(node);
    if (!desc)
        return NGL_ERROR_MEMORY;

    int ret = init_pipeline_desc(node, desc, o->common.blending);
    if (ret < 0)
        return ret;

    const struct block_info *block_info = o->stats->priv_data;
    const int32_t index = ngpu_pgcraft_get_block_index(s->common.crafter, "stats", NGPU_PROGRAM_SHADER_FRAG);
    const struct resource_map map = {.index = index, .info = block_info, .buffer_rev = SIZE_MAX};
    if (!ngli_darray_push(&desc->blocks_map, &map))
        return NGL_ERROR_MEMORY;

    return 0;
}

static void drawother_draw(struct ngl_node *node, struct draw_common *s, const struct draw_common_opts *o)
{
    ngli_node_draw_children(node);

    struct ngl_ctx *ctx = node->ctx;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline_compat *pl_compat = desc->pipeline_compat;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_compat_update_uniform(pl_compat, s->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(pl_compat, s->projection_matrix_index, projection_matrix);

    if (s->aspect_index >= 0) {
        const float aspect = (float)ctx->viewport.width / (float)ctx->viewport.height;
        ngli_pipeline_compat_update_uniform(pl_compat, s->aspect_index, &aspect);
    }

    const struct uniform_map *uniform_map = ngli_darray_data(&s->uniforms_map);
    for (size_t i = 0; i < ngli_darray_count(&s->uniforms_map); i++)
        ngli_pipeline_compat_update_uniform(pl_compat, uniform_map[i].index, uniform_map[i].data);

    struct texture_map *texture_map = ngli_darray_data(&desc->textures_map);
    const struct ngl_node **reframing_nodes = ngli_darray_data(&desc->reframing_nodes);
    for (size_t i = 0; i < ngli_darray_count(&desc->textures_map); i++) {
        if (texture_map[i].image_rev != texture_map[i].image->rev) {
            ngli_pipeline_compat_update_image(pl_compat, (int32_t)i, texture_map[i].image);
            texture_map[i].image_rev = texture_map[i].image->rev;
        }

        NGLI_ALIGNED_MAT(reframing_matrix);
        ngli_transform_chain_compute(reframing_nodes[i], reframing_matrix);
        ngli_pipeline_compat_apply_reframing_matrix(pl_compat, (int32_t)i, texture_map[i].image, reframing_matrix);
    }

    struct resource_map *resource_map = ngli_darray_data(&desc->blocks_map);
    for (size_t i = 0; i < ngli_darray_count(&desc->blocks_map); i++) {
        const struct block_info *info = resource_map[i].info;
        if (resource_map[i].buffer_rev != info->buffer_rev) {
            ngli_pipeline_compat_update_buffer(pl_compat, resource_map[i].index, info->buffer, 0, 0);
            resource_map[i].buffer_rev = info->buffer_rev;
        }
    }

    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;

    if (!ngpu_ctx_is_render_pass_active(gpu_ctx)) {
        ngpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    }

    ngpu_ctx_set_viewport(gpu_ctx, &ctx->viewport);
    ngpu_ctx_set_scissor(gpu_ctx, &ctx->scissor);

    s->draw(s, desc->pipeline_compat);
}

static void drawother_uninit(struct ngl_node *node, struct draw_common *s)
{
    ngli_darray_reset(&s->pipeline_descs);
    ngpu_pgcraft_freep(&s->crafter);
    ngli_darray_reset(&s->uniforms);
    ngli_darray_reset(&s->uniforms_map);
    ngli_freep(&s->combined_fragment);
    ngli_filterschain_freep(&s->filterschain);
    if (s->own_geometry)
        ngli_geometry_freep(&s->geometry);
}

#define DECLARE_DRAWOTHER(type, cls_id, cls_name)   \
static void type##_draw(struct ngl_node *node)      \
{                                                   \
    struct type##_priv *s = node->priv_data;        \
    const struct type##_opts *o = node->opts;       \
    drawother_draw(node, &s->common, &o->common);   \
}                                                   \
                                                    \
static void type##_uninit(struct ngl_node *node)    \
{                                                   \
    struct type##_priv *s = node->priv_data;        \
    drawother_uninit(node, &s->common);             \
}                                                   \
                                                    \
const struct node_class ngli_##type##_class = {     \
    .id        = cls_id,                            \
    .category  = NGLI_NODE_CATEGORY_DRAW,           \
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

DECLARE_DRAWOTHER(drawcolor,     NGL_NODE_DRAWCOLOR,     "DrawColor")
DECLARE_DRAWOTHER(drawdisplace,  NGL_NODE_DRAWDISPLACE,  "DrawDisplace")
DECLARE_DRAWOTHER(drawgradient,  NGL_NODE_DRAWGRADIENT,  "DrawGradient")
DECLARE_DRAWOTHER(drawgradient4, NGL_NODE_DRAWGRADIENT4, "DrawGradient4")
DECLARE_DRAWOTHER(drawhistogram, NGL_NODE_DRAWHISTOGRAM, "DrawHistogram")
DECLARE_DRAWOTHER(drawmask,      NGL_NODE_DRAWMASK,      "DrawMask")
DECLARE_DRAWOTHER(drawnoise,     NGL_NODE_DRAWNOISE,     "DrawNoise")
DECLARE_DRAWOTHER(drawtexture,   NGL_NODE_DRAWTEXTURE,   "DrawTexture")
DECLARE_DRAWOTHER(drawwaveform,  NGL_NODE_DRAWWAVEFORM,  "DrawWaveform")
