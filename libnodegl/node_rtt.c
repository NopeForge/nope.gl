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

#include "fbo.h"
#include "format.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

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

static int has_stencil(int format)
{
    switch (format) {
    case NGLI_FORMAT_D24_UNORM_S8_UINT:
    case NGLI_FORMAT_D32_SFLOAT_S8_UINT:
        return 1;
    default:
        return 0;
    }
}

static int rtt_init(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    static const float clear_color[4] = DEFAULT_CLEAR_COLOR;
    s->use_clear_color = memcmp(s->clear_color, clear_color, sizeof(s->clear_color));

    return 0;
}

static int create_ms_fbo(struct ngl_node *node, int depth_format)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt_priv *s = node->priv_data;

    ngli_darray_init(&s->fbo_ms_colors, sizeof(struct texture), 0);

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
            struct texture *ms_texture = ngli_darray_push(&s->fbo_ms_colors, NULL);
            if (!ms_texture)
                goto error;
            attachment_params.format = params->format;
            int ret = ngli_texture_init(ms_texture, gl, &attachment_params);
            if (ret < 0)
                goto error;
            if (!ngli_darray_push(&attachments, &ms_texture))
                goto error;
        }
    }

    if (depth_format != NGLI_FORMAT_UNDEFINED) {
        attachment_params.format = depth_format;
        int ret = ngli_texture_init(&s->fbo_ms_depth, gl, &attachment_params);
        if (ret < 0)
            goto error;
        struct texture *fbo_ms_depth = &s->fbo_ms_depth;
        if (!ngli_darray_push(&attachments, &fbo_ms_depth))
            goto error;
    }

    struct fbo_params fbo_params = {
        .width = s->width,
        .height = s->height,
        .nb_attachments = ngli_darray_count(&attachments),
        .attachments = ngli_darray_data(&attachments),
    };
    int ret = ngli_fbo_init(&s->fbo_ms, gl, &fbo_params);
    if (ret < 0)
        goto error;

    ngli_darray_reset(&attachments);
    return 0;

error:
    ngli_darray_reset(&attachments);
    return -1;
}

static int rtt_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt_priv *s = node->priv_data;
    struct texture_priv *depth_texture = NULL;
    struct texture_params *depth_texture_params = NULL;

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
        depth_texture = s->depth_texture->priv_data;
        depth_texture_params = &depth_texture->params;
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
        ngli_darray_push(&attachments, &texture);
    }

    int depth_format = NGLI_FORMAT_UNDEFINED;
    if (depth_texture) {
        struct texture *dt = &depth_texture->texture;
        depth_format = depth_texture_params->format;
        s->features |= FEATURE_DEPTH;
        s->features |= has_stencil(depth_format) ? FEATURE_STENCIL : 0;
        if (!ngli_darray_push(&attachments, &dt))
            goto error;
    } else {
        if (s->features & FEATURE_STENCIL)
            depth_format = NGLI_FORMAT_D24_UNORM_S8_UINT;
        else if (s->features & FEATURE_DEPTH)
            depth_format = NGLI_FORMAT_D16_UNORM;

        if (depth_format != NGLI_FORMAT_UNDEFINED) {
            struct texture *fbo_depth = &s->fbo_depth;
            attachment_params.format = depth_format;
            int ret = ngli_texture_init(fbo_depth, gl, &attachment_params);
            if (ret < 0)
                goto error;
            if (!ngli_darray_push(&attachments, &fbo_depth))
                goto error;
        }
    }

    struct fbo_params fbo_params = {
        .width = s->width,
        .height = s->height,
        .nb_attachments = ngli_darray_count(&attachments),
        .attachments = ngli_darray_data(&attachments),
    };
    int ret = ngli_fbo_init(&s->fbo, gl, &fbo_params);
    if (ret < 0)
        goto error;

    if (s->samples > 0) {
        ret = create_ms_fbo(node, depth_format);
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

        if (depth_texture) {
            struct image *depth_image = &depth_texture->image;
            depth_image->coordinates_matrix[5] = -1.0f;
            depth_image->coordinates_matrix[13] = 1.0f;
        }
    }

    ngli_darray_reset(&attachments);
    return 0;

error:
    ngli_darray_reset(&attachments);
    return -1;
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
    struct glcontext *gl = ctx->glcontext;
    struct rtt_priv *s = node->priv_data;

    struct fbo *fbo = s->samples > 0 ? &s->fbo_ms : &s->fbo;
    int ret = ngli_fbo_bind(fbo);
    if (ret < 0)
        return;

    GLint viewport[4];
    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, s->width, s->height);

    if (s->use_clear_color) {
        float *rgba = s->clear_color;
        ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    if (!(s->features & FEATURE_NO_CLEAR))
        ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ngli_node_draw(s->child);

    if (s->use_clear_color) {
        struct ngl_config *config = &ctx->config;
        float *rgba = config->clear_color;
        ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    if (s->samples > 0)
        ngli_fbo_blit(fbo, &s->fbo, 0);

    if (!(s->features & FEATURE_NO_CLEAR))
        ngli_fbo_invalidate_depth_buffers(fbo);

    ngli_fbo_unbind(fbo);

    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);

    for (int i = 0; i < s->nb_color_textures; i++) {
        struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        struct texture *texture = &texture_priv->texture;
        if (ngli_texture_has_mipmap(texture))
            ngli_texture_generate_mipmap(texture);
        if (s->vflip) {
            struct image *image = &texture_priv->image;
            image->coordinates_matrix[5] = -1.0f;
            image->coordinates_matrix[13] = 1.0f;
        }
    }

    if (s->depth_texture && s->vflip) {
        struct texture_priv *depth_texture = s->depth_texture->priv_data;
        struct image *depth_image = &depth_texture->image;
        depth_image->coordinates_matrix[5] = -1.0f;
        depth_image->coordinates_matrix[13] = 1.0f;
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    ngli_fbo_reset(&s->fbo);
    ngli_texture_reset(&s->fbo_depth);

    ngli_fbo_reset(&s->fbo_ms);
    struct texture *fbo_ms_colors = ngli_darray_data(&s->fbo_ms_colors);
    for (int i = 0; i < ngli_darray_count(&s->fbo_ms_colors); i++)
        ngli_texture_reset(&fbo_ms_colors[i]);
    ngli_darray_reset(&s->fbo_ms_colors);
    ngli_texture_reset(&s->fbo_ms_depth);
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
