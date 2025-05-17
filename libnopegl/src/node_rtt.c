/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2016-2022 GoPro Inc.
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
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "ngpu/format.h"
#include "ngpu/graphics_state.h"
#include "ngpu/rendertarget.h"
#include "node_graphicconfig.h"
#include "node_rtt.h"
#include "node_texture.h"
#include "node_textureview.h"
#include "nopegl.h"
#include "rtt.h"
#include "utils/utils.h"

struct rtt_opts {
    struct ngl_node *child;
    struct ngl_node **color_textures;
    size_t nb_color_textures;
    struct ngl_node *depth_texture;
    uint32_t samples;
    float clear_color[4];
    int forward_transforms;
};

struct rtt_priv {
    struct renderpass_info renderpass_info;
    uint32_t width;
    uint32_t height;
    int resizable;

    struct ngpu_rendertarget_layout layout;
    struct rtt_params rtt_params;
    struct rtt_ctx *rtt_ctx;
};

#define OFFSET(x) offsetof(struct rtt_opts, x)
static const struct node_param rtt_params[] = {
    {"child",         NGLI_PARAM_TYPE_NODE, OFFSET(child),
                      .flags=NGLI_PARAM_FLAG_NON_NULL,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_textures` and optionally to `depth_texture`")},
    {"color_textures", NGLI_PARAM_TYPE_NODELIST, OFFSET(color_textures),
                      .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURE2DARRAY, NGL_NODE_TEXTURE3D, NGL_NODE_TEXTURECUBE, NGL_NODE_TEXTUREVIEW, NGLI_NODE_NONE},
                      .desc=NGLI_DOCSTRING("destination color texture")},
    {"depth_texture", NGLI_PARAM_TYPE_NODE, OFFSET(depth_texture),
                      .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTUREVIEW, NGLI_NODE_NONE},
                      .desc=NGLI_DOCSTRING("destination depth (and potentially combined stencil) texture")},
    {"samples",       NGLI_PARAM_TYPE_I32, OFFSET(samples),
                      .desc=NGLI_DOCSTRING("number of samples used for multisampling anti-aliasing")},
    {"clear_color",   NGLI_PARAM_TYPE_VEC4, OFFSET(clear_color),
                      .desc=NGLI_DOCSTRING("color used to clear the `color_texture`")},
    {"forward_transforms", NGLI_PARAM_TYPE_BOOL, OFFSET(forward_transforms), {.i32=0},
                           .desc=NGLI_DOCSTRING("enable forwarding of camera/model transformations")},
    {NULL}
};

struct rtt_texture_info {
    struct ngl_node *node;
    struct texture_info *info;
    uint32_t layer_base;
    uint32_t layer_count;
};

static struct rtt_texture_info get_rtt_texture_info(struct ngl_node *node)
{
    if (node->cls->id == NGL_NODE_TEXTUREVIEW) {
        const struct textureview_opts *textureview_opts = node->opts;
        struct ngl_node *texture = textureview_opts->texture;
        struct texture_info *texture_info = texture->priv_data;
        const struct rtt_texture_info rtt_texture_info = {
            .node = texture,
            .info = texture_info,
            .layer_base = textureview_opts->layer,
            .layer_count = 1,
        };
        return rtt_texture_info;
    } else {
        struct texture_info *texture_info = node->priv_data;
        const struct ngpu_texture_params *texture_params = &texture_info->params;
        uint32_t layer_count = 1;
        if (node->cls->id == NGL_NODE_TEXTURECUBE)
            layer_count = 6;
        else if (node->cls->id == NGL_NODE_TEXTURE3D)
            layer_count = texture_params->depth;
        else if (node->cls->id == NGL_NODE_TEXTURE2DARRAY)
            layer_count = texture_params->depth;
        const struct rtt_texture_info rtt_texture_info = {
            .node = node,
            .info = texture_info,
            .layer_base = 0,
            .layer_count = layer_count,
        };
        return rtt_texture_info;
    }
}

static int rtt_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct ngpu_limits *limits = &gpu_ctx->limits;
    const struct rtt_opts *o = node->opts;
    struct rtt_priv *s = node->priv_data;

    if (!o->nb_color_textures) {
        LOG(ERROR, "at least one color texture must be specified");
        return NGL_ERROR_INVALID_ARG;
    }

    ngli_node_get_renderpass_info(o->child, &s->renderpass_info);
#if DEBUG_SCENE
    if (s->renderpass_info.nb_interruptions) {
        LOG(WARNING, "the underlying render pass might not be optimal as it contains a rtt or compute node in the middle of it");
    }
#endif

    s->layout.samples = o->samples;

    size_t nb_color_attachments = 0;
    for (size_t i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info texture_info = get_rtt_texture_info(o->color_textures[i]);

        nb_color_attachments += texture_info.layer_count;

        if (ngli_node_texture_has_media_data_src(texture_info.node)) {
            LOG(ERROR, "render targets cannot have a data source");
            return NGL_ERROR_INVALID_ARG;
        }

        struct ngpu_texture_params *params = &texture_info.info->params;
        if (i == 0) {
            s->width = params->width;
            s->height = params->height;
            s->resizable = (s->width == 0 && s->height == 0);
        } else if (s->width != params->width || s->height != params->height) {
            LOG(ERROR, "all color texture dimensions do not match: %dx%d != %dx%d",
            s->width, s->height, params->width, params->height);
            return NGL_ERROR_INVALID_ARG;
        }

        params->usage |= NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
        for (int32_t j = 0; j < texture_info.layer_count; j++) {
            s->layout.colors[s->layout.nb_colors].format = params->format;
            s->layout.colors[s->layout.nb_colors].resolve = o->samples > 1;
            s->layout.nb_colors++;
        }
    }

    if (nb_color_attachments > limits->max_color_attachments) {
        LOG(ERROR, "context does not support more than %d color attachments", limits->max_color_attachments);
        return NGL_ERROR_UNSUPPORTED;
    }

    if (o->depth_texture) {
        const struct rtt_texture_info texture_info = get_rtt_texture_info(o->depth_texture);

        if (ngli_node_texture_has_media_data_src(texture_info.node)) {
            LOG(ERROR, "render targets cannot have a data source");
            return NGL_ERROR_INVALID_ARG;
        }

        struct ngpu_texture_params *params = &texture_info.info->params;
        if (s->width != params->width || s->height != params->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, params->width, params->height);
            return NGL_ERROR_INVALID_ARG;
        }

        if (!(gpu_ctx->features & NGPU_FEATURE_DEPTH_STENCIL_RESOLVE) && o->samples > 0) {
            LOG(ERROR, "context does not support resolving depth/stencil attachments");
            return NGL_ERROR_GRAPHICS_UNSUPPORTED;
        }

        params->usage |= NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        s->layout.depth_stencil.format = params->format;
        s->layout.depth_stencil.resolve = o->samples > 1;
    } else {
        enum ngpu_format depth_format = NGPU_FORMAT_UNDEFINED;
        if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_STENCIL)
            depth_format = ngpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
        else if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_DEPTH)
            depth_format = ngpu_ctx_get_preferred_depth_format(gpu_ctx);
        s->layout.depth_stencil.format = depth_format;
    }

    return 0;
}

enum {
    RENDER_PASS_STATE_NONE,
    RENDER_PASS_STATE_STARTED,
    RENDER_PASS_STATE_STOPPED,
};

static int get_renderpass_info(const struct ngl_node *node, int state, struct renderpass_info *info)
{
    const struct ngl_node **children = ngli_darray_data(&node->children);
    for (size_t i = 0; i < ngli_darray_count(&node->children); i++) {
        const struct ngl_node *child = children[i];
        if (child->cls->id == NGL_NODE_RENDERTOTEXTURE ||
            child->cls->id == NGL_NODE_COMPUTE) {
            if (state == RENDER_PASS_STATE_STARTED)
                state = RENDER_PASS_STATE_STOPPED;
        } else if (child->cls->id == NGL_NODE_TEXTURE2D) {
            struct texture_info *texture_info = child->priv_data;
            if (texture_info->rtt && state == RENDER_PASS_STATE_STARTED)
                state = RENDER_PASS_STATE_STOPPED;
        } else if (child->cls->category == NGLI_NODE_CATEGORY_DRAW) {
            state = get_renderpass_info(child, state, info);
            if (state == RENDER_PASS_STATE_STOPPED)
                info->nb_interruptions++;
            state = RENDER_PASS_STATE_STARTED;
        } else if (child->cls->id == NGL_NODE_GRAPHICCONFIG) {
            struct ngpu_graphics_state graphics_state = {0};
            ngli_node_graphicconfig_get_state(child, &graphics_state);
            if (graphics_state.depth_test)
                info->features |= NGLI_RENDERPASS_FEATURE_DEPTH;
            if (graphics_state.stencil_test)
                info->features |= NGLI_RENDERPASS_FEATURE_STENCIL;
            state = get_renderpass_info(child, state, info);
        } else {
            state = get_renderpass_info(child, state, info);
        }
    }
    return state;
}

void ngli_node_get_renderpass_info(const struct ngl_node *node, struct renderpass_info *info)
{
    get_renderpass_info(node, RENDER_PASS_STATE_NONE, info);
}

static int rtt_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct rtt_priv *s = node->priv_data;

    rnode->rendertarget_layout = s->layout;
    return ngli_node_prepare_children(node);
}

static int rtt_prefetch(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rtt_priv *s = node->priv_data;
    const struct rtt_opts *o = node->opts;

    s->rtt_params = (struct rtt_params) {
        .width = s->width,
        .height = s->height,
        .samples = o->samples,
        .nb_interruptions = s->renderpass_info.nb_interruptions,
    };

    for (size_t i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info rtt_texture_info = get_rtt_texture_info(o->color_textures[i]);
        struct texture_info *texture_info = rtt_texture_info.info;
        struct ngpu_texture *texture = texture_info->texture;
        const uint32_t layer_end = rtt_texture_info.layer_base + rtt_texture_info.layer_count;
        for (uint32_t j = rtt_texture_info.layer_base; j < layer_end; j++) {
            s->rtt_params.colors[s->rtt_params.nb_colors++] = (struct ngpu_attachment) {
                .attachment       = texture,
                .attachment_layer = j,
                .load_op          = NGPU_LOAD_OP_CLEAR,
                .clear_value      = {NGLI_ARG_VEC4(o->clear_color)},
                .store_op         = NGPU_STORE_OP_STORE,
            };
        }
        /* Transform the color textures coordinates so it matches how the
         * graphics context uv coordinate system works */
        struct image *image = &texture_info->image;
        ngpu_ctx_get_rendertarget_uvcoord_matrix(gpu_ctx, image->coordinates_matrix);
    }

    enum ngpu_format depth_format = NGPU_FORMAT_UNDEFINED;
    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        struct texture_info *depth_texture_info = info.info;
        struct ngpu_texture *texture = depth_texture_info->texture;
        s->rtt_params.depth_stencil = (struct ngpu_attachment) {
            .attachment       = texture,
            .attachment_layer = info.layer_base,
            .load_op          = NGPU_LOAD_OP_CLEAR,
            .store_op         = NGPU_STORE_OP_STORE,
        };
        /* Transform the depth texture coordinates so it matches how the
         * graphics context uv coordinate system works */
        struct image *depth_image = &depth_texture_info->image;
        ngpu_ctx_get_rendertarget_uvcoord_matrix(gpu_ctx, depth_image->coordinates_matrix);
    } else {
        if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_STENCIL)
            depth_format = ngpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
        else if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_DEPTH)
            depth_format = ngpu_ctx_get_preferred_depth_format(gpu_ctx);
        s->rtt_params.depth_stencil_format = depth_format;
    }

    if (s->resizable)
        return 0;

    s->rtt_ctx = ngli_rtt_create(node->ctx);
    if (!s->rtt_ctx)
        return NGL_ERROR_MEMORY;

    ret = ngli_rtt_init(s->rtt_ctx, &s->rtt_params);
    if (ret < 0)
        return ret;

    return 0;
}

static int rtt_resize(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct rtt_priv *s = node->priv_data;
    struct rtt_opts *o = node->opts;

    const uint32_t width = ctx->current_rendertarget->width;
    const uint32_t height = ctx->current_rendertarget->height;
    if (s->rtt_ctx) {
        uint32_t current_width, current_height;
        ngli_rtt_get_dimensions(s->rtt_ctx, &current_width, &current_height);
        if (current_width == width && current_height == height)
            return 0;
    }

    struct ngpu_texture *textures[NGPU_MAX_COLOR_ATTACHMENTS] = {NULL};
    struct ngpu_texture *depth_texture = NULL;
    struct rtt_ctx *rtt_ctx = NULL;

    for (size_t i = 0; i < o->nb_color_textures; i++) {
        textures[i] = ngpu_texture_create(ctx->gpu_ctx);
        if (!textures[i]) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        struct ngpu_texture_params texture_params = info.info->params;
        texture_params.width = width;
        texture_params.height = height;

        ret = ngpu_texture_init(textures[i], &texture_params);
        if (ret < 0)
            goto fail;
    }

    if (o->depth_texture) {
        depth_texture = ngpu_texture_create(ctx->gpu_ctx);
        if (!depth_texture) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        struct ngpu_texture_params texture_params = info.info->params;
        texture_params.width = width;
        texture_params.height = height;

        ret = ngpu_texture_init(depth_texture, &texture_params);
        if (ret < 0)
            goto fail;
    }

    rtt_ctx = ngli_rtt_create(ctx);
    if (!rtt_ctx) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    struct rtt_params params = s->rtt_params;
    params.width  = width;
    params.height = height;

    for (size_t i = 0; i < o->nb_color_textures; i++)
        params.colors[i].attachment = textures[i];
    params.depth_stencil.attachment = depth_texture;

    ret = ngli_rtt_init(rtt_ctx, &params);
    if (ret < 0)
        goto fail;

    ngli_rtt_freep(&s->rtt_ctx);

    s->width = width;
    s->height = height;
    s->rtt_params = params;
    s->rtt_ctx = rtt_ctx;

    for (size_t i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        struct texture_info *texture_info = info.info;
        ngpu_texture_freep(&texture_info->texture);
        texture_info->texture = textures[i];
        texture_info->image.params.width = width;
        texture_info->image.params.height = height;
        texture_info->image.planes[0] = textures[i];
        texture_info->image.rev = texture_info->image_rev++;
    }

    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        struct texture_info *texture_info = info.info;
        ngpu_texture_freep(&texture_info->texture);
        texture_info->texture = depth_texture;
        texture_info->image.params.width = width;
        texture_info->image.params.height = height;
        texture_info->image.planes[0] = depth_texture;
        texture_info->image.rev = texture_info->image_rev++;
    }

    return 0;

fail:
    for (size_t i = 0; i < o->nb_color_textures; i++)
        ngpu_texture_freep(&textures[i]);
    ngpu_texture_freep(&depth_texture);
    ngli_rtt_freep(&rtt_ctx);

    LOG(ERROR, "failed to resize rtt: %dx%d", width, height);
    return ret;
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rtt_priv *s = node->priv_data;
    const struct rtt_opts *o = node->opts;

    if (s->resizable) {
        int ret = rtt_resize(node);
        if (ret < 0)
            return;
    }

    if (!o->forward_transforms) {
        if (!ngli_darray_push(&ctx->modelview_matrix_stack, ctx->default_modelview_matrix) ||
            !ngli_darray_push(&ctx->projection_matrix_stack, ctx->default_projection_matrix))
            return;
    }

    ngli_rtt_begin(s->rtt_ctx);
    ngli_node_draw(o->child);
    ngli_rtt_end(s->rtt_ctx);

    if (!o->forward_transforms) {
        ngli_darray_pop(&ctx->modelview_matrix_stack);
        ngli_darray_pop(&ctx->projection_matrix_stack);
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;
    ngli_rtt_freep(&s->rtt_ctx);
}

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RENDERTOTEXTURE,
    .name      = "RenderToTexture",
    .init      = rtt_init,
    .prepare   = rtt_prepare,
    .prefetch  = rtt_prefetch,
    .update    = ngli_node_update_children,
    .draw      = rtt_draw,
    .release   = rtt_release,
    .opts_size = sizeof(struct rtt_opts),
    .priv_size = sizeof(struct rtt_priv),
    .params    = rtt_params,
    .file      = __FILE__,
};
