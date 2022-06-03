/*
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
#include "rendertarget.h"
#include "format.h"
#include "gpu_ctx.h"
#include "log.h"
#include "nodegl.h"
#include "internal.h"
#include "utils.h"

struct renderpass_info {
    int nb_interruptions;
};

struct rtt_opts {
    struct ngl_node *child;
    struct ngl_node **color_textures;
    int nb_color_textures;
    struct ngl_node *depth_texture;
    int samples;
    float clear_color[4];
    int features;
};

struct rtt_priv {
    struct renderpass_info renderpass_info;
    int width;
    int height;

    struct rendertarget *rt;
    struct rendertarget *rt_resume;
    struct rendertarget *available_rendertargets[2];
    struct texture *depth;

    struct texture *ms_colors[NGLI_MAX_COLOR_ATTACHMENTS];
    int nb_ms_colors;
    struct texture *ms_depth;
};

#define FEATURE_DEPTH       (1 << 0)
#define FEATURE_STENCIL     (1 << 1)

static const struct param_choices feature_choices = {
    .name = "framebuffer_features",
    .consts = {
        {"depth",   FEATURE_DEPTH,   .desc=NGLI_DOCSTRING("add depth buffer")},
        {"stencil", FEATURE_STENCIL, .desc=NGLI_DOCSTRING("add stencil buffer")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct rtt_opts, x)
static const struct node_param rtt_params[] = {
    {"child",         NGLI_PARAM_TYPE_NODE, OFFSET(child),
                      .flags=NGLI_PARAM_FLAG_NON_NULL,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_textures` and optionally to `depth_texture`")},
    {"color_textures", NGLI_PARAM_TYPE_NODELIST, OFFSET(color_textures),
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURECUBE, NGL_NODE_TEXTUREVIEW, -1},
                      .desc=NGLI_DOCSTRING("destination color texture")},
    {"depth_texture", NGLI_PARAM_TYPE_NODE, OFFSET(depth_texture),
                      .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTUREVIEW, -1},
                      .desc=NGLI_DOCSTRING("destination depth (and potentially combined stencil) texture")},
    {"samples",       NGLI_PARAM_TYPE_I32, OFFSET(samples),
                      .desc=NGLI_DOCSTRING("number of samples used for multisampling anti-aliasing")},
    {"clear_color",   NGLI_PARAM_TYPE_VEC4, OFFSET(clear_color),
                      .desc=NGLI_DOCSTRING("color used to clear the `color_texture`")},
    {"features",      NGLI_PARAM_TYPE_FLAGS, OFFSET(features),
                      .choices=&feature_choices,
                      .desc=NGLI_DOCSTRING("framebuffer feature mask")},
    {NULL}
};

struct rtt_texture_info {
    struct texture_priv *texture_priv;
    const struct texture_opts *texture_opts;
    int layer_base;
    int layer_count;
};

static struct rtt_texture_info get_rtt_texture_info(struct ngl_node *node)
{
    if (node->cls->id == NGL_NODE_TEXTUREVIEW) {
        const struct textureview_opts *textureview_opts = node->opts;
        struct texture_priv *texture_priv = textureview_opts->texture->priv_data;
        const struct texture_opts *texture_opts = textureview_opts->texture->opts;
        const struct rtt_texture_info info = {
            .texture_priv = texture_priv,
            .texture_opts = texture_opts,
            .layer_base = textureview_opts->layer,
            .layer_count = 1,
        };
        return info;
    } else {
        struct texture_priv *texture_priv = node->priv_data;
        const struct texture_opts *texture_opts = node->opts;
        const struct rtt_texture_info info = {
            .texture_priv = texture_priv,
            .texture_opts = texture_opts,
            .layer_base = 0,
            .layer_count = node->cls->id == NGL_NODE_TEXTURECUBE ? 6 : 1,
        };
        return info;
    }
}

static int rtt_init(struct ngl_node *node)
{
    const struct rtt_opts *o = node->opts;

    for (int i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        const struct texture_opts *texture_opts = info.texture_opts;
        if (texture_opts->data_src) {
            LOG(ERROR, "render targets cannot have a data source");
            return NGL_ERROR_INVALID_ARG;
        }
    }

    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        const struct texture_opts *texture_opts = info.texture_opts;
        if (texture_opts->data_src) {
            LOG(ERROR, "render targets cannot have a data source");
            return NGL_ERROR_INVALID_ARG;
        }
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
    for (int i = 0; i < ngli_darray_count(&node->children); i++) {
        const struct ngl_node *child = children[i];
        if (child->cls->id == NGL_NODE_RENDERTOTEXTURE ||
            child->cls->id == NGL_NODE_COMPUTE) {
            if (state == RENDER_PASS_STATE_STARTED)
                state = RENDER_PASS_STATE_STOPPED;
        } else if (child->cls->category == NGLI_NODE_CATEGORY_RENDER) {
            if (state == RENDER_PASS_STATE_STOPPED)
                info->nb_interruptions++;
            state = RENDER_PASS_STATE_STARTED;
        } else {
            state = get_renderpass_info(child, state, info);
        }
    }
    return state;
}

static int rtt_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct rtt_priv *s = node->priv_data;
    const struct rtt_opts *o = node->opts;

    get_renderpass_info(o->child, RENDER_PASS_STATE_NONE, &s->renderpass_info);
#if DEBUG_SCENE
    if (s->renderpass_info.nb_interruptions) {
        LOG(WARNING, "the underlying render pass might not be optimal as it contains a rtt or compute node in the middle of it");
    }
#endif

    struct rendertarget_desc desc = {
        .samples = o->samples,
    };
    for (int i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        struct texture_params *params = &info.texture_priv->params;
        params->usage |= NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
        for (int j = 0; j < info.layer_count; j++) {
            desc.colors[desc.nb_colors].format = params->format;
            desc.colors[desc.nb_colors].resolve = o->samples > 1;
            desc.nb_colors++;
        }
    }
    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        struct texture_params *depth_texture_params = &info.texture_priv->params;
        depth_texture_params->usage |= NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        desc.depth_stencil.format = depth_texture_params->format;
        desc.depth_stencil.resolve = o->samples > 1;
    } else {
        int depth_format = NGLI_FORMAT_UNDEFINED;
        if (o->features & FEATURE_STENCIL)
            depth_format = ngli_gpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
        else if (o->features & FEATURE_DEPTH)
            depth_format = ngli_gpu_ctx_get_preferred_depth_format(gpu_ctx);
        desc.depth_stencil.format = depth_format;
    }
    rnode->rendertarget_desc = desc;

    return ngli_node_prepare_children(node);
}

static int rtt_prefetch(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rtt_priv *s = node->priv_data;
    const int nb_interruptions = s->renderpass_info.nb_interruptions;
    struct rtt_opts *o = node->opts;

    if (!(gpu_ctx->features & NGLI_FEATURE_COLOR_RESOLVE) && o->samples > 0) {
        LOG(WARNING, "context does not support resolving color attachments, "
            "multisample anti-aliasing will be disabled");
        o->samples = 0;
    }

    if (!o->nb_color_textures) {
        LOG(ERROR, "at least one color texture must be specified");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->nb_color_textures > NGLI_MAX_COLOR_ATTACHMENTS) {
        LOG(ERROR, "context does not support more than %d color attachments", NGLI_MAX_COLOR_ATTACHMENTS);
        return NGL_ERROR_UNSUPPORTED;
    }

    for (int i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        const struct texture_priv *texture_priv = info.texture_priv;
        const struct texture *texture = texture_priv->texture;
        const struct texture_params *params = &texture->params;
        if (i == 0) {
            s->width = params->width;
            s->height = params->height;
        } else if (s->width != params->width || s->height != params->height) {
            LOG(ERROR, "all color texture dimensions do not match %dx%d != %dx%d",
            s->width, s->height, params->width, params->height);
            return NGL_ERROR_INVALID_ARG;
        }
    }

    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        const struct texture_priv *depth_texture_priv = info.texture_priv;
        const struct texture *depth_texture = depth_texture_priv->texture;
        const struct texture_params *depth_texture_params = &depth_texture->params;
        if (s->width != depth_texture_params->width || s->height != depth_texture_params->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, depth_texture_params->width, depth_texture_params->height);
            return NGL_ERROR_INVALID_ARG;
        }
        if (!(gpu_ctx->features & NGLI_FEATURE_DEPTH_STENCIL_RESOLVE) && o->samples > 0) {
            LOG(WARNING, "context does not support resolving depth/stencil attachments, "
                "multisample anti-aliasing will be disabled");
            o->samples = 0;
        }
    }

    const int transient_usage = nb_interruptions == 0 ? NGLI_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0;

    struct rendertarget_params rt_params = {
        .width = s->width,
        .height = s->height,
    };

    for (int i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        struct texture_priv *texture_priv = info.texture_priv;
        struct texture *texture = texture_priv->texture;
        struct texture_params *params = &texture->params;
        const int layer_end = info.layer_base + info.layer_count;
        for (int j = info.layer_base; j < layer_end; j++) {
            if (o->samples) {
                struct texture *ms_texture = ngli_texture_create(gpu_ctx);
                if (!ms_texture)
                    return NGL_ERROR_MEMORY;
                s->ms_colors[s->nb_ms_colors++] = ms_texture;
                struct texture_params attachment_params = {
                    .type    = NGLI_TEXTURE_TYPE_2D,
                    .format  = params->format,
                    .width   = s->width,
                    .height  = s->height,
                    .samples = o->samples,
                    .usage   = NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | transient_usage,
                };
                ret = ngli_texture_init(ms_texture, &attachment_params);
                if (ret < 0)
                    return ret;
                rt_params.colors[rt_params.nb_colors].attachment = ms_texture;
                rt_params.colors[rt_params.nb_colors].attachment_layer = 0;
                rt_params.colors[rt_params.nb_colors].resolve_target = texture;
                rt_params.colors[rt_params.nb_colors].resolve_target_layer = j;
                rt_params.colors[rt_params.nb_colors].load_op = NGLI_LOAD_OP_CLEAR;
                float *clear_value = rt_params.colors[rt_params.nb_colors].clear_value;
                memcpy(clear_value, o->clear_color, sizeof(o->clear_color));
                const int store_op = nb_interruptions ? NGLI_STORE_OP_STORE : NGLI_STORE_OP_DONT_CARE;
                rt_params.colors[rt_params.nb_colors].store_op = store_op;
            } else {
                rt_params.colors[rt_params.nb_colors].attachment = texture;
                rt_params.colors[rt_params.nb_colors].attachment_layer = j;
                rt_params.colors[rt_params.nb_colors].load_op = NGLI_LOAD_OP_CLEAR;
                float *clear_value = rt_params.colors[rt_params.nb_colors].clear_value;
                memcpy(clear_value, o->clear_color, sizeof(o->clear_color));
                rt_params.colors[rt_params.nb_colors].store_op = NGLI_STORE_OP_STORE;
            }
            rt_params.nb_colors++;
        }
    }

    int depth_format = NGLI_FORMAT_UNDEFINED;
    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        struct texture_priv *depth_texture_priv = info.texture_priv;
        struct texture *texture = depth_texture_priv->texture;
        struct texture_params *params = &texture->params;

        if (o->samples) {
            struct texture *ms_texture = ngli_texture_create(gpu_ctx);
            if (!ms_texture)
                return NGL_ERROR_MEMORY;
            s->ms_depth = ms_texture;
            struct texture_params attachment_params = {
                .type    = NGLI_TEXTURE_TYPE_2D,
                .format  = params->format,
                .width   = s->width,
                .height  = s->height,
                .samples = o->samples,
                .usage   = NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient_usage,
            };
            ret = ngli_texture_init(ms_texture, &attachment_params);
            if (ret < 0)
                return ret;
            rt_params.depth_stencil.attachment = ms_texture;
            rt_params.depth_stencil.attachment_layer = 0;
            rt_params.depth_stencil.resolve_target = texture;
            rt_params.depth_stencil.resolve_target_layer = info.layer_base;
            rt_params.depth_stencil.load_op = NGLI_LOAD_OP_CLEAR;
            const int store_op = nb_interruptions ? NGLI_STORE_OP_STORE : NGLI_STORE_OP_DONT_CARE;
            rt_params.depth_stencil.store_op = store_op;
        } else {
            rt_params.depth_stencil.attachment = texture;
            rt_params.depth_stencil.attachment_layer = info.layer_base;
            rt_params.depth_stencil.load_op = NGLI_LOAD_OP_CLEAR;
            rt_params.depth_stencil.store_op = NGLI_STORE_OP_STORE;
        }
    } else {
        if (o->features & FEATURE_STENCIL)
            depth_format = ngli_gpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
        else if (o->features & FEATURE_DEPTH)
            depth_format = ngli_gpu_ctx_get_preferred_depth_format(gpu_ctx);

        if (depth_format != NGLI_FORMAT_UNDEFINED) {
            struct texture *depth = ngli_texture_create(gpu_ctx);
            if (!depth)
                return NGL_ERROR_MEMORY;
            s->depth = depth;
            struct texture_params attachment_params = {
                .type    = NGLI_TEXTURE_TYPE_2D,
                .format  = depth_format,
                .width   = s->width,
                .height  = s->height,
                .samples = o->samples,
                .usage   = NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient_usage,
            };
            ret = ngli_texture_init(depth, &attachment_params);
            if (ret < 0)
                return ret;
            rt_params.depth_stencil.attachment = depth;
            rt_params.depth_stencil.load_op = NGLI_LOAD_OP_CLEAR;
            /*
             * For the first rendertarget with load operations set to clear, if
             * the depth attachment is not exposed in the graph (ie: it is not
             * a user supplied texture) and if the renderpass is not
             * interrupted we can discard the depth attachment at the end of
             * the renderpass.
             */
            const int store_op = nb_interruptions ? NGLI_STORE_OP_STORE : NGLI_STORE_OP_DONT_CARE;
            rt_params.depth_stencil.store_op = store_op;
        }
    }

    s->rt = ngli_rendertarget_create(gpu_ctx);
    if (!s->rt)
        return NGL_ERROR_MEMORY;

    ret = ngli_rendertarget_init(s->rt, &rt_params);
    if (ret < 0)
        return ret;

    s->available_rendertargets[0] = s->rt;
    s->available_rendertargets[1] = s->rt;

    if (nb_interruptions) {
        for (int i = 0; i < rt_params.nb_colors; i++)
            rt_params.colors[i].load_op = NGLI_LOAD_OP_LOAD;
        rt_params.depth_stencil.load_op = NGLI_LOAD_OP_LOAD;
        if (o->depth_texture) {
            rt_params.depth_stencil.store_op = NGLI_STORE_OP_STORE;
        } else {
            /*
             * For the second rendertarget with load operations set to load, if
             * the depth attachment is not exposed in the graph (ie: it is not
             * a user supplied texture) and if the renderpass is interrupted
             * *once*, we can discard the depth attachment at the end of the
             * renderpass.
             */
            const int store_op = nb_interruptions > 1 ? NGLI_STORE_OP_STORE : NGLI_STORE_OP_DONT_CARE;
            rt_params.depth_stencil.store_op = store_op;
        }

        s->rt_resume = ngli_rendertarget_create(gpu_ctx);
        if (!s->rt_resume)
            return NGL_ERROR_MEMORY;

        ret = ngli_rendertarget_init(s->rt_resume, &rt_params);
        if (ret < 0)
            return ret;
        s->available_rendertargets[1] = s->rt_resume;
    }

    /* transform the color and depth textures so the coordinates
     * match how the graphics context uv coordinate system works */
    for (int i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        struct texture_priv *texture_priv = info.texture_priv;
        struct image *image = &texture_priv->image;
        ngli_gpu_ctx_get_rendertarget_uvcoord_matrix(gpu_ctx, image->coordinates_matrix);
    }

    if (o->depth_texture) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->depth_texture);
        struct texture_priv *depth_texture_priv = info.texture_priv;
        struct image *depth_image = &depth_texture_priv->image;
        ngli_gpu_ctx_get_rendertarget_uvcoord_matrix(gpu_ctx, depth_image->coordinates_matrix);
    }

    return 0;
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rtt_priv *s = node->priv_data;
    const struct rtt_opts *o = node->opts;

    int prev_vp[4] = {0};
    ngli_gpu_ctx_get_viewport(gpu_ctx, prev_vp);

    const int vp[4] = {0, 0, s->width, s->height};
    ngli_gpu_ctx_set_viewport(gpu_ctx, vp);

    int prev_scissor[4] = {0};
    ngli_gpu_ctx_get_scissor(gpu_ctx, prev_scissor);

    const int scissor[4] = {0, 0, s->width, s->height};
    ngli_gpu_ctx_set_scissor(gpu_ctx, scissor);

    struct rendertarget *prev_rendertargets[2] = {
        ctx->available_rendertargets[0],
        ctx->available_rendertargets[1],
    };

    struct rendertarget *prev_rendertarget = ctx->current_rendertarget;
    if (ctx->render_pass_started) {
        ngli_gpu_ctx_end_render_pass(gpu_ctx);
        ctx->render_pass_started = 0;
        prev_rendertarget = ctx->available_rendertargets[1];
    }

    ctx->available_rendertargets[0] = s->available_rendertargets[0];
    ctx->available_rendertargets[1] = s->available_rendertargets[1];
    ctx->current_rendertarget = s->available_rendertargets[0];

    ngli_node_draw(o->child);

    if (!ctx->render_pass_started) {
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->render_pass_started = 1;
    }
    ngli_gpu_ctx_end_render_pass(gpu_ctx);
    ctx->render_pass_started = 0;

    ctx->current_rendertarget = prev_rendertarget;
    ctx->available_rendertargets[0] = prev_rendertargets[0];
    ctx->available_rendertargets[1] = prev_rendertargets[1];

    ngli_gpu_ctx_set_viewport(gpu_ctx, prev_vp);
    ngli_gpu_ctx_set_scissor(gpu_ctx, prev_scissor);

    for (int i = 0; i < o->nb_color_textures; i++) {
        const struct rtt_texture_info info = get_rtt_texture_info(o->color_textures[i]);
        struct texture_priv *texture_priv = info.texture_priv;
        struct texture *texture = texture_priv->texture;
        const struct texture_params *texture_params = &texture->params;
        if (texture_params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE)
            ngli_texture_generate_mipmap(texture);
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    s->available_rendertargets[0] = NULL;
    s->available_rendertargets[1] = NULL;

    ngli_rendertarget_freep(&s->rt);
    ngli_rendertarget_freep(&s->rt_resume);
    ngli_texture_freep(&s->depth);

    for (int i = 0; i < s->nb_ms_colors; i++)
        ngli_texture_freep(&s->ms_colors[i]);
    s->nb_ms_colors = 0;
    ngli_texture_freep(&s->ms_depth);
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
