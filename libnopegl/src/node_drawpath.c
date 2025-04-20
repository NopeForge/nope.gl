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
#include "box.h"
#include "distmap.h"
#include "internal.h"
#include "ngpu/ctx.h"
#include "ngpu/texture.h"
#include "ngpu/type.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "path.h"
#include "pipeline_compat.h"
#include "ngpu/pgcraft.h"
#include "utils/utils.h"

/* GLSL fragments as string */
#include "path_frag.h"
#include "path_vert.h"

struct uniform_map {
    int index;
    const void *data;
};

struct pipeline_desc {
    struct pipeline_compat *pipeline_compat;
};

struct drawpath_opts {
    struct ngl_node *path_node;
    float box[4];
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

struct drawpath_priv {
    int32_t atlas_coords_fill[4];
    int32_t atlas_coords_outline[4];
    float transform[4];
    struct distmap *distmap;
    struct path *path;
    struct darray uniforms_map; // struct uniform_map
    struct darray uniforms; // struct pgcraft_uniform
    struct ngpu_pgcraft *crafter;
    int modelview_matrix_index;
    int projection_matrix_index;
    int transform_index;
    int coords_fill_index;
    int coords_outline_index;
    struct darray pipeline_descs;
};

#define OFFSET(x) offsetof(struct drawpath_opts, x)
static const struct node_param drawpath_params[] = {
    {"path",         NGLI_PARAM_TYPE_NODE, OFFSET(path_node),
                     .node_types=(const uint32_t[]){NGL_NODE_PATH, NGL_NODE_SMOOTHPATH, NGLI_NODE_NONE},
                     .flags=NGLI_PARAM_FLAG_NON_NULL,
                     .desc=NGLI_DOCSTRING("path to draw")},
    {"box",          NGLI_PARAM_TYPE_VEC4, OFFSET(box), {.vec={-1.f, -1.f, 2.f, 2.f}},
                     .desc=NGLI_DOCSTRING("geometry box relative to screen (x, y, width, height)")},
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

// TODO factor out with drawother and pass
static int build_uniforms_map(struct drawpath_priv *s)
{
    ngli_darray_init(&s->uniforms_map, sizeof(struct uniform_map), 0);

    const struct ngpu_pgcraft_uniform *uniforms = ngli_darray_data(&s->uniforms);
    for (size_t i = 0; i < ngli_darray_count(&s->uniforms); i++) {
        const struct ngpu_pgcraft_uniform *uniform = &uniforms[i];
        const int index = ngpu_pgcraft_get_uniform_index(s->crafter, uniform->name, uniform->stage);

        /* The following can happen if the driver makes optimisation (MESA is
         * typically able to optimize several passes of the same filter) */
        if (index < 0)
            continue;

        /* This skips unwanted uniforms such as modelview and projection which
         * are handled separately */
        if (!uniform->data)
            continue;

        const struct uniform_map map = {.index=index, .data=uniform->data};
        if (!ngli_darray_push(&s->uniforms_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int drawpath_init(struct ngl_node *node)
{
    struct drawpath_priv *s = node->priv_data;
    struct drawpath_opts *o = node->opts;

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
    const struct ngli_box vb = {NGLI_ARG_VEC4(o->viewbox)};
    const NGLI_ALIGNED_MAT(path_transform) = {
        res/vb.w, 0.f, 0.f, 0.f,
        0.f, res/vb.h, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        -vb.x/vb.w*res, -vb.y/vb.h*res, 0.f, 1.f,
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
    const struct ngli_box box = {NGLI_ARG_VEC4(o->box)};
    const float nw = box.w * scale_fill[0];
    const float nh = box.h * scale_fill[1];
    const float offx = (box.w - nw) / 2.f;
    const float offy = (box.h - nh) / 2.f;
    const float ref[] = {box.x + offx, box.y + offy, nw, nh};
    memcpy(s->transform, ref, sizeof(s->transform));

    const struct ngpu_pgcraft_uniform uniforms[] = {
        {.name="modelview_matrix",  .type=NGPU_TYPE_MAT4,  .stage=NGPU_PROGRAM_SHADER_VERT},
        {.name="projection_matrix", .type=NGPU_TYPE_MAT4,  .stage=NGPU_PROGRAM_SHADER_VERT},
        {.name="transform",         .type=NGPU_TYPE_VEC4,  .stage=NGPU_PROGRAM_SHADER_VERT},

        {.name="debug",             .type=NGPU_TYPE_BOOL,  .stage=NGPU_PROGRAM_SHADER_FRAG},
        {.name="coords_fill",       .type=NGPU_TYPE_VEC4,  .stage=NGPU_PROGRAM_SHADER_FRAG},
        {.name="coords_outline",    .type=NGPU_TYPE_VEC4,  .stage=NGPU_PROGRAM_SHADER_FRAG},

        {.name="color",             .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->color_node, o->color)},
        {.name="opacity",           .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->opacity_node, &o->opacity)},
        {.name="outline",           .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->outline_node, &o->outline)},
        {.name="outline_color",     .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->outline_color_node, &o->outline_color)},
        {.name="glow",              .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->glow_node, &o->glow)},
        {.name="glow_color",        .type=NGPU_TYPE_VEC3,  .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->glow_color_node, o->glow_color)},
        {.name="blur",              .type=NGPU_TYPE_F32,   .stage=NGPU_PROGRAM_SHADER_FRAG, .data=ngli_node_get_data_ptr(o->blur_node, &o->blur)},
    };

    /* register source uniforms */
    ngli_darray_init(&s->uniforms, sizeof(struct ngpu_pgcraft_uniform), 0);
    for (size_t i = 0; i < NGLI_ARRAY_NB(uniforms); i++)
        if (!ngli_darray_push(&s->uniforms, &uniforms[i]))
            return NGL_ERROR_MEMORY;

    struct ngpu_texture *texture = ngli_distmap_get_texture(s->distmap);
    const struct ngpu_pgcraft_texture textures[] = {
        {
            .name = "tex",
            .type = NGPU_PGCRAFT_TEXTURE_TYPE_2D,
            .stage = NGPU_PROGRAM_SHADER_FRAG,
            .texture = texture,
        },
    };

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {
            .name = "uv",
            .type = NGPU_TYPE_VEC2,
        },
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/path",
        .vert_base        = path_vert,
        .frag_base        = path_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .uniforms         = ngli_darray_data(&s->uniforms),
        .nb_uniforms      = ngli_darray_count(&s->uniforms),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    struct ngl_ctx *ctx = node->ctx;
    s->crafter = ngpu_pgcraft_create(ctx->gpu_ctx);
    if (!s->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngpu_pgcraft_craft(s->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    s->modelview_matrix_index  = ngpu_pgcraft_get_uniform_index(s->crafter, "modelview_matrix", NGPU_PROGRAM_SHADER_VERT);
    s->projection_matrix_index = ngpu_pgcraft_get_uniform_index(s->crafter, "projection_matrix", NGPU_PROGRAM_SHADER_VERT);
    s->transform_index         = ngpu_pgcraft_get_uniform_index(s->crafter, "transform", NGPU_PROGRAM_SHADER_VERT);

    s->coords_fill_index    = ngpu_pgcraft_get_uniform_index(s->crafter, "coords_fill", NGPU_PROGRAM_SHADER_FRAG);
    s->coords_outline_index = ngpu_pgcraft_get_uniform_index(s->crafter, "coords_outline", NGPU_PROGRAM_SHADER_FRAG);

    ret = build_uniforms_map(s);
    if (ret < 0)
        return ret;

    return 0;
}

static int drawpath_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct drawpath_priv *s = node->priv_data;


    struct rnode *rnode = node->ctx->rnode_pos;

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    rnode->id = ngli_darray_count(&s->pipeline_descs) - 1;

    struct ngpu_graphics_state state = rnode->graphics_state;
    int ret = ngli_blending_apply_preset(&state, NGLI_BLENDING_SRC_OVER);
    if (ret < 0)
        return ret;

    desc->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!desc->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics = {
            .topology     = NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
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

    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    return 0;
}

static void drawpath_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct drawpath_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline_compat *pl_compat = desc->pipeline_compat;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, s->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, s->projection_matrix_index, projection_matrix);
    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, s->transform_index, s->transform);

    const struct ngpu_texture *texture = ngli_distmap_get_texture(s->distmap);
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

    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, s->coords_fill_index,    atlas_coords_fill);
    ngli_pipeline_compat_update_uniform(desc->pipeline_compat, s->coords_outline_index, atlas_coords_outline);

    const struct uniform_map *map = ngli_darray_data(&s->uniforms_map);
    for (size_t i = 0; i < ngli_darray_count(&s->uniforms_map); i++)
        ngli_pipeline_compat_update_uniform(pl_compat, map[i].index, map[i].data);

    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    if (!ngpu_ctx_is_render_pass_active(gpu_ctx)) {
        ngpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    }

    ngpu_ctx_set_viewport(gpu_ctx, &ctx->viewport);
    ngpu_ctx_set_scissor(gpu_ctx, &ctx->scissor);

    ngli_pipeline_compat_draw(desc->pipeline_compat, 4, 1, 0);
}

static void drawpath_uninit(struct ngl_node *node)
{
    struct drawpath_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_compat_freep(&desc->pipeline_compat);
    }
    ngli_darray_reset(&s->uniforms);
    ngli_darray_reset(&s->uniforms_map);
    ngpu_pgcraft_freep(&s->crafter);
    ngli_distmap_freep(&s->distmap);
    ngli_path_freep(&s->path);
    ngli_darray_reset(&s->pipeline_descs);
}

const struct node_class ngli_drawpath_class = {
    .id        = NGL_NODE_DRAWPATH,
    .name      = "DrawPath",
    .init      = drawpath_init,
    .prepare   = drawpath_prepare,
    .update    = ngli_node_update_children,
    .draw      = drawpath_draw,
    .uninit    = drawpath_uninit,
    .opts_size = sizeof(struct drawpath_opts),
    .priv_size = sizeof(struct drawpath_priv),
    .params    = drawpath_params,
    .file      = __FILE__,
};
