/*
 * Copyright 2016 GoPro Inc.
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

#include "rendertarget.h"
#include "format.h"
#include "gctx.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

struct rtt_priv {
    struct ngl_node *child;
    struct ngl_node **color_textures;
    int nb_color_textures;
    struct ngl_node *depth_texture;
    int samples;
    float clear_color[4];
    int features;
    int vflip;

    int use_clear_color;
    int invalidate_depth_stencil;
    int width;
    int height;

    struct rendertarget rt;
    struct texture rt_depth;

    struct rendertarget rt_ms;
    struct darray rt_ms_colors;
    struct texture rt_ms_depth;
};

#define DEFAULT_CLEAR_COLOR {-1.0f, -1.0f, -1.0f, -1.0f}
#define FEATURE_DEPTH       (1 << 0)
#define FEATURE_STENCIL     (1 << 1)
#define FEATURE_NO_CLEAR    (1 << 2)

static const struct param_choices feature_choices = {
    .name = "framebuffer_features",
    .consts = {
        {"depth",   FEATURE_DEPTH,   .desc=NGLI_DOCSTRING("add depth buffer")},
        {"stencil", FEATURE_STENCIL, .desc=NGLI_DOCSTRING("add stencil buffer")},
        {"no_clear",FEATURE_NO_CLEAR,.desc=NGLI_DOCSTRING("not cleared between draws (non-deterministic)")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct rtt_priv, x)
static const struct node_param rtt_params[] = {
    {"child",         PARAM_TYPE_NODE, OFFSET(child),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_textures` and optionally to `depth_texture`")},
    {"color_textures", PARAM_TYPE_NODELIST, OFFSET(color_textures),
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURECUBE, -1},
                      .desc=NGLI_DOCSTRING("destination color texture")},
    {"depth_texture", PARAM_TYPE_NODE, OFFSET(depth_texture),
                      .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
                      .desc=NGLI_DOCSTRING("destination depth (and potentially combined stencil) texture")},
    {"samples",       PARAM_TYPE_INT, OFFSET(samples),
                      .desc=NGLI_DOCSTRING("number of samples used for multisampling anti-aliasing")},
    {"clear_color",   PARAM_TYPE_VEC4, OFFSET(clear_color), {.vec=DEFAULT_CLEAR_COLOR},
                      .desc=NGLI_DOCSTRING("color used to clear the `color_texture`")},
    {"features",      PARAM_TYPE_FLAGS, OFFSET(features),
                      .choices=&feature_choices,
                      .desc=NGLI_DOCSTRING("framebuffer feature mask")},
    {"vflip",         PARAM_TYPE_BOOL, OFFSET(vflip), {.i64=1},
                      .desc=NGLI_DOCSTRING("apply a vertical flip to `color_texture` and `depth_texture` transformation matrices to match the `node.gl` uv coordinates system")},
    {NULL}
};

static int rtt_init(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    for (int i = 0; i < s->nb_color_textures; i++) {
        const struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        if (texture_priv->data_src) {
            LOG(ERROR, "render targets cannot have a data source");
            return -1;
        }
    }

    if (s->depth_texture) {
        const struct texture_priv *texture_priv = s->depth_texture->priv_data;
        if (texture_priv->data_src) {
            LOG(ERROR, "render targets cannot have a data source");
            return -1;
        }
    }

    static const float clear_color[4] = DEFAULT_CLEAR_COLOR;
    s->use_clear_color = memcmp(s->clear_color, clear_color, sizeof(s->clear_color));

    return 0;
}

static int create_ms_rendertarget(struct ngl_node *node, int depth_format)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct rtt_priv *s = node->priv_data;

    ngli_darray_init(&s->rt_ms_colors, sizeof(struct texture), 0);

    struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    attachment_params.width = s->width;
    attachment_params.height = s->height;
    attachment_params.samples = s->samples;
    attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;

    struct darray attachments;
    ngli_darray_init(&attachments, sizeof(struct texture *), 0);
    for (int i = 0; i < s->nb_color_textures; i++) {
        const struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        const struct texture *texture = &texture_priv->texture;
        const struct texture_params *params = &texture->params;
        const int n = params->cubemap ? 6 : 1;
        for (int i = 0; i < n; i++) {
            struct texture *ms_texture = ngli_darray_push(&s->rt_ms_colors, NULL);
            if (!ms_texture) {
                ret = -1;
                goto error;
            }
            attachment_params.format = params->format;
            ret = ngli_texture_init(ms_texture, ctx, &attachment_params);
            if (ret < 0)
                goto error;
            if (!ngli_darray_push(&attachments, &ms_texture)) {
                ret = -1;
                goto error;
            }
        }
    }

    if (depth_format != NGLI_FORMAT_UNDEFINED) {
        attachment_params.format = depth_format;
        ret = ngli_texture_init(&s->rt_ms_depth, ctx, &attachment_params);
        if (ret < 0)
            goto error;
        struct texture *rt_ms_depth = &s->rt_ms_depth;
        if (!ngli_darray_push(&attachments, &rt_ms_depth)) {
            ret = -1;
            goto error;
        }
    }

    struct rendertarget_params rt_params = {
        .width = s->width,
        .height = s->height,
        .nb_attachments = ngli_darray_count(&attachments),
        .attachments = ngli_darray_data(&attachments),
    };
    ret = ngli_rendertarget_init(&s->rt_ms, ctx, &rt_params);
    if (ret < 0)
        goto error;

error:
    ngli_darray_reset(&attachments);
    return ret;
}

static int rtt_prefetch(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt_priv *s = node->priv_data;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) && s->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, "
            "multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (!s->nb_color_textures) {
        LOG(ERROR, "at least one color texture must be specified");
        return -1;
    }

    for (int i = 0; i < s->nb_color_textures; i++) {
        const struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        const struct texture *texture = &texture_priv->texture;
        const struct texture_params *params = &texture->params;
        if (i == 0) {
            s->width = params->width;
            s->height = params->height;
        } else if (s->width != params->width || s->height != params->height) {
            LOG(ERROR, "all color texture dimensions do not match %dx%d != %dx%d",
            s->width, s->height, params->width, params->height);
            return -1;
        }
    }

    if (s->depth_texture) {
        const struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
        const struct texture *depth_texture = &depth_texture_priv->texture;
        const struct texture_params *depth_texture_params = &depth_texture->params;
        if (s->width != depth_texture_params->width || s->height != depth_texture_params->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, depth_texture_params->width, depth_texture_params->height);
            return -1;
        }
    }

    struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    attachment_params.width = s->width;
    attachment_params.height = s->height;
    attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;

    struct darray attachments;
    ngli_darray_init(&attachments, sizeof(struct texture *), 0);
    for (int i = 0; i < s->nb_color_textures; i++) {
        struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        struct texture *texture = &texture_priv->texture;
        if (!ngli_darray_push(&attachments, &texture)) {
            ret = -1;
            goto error;
        }
    }

    int depth_format = NGLI_FORMAT_UNDEFINED;
    if (s->depth_texture) {
        const struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
        const struct texture *depth_texture = &depth_texture_priv->texture;
        const struct texture_params *depth_texture_params = &depth_texture->params;
        depth_format = depth_texture_params->format;
        if (!ngli_darray_push(&attachments, &depth_texture)) {
            ret = -1;
            goto error;
        }
    } else {
        if (s->features & FEATURE_STENCIL)
            depth_format = NGLI_FORMAT_D24_UNORM_S8_UINT;
        else if (s->features & FEATURE_DEPTH)
            depth_format = NGLI_FORMAT_D16_UNORM;

        if (depth_format != NGLI_FORMAT_UNDEFINED) {
            struct texture *rt_depth = &s->rt_depth;
            attachment_params.format = depth_format;
            ret = ngli_texture_init(rt_depth, ctx, &attachment_params);
            if (ret < 0)
                goto error;
            if (!ngli_darray_push(&attachments, &rt_depth)) {
                ret = -1;
                goto error;
            }
            if (!(s->features & FEATURE_NO_CLEAR))
                s->invalidate_depth_stencil = 1;
        }
    }

    struct rendertarget_params rt_params = {
        .width = s->width,
        .height = s->height,
        .nb_attachments = ngli_darray_count(&attachments),
        .attachments = ngli_darray_data(&attachments),
    };
    ret = ngli_rendertarget_init(&s->rt, ctx, &rt_params);
    if (ret < 0)
        goto error;

    if (s->samples > 0) {
        ret = create_ms_rendertarget(node, depth_format);
        if (ret < 0)
            goto error;
    }

    if (s->vflip) {
        /* flip vertically the color and depth textures so the coordinates
         * match how the uv coordinates system works */
        for (int i = 0; i < s->nb_color_textures; i++) {
            struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
            struct image *image = &texture_priv->image;
            image->coordinates_matrix[5] = -1.0f;
            image->coordinates_matrix[13] = 1.0f;
        }

        if (s->depth_texture) {
            struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
            struct image *depth_image = &depth_texture_priv->image;
            depth_image->coordinates_matrix[5] = -1.0f;
            depth_image->coordinates_matrix[13] = 1.0f;
        }
    }

error:
    ngli_darray_reset(&attachments);
    return ret;
}

static int rtt_update(struct ngl_node *node, double t)
{
    struct rtt_priv *s = node->priv_data;
    int ret = ngli_node_update(s->child, t);
    if (ret < 0)
        return ret;

    for (int i = 0; i < s->nb_color_textures; i++) {
        ret = ngli_node_update(s->color_textures[i], t);
        if (ret < 0)
            return ret;
    }

    if (s->depth_texture) {
        ret = ngli_node_update(s->depth_texture, t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rtt_priv *s = node->priv_data;

    struct rendertarget *rt = s->samples > 0 ? &s->rt_ms : &s->rt;
    struct rendertarget *prev_rt = ngli_gctx_get_rendertarget(ctx);
    ngli_gctx_set_rendertarget(ctx, rt);

    int prev_vp[4] = {0};
    ngli_gctx_get_viewport(ctx, prev_vp);

    const int vp[4] = {0, 0, s->width, s->height};
    ngli_gctx_set_viewport(ctx, vp);

    float prev_clear_color[4] = {0};
    if (s->use_clear_color) {
        ngli_gctx_get_clear_color(ctx, prev_clear_color);
        ngli_gctx_set_clear_color(ctx, s->clear_color);
    }

    if (!(s->features & FEATURE_NO_CLEAR)) {
        ngli_gctx_clear_color(ctx);
        ngli_gctx_clear_depth_stencil(ctx);
    }

    ngli_node_draw(s->child);

    if (s->samples > 0)
        ngli_rendertarget_blit(rt, &s->rt, 0);

    if (s->invalidate_depth_stencil)
        ngli_gctx_invalidate_depth_stencil(ctx);

    ngli_gctx_set_rendertarget(ctx, prev_rt);
    ngli_gctx_set_viewport(ctx, prev_vp);

    if (s->use_clear_color)
        ngli_gctx_set_clear_color(ctx, prev_clear_color);

    for (int i = 0; i < s->nb_color_textures; i++) {
        struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        struct texture *texture = &texture_priv->texture;
        if (ngli_texture_has_mipmap(texture))
            ngli_texture_generate_mipmap(texture);
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    ngli_rendertarget_reset(&s->rt);
    ngli_texture_reset(&s->rt_depth);

    ngli_rendertarget_reset(&s->rt_ms);
    struct texture *rt_ms_colors = ngli_darray_data(&s->rt_ms_colors);
    for (int i = 0; i < ngli_darray_count(&s->rt_ms_colors); i++)
        ngli_texture_reset(&rt_ms_colors[i]);
    ngli_darray_reset(&s->rt_ms_colors);
    ngli_texture_reset(&s->rt_ms_depth);
}

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RENDERTOTEXTURE,
    .name      = "RenderToTexture",
    .init      = rtt_init,
    .prefetch  = rtt_prefetch,
    .update    = rtt_update,
    .draw      = rtt_draw,
    .release   = rtt_release,
    .priv_size = sizeof(struct rtt_priv),
    .params    = rtt_params,
    .file      = __FILE__,
};
