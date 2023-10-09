/*
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

#include <stddef.h>
#include <string.h>
#include <math.h>

#include "blending.h"
#include "distmap.h"
#include "gpu_ctx.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nopegl.h"
#include "path.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "texture.h"
#include "topology.h"
#include "type.h"
#include "utils.h"

/* GLSL fragments as string */
#include "path_frag.h"
#include "path_vert.h"

struct uniform_map {
    int index;
    const void *data;
};

struct pipeline_desc {
    struct pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
    int modelview_matrix_index;
    int projection_matrix_index;
    int transform_index;
    int coords_fill_index;
    int coords_outline_index;
    struct darray uniforms_map; // struct uniform_map
    struct darray uniforms; // struct pgcraft_uniform
};

struct renderpath_opts {
    struct ngl_node *path_node;
    float viewbox[4];
    int32_t pt_size;
    int32_t dpi;
    int32_t aspect_ratio[2];
    struct ngl_node *transform_chain;
    struct ngl_node *color_node;
    float color[3];
    struct ngl_node *opacity_node;
    float opacity;
    struct ngl_node *outline_node;
    float outline;
    struct ngl_node *outline_color_node;
    float outline_color[3];
    struct ngl_node *glow_node;
    float glow;
    struct ngl_node *glow_color_node;
    float glow_color[3];
    struct ngl_node *blur_node;
    float blur;
};

struct renderpath_priv {
    struct distmap *distmap;
    struct path *path;
    struct darray pipeline_descs;
    int32_t atlas_coords_fill[4];
    int32_t atlas_coords_outline[4];
    NGLI_ALIGNED_MAT(transform);
};

#define OFFSET(x) offsetof(struct renderpath_opts, x)
static const struct node_param renderpath_params[] = {
    {"path",         NGLI_PARAM_TYPE_NODE, OFFSET(path_node),
                     .node_types=(const uint32_t[]){NGL_NODE_PATH, NGL_NODE_SMOOTHPATH, NGLI_NODE_NONE},
                     .flags=NGLI_PARAM_FLAG_NON_NULL,
                     .desc=NGLI_DOCSTRING("path to draw")},
    {"viewbox",      NGLI_PARAM_TYPE_VEC4, OFFSET(viewbox), {.vec={-1.f, -1.f, 2.f, 2.f}},
                     .desc=NGLI_DOCSTRING("vector space for interpreting the path (x, y, width, height)")},
    {"pt_size",      NGLI_PARAM_TYPE_I32, OFFSET(pt_size), {.i32=54},
                     .desc=NGLI_DOCSTRING("size in point (nominal size, 1pt = 1/72 inch)")},
    {"dpi",          NGLI_PARAM_TYPE_I32, OFFSET(dpi), {.i32=300},
                     .desc=NGLI_DOCSTRING("resolution (dot per inch)")},
    {"aspect_ratio", NGLI_PARAM_TYPE_IVEC2, OFFSET(aspect_ratio), {.ivec={1, 1}},
                     .desc=NGLI_DOCSTRING("aspect ratio")},
    {"color",        NGLI_PARAM_TYPE_VEC3, OFFSET(color_node), {.vec={1.f, 1.f, 1.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path fill color")},
    {"opacity",      NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path fill opacity")},
    {"outline",      NGLI_PARAM_TYPE_F32, OFFSET(outline_node), {.f32=.005f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path outline width")},
    {"outline_color", NGLI_PARAM_TYPE_VEC3, OFFSET(outline_color_node), {.vec={1.f, .7f, 0.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path outline color")},
    {"glow",         NGLI_PARAM_TYPE_F32, OFFSET(glow_node),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path glow width")},
    {"glow_color",   NGLI_PARAM_TYPE_VEC3, OFFSET(glow_color_node), {.vec={1.f, 1.f, 1.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path glow color")},
    {"blur",         NGLI_PARAM_TYPE_F32, OFFSET(blur_node),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("path blur")},
    {NULL}
};

static int renderpath_init(struct ngl_node *node)
{
    struct renderpath_priv *s = node->priv_data;
    const struct renderpath_opts *o = node->opts;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    s->distmap = ngli_distmap_create(node->ctx);
    if (!s->distmap)
        return NGL_ERROR_MEMORY;

    int ret = ngli_distmap_init(s->distmap);
    if (ret < 0)
        return ret;

    const struct path *src_path = *(struct path **)o->path_node->priv_data;

    s->path = ngli_path_create();
    if (!s->path)
        return NGL_ERROR_MEMORY;

    ret = ngli_path_add_path(s->path, src_path);
    if (ret < 0)
        return ret;

    /*
     * Build a matrix to transform path into normalized coordinates, scaled up
     * to the desired resolution.
     */
    const float res = (float)o->pt_size * (float)o->dpi / 72.f;
    const float vb_x = o->viewbox[0];
    const float vb_y = o->viewbox[1];
    const float vb_w = o->viewbox[2];
    const float vb_h = o->viewbox[3];
    const NGLI_ALIGNED_MAT(path_transform) = {
        res/vb_w, 0.f, 0.f, 0.f,
        0.f, res/vb_h, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        -vb_x/vb_w*res, -vb_y/vb_h*res, 0.f, 1.f,
    };
    ngli_path_transform(s->path, path_transform);

    ret = ngli_path_finalize(s->path);
    if (ret < 0)
        return ret;

    const float ar = (float)o->aspect_ratio[0] / (float)o->aspect_ratio[1];
    const int32_t shape_w = (int32_t)lrintf(ar > 1.f ? res * ar : res);
    const int32_t shape_h = (int32_t)lrintf(ar > 1.f ? res : res / ar);

    int32_t shape_id_fill;
    ret = ngli_distmap_add_shape(s->distmap, shape_w, shape_h, s->path, NGLI_DISTMAP_FLAG_PATH_AUTO_CLOSE, &shape_id_fill);
    if (ret< 0)
        return ret;

    int32_t shape_id_outline;
    ret = ngli_distmap_add_shape(s->distmap, shape_w, shape_h, s->path, 0, &shape_id_outline);
    if (ret< 0)
        return ret;

    ret = ngli_distmap_finalize(s->distmap);
    if (ret < 0)
        return ret;

    ngli_distmap_get_shape_coords(s->distmap, shape_id_fill,    s->atlas_coords_fill);
    ngli_distmap_get_shape_coords(s->distmap, shape_id_outline, s->atlas_coords_outline);

    float scale_fill[2], scale_outline[2];
    ngli_distmap_get_shape_scale(s->distmap, shape_id_fill,    scale_fill);
    ngli_distmap_get_shape_scale(s->distmap, shape_id_outline, scale_outline);
    ngli_assert(!memcmp(scale_fill, scale_outline, sizeof(scale_fill)));

    /* Geometry scale up */
    // TODO: allow at least a quad geometry (need to identify its gravity center for the scaling anchor)
    const float x = -1.f;
    const float y = -1.f;
    const float w = 2.f;
    const float h = 2.f;
    const float nw = w * scale_fill[0];
    const float nh = h * scale_fill[1];
    const float offx = (w - nw) / 2.f;
    const float offy = (h - nh) / 2.f;
    const NGLI_ALIGNED_MAT(ref) = {
        nw, 0, 0, 0,
        0, nh, 0, 0,
        x+offx, y+offy, 0, 0,
        0, 0, 0, 1,
    };
    memcpy(s->transform, ref, sizeof(s->transform));

    return 0;
}

static int init_desc(struct ngl_node *node, struct renderpath_priv *s,
                     const struct pgcraft_uniform *uniforms, size_t nb_uniforms)
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
    for (size_t i = 0; i < nb_uniforms; i++)
        if (!ngli_darray_push(&desc->uniforms, &uniforms[i]))
            return NGL_ERROR_MEMORY;

    return 0;
}

// TODO factor out with renderother and pass
static int build_uniforms_map(struct pipeline_desc *desc)
{
    const struct pgcraft_uniform *uniforms = ngli_darray_data(&desc->uniforms);
    for (size_t i = 0; i < ngli_darray_count(&desc->uniforms); i++) {
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
                             struct renderpath_priv *s, const struct renderpath_opts *o,
                             const struct pgcraft_params *crafter_params)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[rnode->id];

    struct graphics_state state = rnode->graphics_state;
    int ret = ngli_blending_apply_preset(&state, NGLI_BLENDING_SRC_OVER);
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

    const struct pipeline_compat_resources pipeline_resources = ngli_pgcraft_get_pipeline_resources(desc->crafter);
    const struct pgcraft_compat_info *compat_info = ngli_pgcraft_get_compat_info(desc->crafter);

    const struct pipeline_compat_params params = {
        .type = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics = {
            .topology     = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state        = state,
            .rt_layout    = rnode->rendertarget_layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(desc->crafter),
        },
        .program     = ngli_pgcraft_get_program(desc->crafter),
        .layout      = ngli_pgcraft_get_pipeline_layout(desc->crafter),
        .resources   = &pipeline_resources,
        .compat_info = compat_info,
    };

    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    ret = build_uniforms_map(desc);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index  = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix",  NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->transform_index         = ngli_pgcraft_get_uniform_index(desc->crafter, "transform",         NGLI_PROGRAM_SHADER_VERT);

    desc->coords_fill_index    = ngli_pgcraft_get_uniform_index(desc->crafter, "coords_fill",    NGLI_PROGRAM_SHADER_FRAG);
    desc->coords_outline_index = ngli_pgcraft_get_uniform_index(desc->crafter, "coords_outline", NGLI_PROGRAM_SHADER_FRAG);

    return 0;
}

static int renderpath_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct renderpath_priv *s = node->priv_data;
    struct renderpath_opts *o = node->opts;

    const struct pgcraft_uniform uniforms[] = {
        {.name="modelview_matrix",  .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},
        {.name="transform",         .type=NGLI_TYPE_MAT4,  .stage=NGLI_PROGRAM_SHADER_VERT},

        {.name="debug",             .type=NGLI_TYPE_BOOL,  .stage=NGLI_PROGRAM_SHADER_FRAG},
        {.name="coords_fill",       .type=NGLI_TYPE_VEC4,  .stage=NGLI_PROGRAM_SHADER_FRAG},
        {.name="coords_outline",    .type=NGLI_TYPE_VEC4,  .stage=NGLI_PROGRAM_SHADER_FRAG},

        {.name="color",             .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_node,         o->color)},
        {.name="opacity",           .type=NGLI_TYPE_F32,   .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_node,       &o->opacity)},
        {.name="outline",           .type=NGLI_TYPE_F32,   .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->outline_node,       &o->outline)},
        {.name="outline_color",     .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->outline_color_node, &o->outline_color)},
        {.name="glow",              .type=NGLI_TYPE_F32,   .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->glow_node,          &o->glow)},
        {.name="glow_color",        .type=NGLI_TYPE_VEC3,  .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->glow_color_node,    o->glow_color)},
        {.name="blur",              .type=NGLI_TYPE_F32,   .stage=NGLI_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->blur_node,          &o->blur)},
    };

    int ret = init_desc(node, s, uniforms, NGLI_ARRAY_NB(uniforms));
    if (ret < 0)
        return ret;

    struct texture *texture = ngli_distmap_get_texture(s->distmap);
    const struct pgcraft_texture textures[] = {
        {.name="tex", .type=NGLI_PGCRAFT_SHADER_TEX_TYPE_2D, .stage=NGLI_PROGRAM_SHADER_FRAG, .texture=texture},
    };

    static const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGLI_TYPE_VEC2},
    };

    const struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/path",
        .vert_base        = path_vert,
        .frag_base        = path_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .uniforms         = ngli_darray_data(&desc->uniforms),
        .nb_uniforms      = ngli_darray_count(&desc->uniforms),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    return finalize_pipeline(node, s, o, &crafter_params);
}

static void renderpath_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct renderpath_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline_compat *pl_compat = desc->pipeline_compat;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, desc->projection_matrix_index, projection_matrix);
    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, desc->transform_index, s->transform);

    const struct texture *texture = ngli_distmap_get_texture(s->distmap);
    const float atlas_coords_fill[] = {
        (float)s->atlas_coords_fill[0] / (float)texture->params.width,
        (float)s->atlas_coords_fill[1] / (float)texture->params.height,
        (float)s->atlas_coords_fill[2] / (float)texture->params.width,
        (float)s->atlas_coords_fill[3] / (float)texture->params.height,
    };
    const float atlas_coords_outline[] = {
        (float)s->atlas_coords_outline[0] / (float)texture->params.width,
        (float)s->atlas_coords_outline[1] / (float)texture->params.height,
        (float)s->atlas_coords_outline[2] / (float)texture->params.width,
        (float)s->atlas_coords_outline[3] / (float)texture->params.height,
    };

    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, desc->coords_fill_index,    atlas_coords_fill);
    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, desc->coords_outline_index, atlas_coords_outline);

    const struct uniform_map *map = ngli_darray_data(&desc->uniforms_map);
    for (size_t i = 0; i < ngli_darray_count(&desc->uniforms_map); i++)
        ngli_pipeline_compat_update_uniform(pl_compat, map[i].index, map[i].data);

    if (!ctx->render_pass_started) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->render_pass_started = 1;
    }

    ngli_pipeline_compat_draw(desc->pipeline_compat, 4, 1);
}

static void renderpath_uninit(struct ngl_node *node)
{
    struct renderpath_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pgcraft_freep(&desc->crafter);
        ngli_pipeline_compat_freep(&desc->pipeline_compat);
        ngli_darray_reset(&desc->uniforms);
        ngli_darray_reset(&desc->uniforms_map);
    }
    ngli_distmap_freep(&s->distmap);
    ngli_path_freep(&s->path);
    ngli_darray_reset(&s->pipeline_descs);
}

const struct node_class ngli_renderpath_class = {
    .id        = NGL_NODE_RENDERPATH,
    .name      = "RenderPath",
    .init      = renderpath_init,
    .prepare   = renderpath_prepare,
    .update    = ngli_node_update_children,
    .draw      = renderpath_draw,
    .uninit    = renderpath_uninit,
    .opts_size = sizeof(struct renderpath_opts),
    .priv_size = sizeof(struct renderpath_priv),
    .params    = renderpath_params,
    .file      = __FILE__,
};
