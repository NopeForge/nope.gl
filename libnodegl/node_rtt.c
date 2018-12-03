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

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define DEFAULT_CLEAR_COLOR {-1.0f, -1.0f, -1.0f, -1.0f}
#define FEATURE_DEPTH       (1 << 0)
#define FEATURE_STENCIL     (1 << 1)

static const struct param_choices feature_choices = {
    .name = "framebuffer_features",
    .consts = {
        {"depth",   FEATURE_DEPTH,   .desc=NGLI_DOCSTRING("depth")},
        {"stencil", FEATURE_STENCIL, .desc=NGLI_DOCSTRING("stencil")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct rtt, x)
static const struct node_param rtt_params[] = {
    {"child",         PARAM_TYPE_NODE, OFFSET(child),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_texture` and optionally to `depth_texture`")},
    {"color_texture", PARAM_TYPE_NODE, OFFSET(color_texture),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
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

static GLenum get_depth_attachment(GLenum format)
{
    switch (format) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
        return GL_DEPTH_ATTACHMENT;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
        return GL_DEPTH_STENCIL_ATTACHMENT;
    default:
        return GL_INVALID_ENUM;
    }
}

static GLuint create_renderbuffer(struct glcontext *gl, GLenum attachment, GLenum format, int width, int height, int samples)
{
    GLuint id = 0;
    ngli_glGenRenderbuffers(gl, 1, &id);
    ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, id);
    if (samples > 0)
        ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, samples, format, width, height);
    else
        ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, format, width, height);
    ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
    ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, id);
    return id;
}

static int rtt_init(struct ngl_node *node)
{
    struct rtt *s = node->priv_data;

    float clear_color[4] = DEFAULT_CLEAR_COLOR;
    s->use_clear_color = memcmp(s->clear_color, clear_color, sizeof(s->clear_color));

    return 0;
}

static int rtt_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt *s = node->priv_data;
    struct texture *texture = s->color_texture->priv_data;
    struct texture *depth_texture = NULL;

    s->width = texture->width;
    s->height = texture->height;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) &&
        s->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (!(gl->features & NGLI_FEATURE_PACKED_DEPTH_STENCIL) &&
        s->samples > 0) {
        LOG(WARNING, "context does not support packed depth stencil feature, multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (s->depth_texture) {
        depth_texture = s->depth_texture->priv_data;
        if (s->width != depth_texture->width || s->height != depth_texture->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, depth_texture->width, depth_texture->height);
            return -1;
        }
    }

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

    ngli_glGenFramebuffers(gl, 1, &s->framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);

    LOG(VERBOSE, "init rtt with texture %d", texture->id);
    ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);

    GLenum depth_format = 0;
    GLenum depth_attachment = 0;
    int packed_depth_stencil = gl->features & NGLI_FEATURE_PACKED_DEPTH_STENCIL;

    if (depth_texture) {
        depth_format = depth_texture->internal_format;
        depth_attachment = get_depth_attachment(depth_format);
        if (!packed_depth_stencil && depth_attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            LOG(ERROR, "context does not support packed depth stencil feature");
            return -1;
        }
        s->features |= FEATURE_DEPTH;
        s->features |= depth_attachment == GL_DEPTH_STENCIL_ATTACHMENT ? FEATURE_STENCIL : 0;
        ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, depth_attachment, GL_TEXTURE_2D, depth_texture->id, 0);
    } else {
        int stencil_format = 0;
        int stencil_attachment = 0;
        if (s->features & FEATURE_STENCIL) {
            if (packed_depth_stencil) {
                depth_format = GL_DEPTH24_STENCIL8;
                depth_attachment = GL_DEPTH_STENCIL_ATTACHMENT;
            } else {
                depth_format = GL_DEPTH_COMPONENT16;
                depth_attachment = GL_DEPTH_ATTACHMENT;
                stencil_format = GL_STENCIL_INDEX8;
                stencil_attachment = GL_STENCIL_ATTACHMENT;
            }
        } else if (s->features & FEATURE_DEPTH) {
            depth_format = GL_DEPTH_COMPONENT16;
            depth_attachment = GL_DEPTH_ATTACHMENT;
        }

        if (depth_format == GL_DEPTH24_STENCIL8 ||
            depth_format == GL_DEPTH_COMPONENT16)
            s->depthbuffer_id = create_renderbuffer(gl, depth_attachment, depth_format, s->width, s->height, 0);
        if (stencil_format == GL_STENCIL_INDEX8)
            s->stencilbuffer_id = create_renderbuffer(gl, stencil_attachment, stencil_format, s->width, s->height, 0);
    }

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", s->framebuffer_id);
        return -1;
    }

    if (s->samples > 0) {
        if (gl->features & NGLI_FEATURE_INTERNALFORMAT_QUERY) {
            GLint cbuffer_samples;
            ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, texture->internal_format, GL_SAMPLES, 1, &cbuffer_samples);
            GLint dbuffer_samples;
            ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, depth_format, GL_SAMPLES, 1, &dbuffer_samples);

            GLint samples = NGLI_MIN(cbuffer_samples, dbuffer_samples);
            if (s->samples > samples) {
                LOG(WARNING,
                    "requested samples (%d) exceed renderbuffer's maximum supported value (%d)",
                    s->samples,
                    samples);
                s->samples = samples;
            }
        }

        ngli_glGenFramebuffers(gl, 1, &s->framebuffer_ms_id);
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_ms_id);

        s->colorbuffer_ms_id = create_renderbuffer(gl, GL_COLOR_ATTACHMENT0, texture->internal_format, s->width, s->height, s->samples);
        if (s->features & (FEATURE_DEPTH | FEATURE_STENCIL))
            s->depthbuffer_ms_id = create_renderbuffer(gl, depth_attachment, depth_format, s->width, s->height, s->samples);

        if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "multisampled framebuffer %u is not complete", s->framebuffer_id);
            return -1;
        }
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);

    if (s->vflip) {
        /* flip vertically the color and depth textures so the coordinates
         * match how the uv coordinates system works */
        texture->coordinates_matrix[5] = -1.0f;
        texture->coordinates_matrix[13] = 1.0f;

        if (depth_texture) {
            depth_texture->coordinates_matrix[5] = -1.0f;
            depth_texture->coordinates_matrix[13] = 1.0f;
        }
    }

    return 0;
}

static int rtt_update(struct ngl_node *node, double t)
{
    struct rtt *s = node->priv_data;
    int ret = ngli_node_update(s->child, t);
    if (ret < 0)
        return ret;

    if (s->depth_texture) {
        ret = ngli_node_update(s->depth_texture, t);
        if (ret < 0)
            return ret;
    }

    return ngli_node_update(s->color_texture, t);
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt *s = node->priv_data;

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

    if (s->samples > 0)
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_ms_id);
    else
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);

    GLint viewport[4];
    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, s->width, s->height);

    if (s->use_clear_color) {
        float *rgba = s->clear_color;
        ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ngli_node_draw(s->child);

    if (s->use_clear_color) {
        struct ngl_config *config = &ctx->config;
        float *rgba = config->clear_color;
        ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    if (s->samples > 0) {
        ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, s->framebuffer_ms_id);
        ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, s->framebuffer_id);
        ngli_glBlitFramebuffer(gl, 0, 0, s->width, s->height, 0, 0, s->width, s->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    }

    if (gl->features & NGLI_FEATURE_INVALIDATE_SUBDATA) {
        GLenum attachments[3] = {0};
        int nb_attachments = 0;
        if (s->depthbuffer_id > 0) {
            if (gl->features & NGLI_FEATURE_PACKED_DEPTH_STENCIL)
                attachments[nb_attachments++] = GL_DEPTH_STENCIL_ATTACHMENT;
            else
                attachments[nb_attachments++] = GL_DEPTH_ATTACHMENT;
        }
        if (s->stencilbuffer_id > 0) {
            attachments[nb_attachments++] = GL_STENCIL_ATTACHMENT;
        }
        if (nb_attachments)
            ngli_glInvalidateFramebuffer(gl, GL_FRAMEBUFFER, nb_attachments, attachments);
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);

    struct texture *texture = s->color_texture->priv_data;
    switch(texture->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glBindTexture(gl, GL_TEXTURE_2D, texture->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        break;
    }

    if (s->vflip) {
        texture->coordinates_matrix[5] = -1.0f;
        texture->coordinates_matrix[13] = 1.0f;

        if (s->depth_texture) {
            struct texture *depth_texture = s->depth_texture->priv_data;
            depth_texture->coordinates_matrix[5] = -1.0f;
            depth_texture->coordinates_matrix[13] = 1.0f;
        }
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt *s = node->priv_data;

    ngli_glDeleteFramebuffers(gl, 1, &s->framebuffer_id);
    ngli_glDeleteRenderbuffers(gl, 1, &s->depthbuffer_id);
    ngli_glDeleteRenderbuffers(gl, 1, &s->stencilbuffer_id);
    ngli_glDeleteFramebuffers(gl, 1, &s->framebuffer_ms_id);
    ngli_glDeleteRenderbuffers(gl, 1, &s->colorbuffer_ms_id);
    ngli_glDeleteRenderbuffers(gl, 1, &s->depthbuffer_ms_id);
}

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RENDERTOTEXTURE,
    .name      = "RenderToTexture",
    .init      = rtt_init,
    .prefetch  = rtt_prefetch,
    .update    = rtt_update,
    .draw      = rtt_draw,
    .release   = rtt_release,
    .priv_size = sizeof(struct rtt),
    .params    = rtt_params,
    .file      = __FILE__,
};
