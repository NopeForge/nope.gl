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
#include <sxplayer.h>

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

#define DATA_SRC_TYPES_LIST (const int[]){NGL_NODE_MEDIA,                   \
                                          NGL_NODE_FPS,                     \
                                          NGL_NODE_ANIMATEDBUFFERFLOAT,     \
                                          NGL_NODE_ANIMATEDBUFFERVEC2,      \
                                          NGL_NODE_ANIMATEDBUFFERVEC3,      \
                                          NGL_NODE_ANIMATEDBUFFERVEC4,      \
                                          NGL_NODE_BUFFERBYTE,              \
                                          NGL_NODE_BUFFERBVEC2,             \
                                          NGL_NODE_BUFFERBVEC3,             \
                                          NGL_NODE_BUFFERBVEC4,             \
                                          NGL_NODE_BUFFERINT,               \
                                          NGL_NODE_BUFFERIVEC2,             \
                                          NGL_NODE_BUFFERIVEC3,             \
                                          NGL_NODE_BUFFERIVEC4,             \
                                          NGL_NODE_BUFFERSHORT,             \
                                          NGL_NODE_BUFFERSVEC2,             \
                                          NGL_NODE_BUFFERSVEC3,             \
                                          NGL_NODE_BUFFERSVEC4,             \
                                          NGL_NODE_BUFFERUBYTE,             \
                                          NGL_NODE_BUFFERUBVEC2,            \
                                          NGL_NODE_BUFFERUBVEC3,            \
                                          NGL_NODE_BUFFERUBVEC4,            \
                                          NGL_NODE_BUFFERUINT,              \
                                          NGL_NODE_BUFFERUIVEC2,            \
                                          NGL_NODE_BUFFERUIVEC3,            \
                                          NGL_NODE_BUFFERUIVEC4,            \
                                          NGL_NODE_BUFFERUSHORT,            \
                                          NGL_NODE_BUFFERUSVEC2,            \
                                          NGL_NODE_BUFFERUSVEC3,            \
                                          NGL_NODE_BUFFERUSVEC4,            \
                                          NGL_NODE_BUFFERFLOAT,             \
                                          NGL_NODE_BUFFERVEC2,              \
                                          NGL_NODE_BUFFERVEC3,              \
                                          NGL_NODE_BUFFERVEC4,              \
                                          -1}

#define OFFSET(x) offsetof(struct texture, x)
static const struct node_param texture2d_params[] = {
    {"format", PARAM_TYPE_INT, OFFSET(format), {.i64=GL_RGBA}},
    {"internal_format", PARAM_TYPE_INT, OFFSET(internal_format), {.i64=GL_RGBA}},
    {"type", PARAM_TYPE_INT, OFFSET(type), {.i64=GL_UNSIGNED_BYTE}},
    {"width", PARAM_TYPE_INT, OFFSET(width), {.i64=0}},
    {"height", PARAM_TYPE_INT, OFFSET(height), {.i64=0}},
    {"min_filter", PARAM_TYPE_INT, OFFSET(min_filter), {.i64=GL_NEAREST}},
    {"mag_filter", PARAM_TYPE_INT, OFFSET(mag_filter), {.i64=GL_NEAREST}},
    {"wrap_s", PARAM_TYPE_INT, OFFSET(wrap_s), {.i64=GL_CLAMP_TO_EDGE}},
    {"wrap_t", PARAM_TYPE_INT, OFFSET(wrap_t), {.i64=GL_CLAMP_TO_EDGE}},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST},
    {"access", PARAM_TYPE_INT, OFFSET(access), {.i64=GL_READ_WRITE}},
    {"direct_rendering", PARAM_TYPE_INT, OFFSET(direct_rendering), {.i64=-1}},
    {NULL}
};

static int texture_alloc(struct ngl_node *node, const uint8_t *data)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    if (s->local_id)
        return 0;

    ngli_glGenTextures(gl, 1, &s->id);
    ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
    ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
    ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
    ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
    if (s->width && s->height) {
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, data);
    }
    ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

    s->local_id = s->id;
    s->local_target = GL_TEXTURE_2D;

    return 0;
}

static int texture2d_init(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    s->target = GL_TEXTURE_2D;

    static const float coordinates_matrix[] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    memcpy(s->coordinates_matrix, coordinates_matrix, sizeof(s->coordinates_matrix));

    if (s->external_id) {
        s->id = s->external_id;
        s->target = s->external_target;
    }

    if (s->id)
        return 0;

    const uint8_t *data = NULL;

    if (s->data_src) {
        int ret = ngli_node_init(s->data_src);
        if (ret < 0)
            return ret;

        switch (s->data_src->class->id) {
        case NGL_NODE_FPS:
            s->format = GL_RED;
            s->internal_format = GL_RED;
            s->type = GL_UNSIGNED_BYTE;
            break;
        case NGL_NODE_MEDIA: {
            struct media *media = s->data_src->priv_data;
            if (media->audio_tex) {
                s->format = GL_RED;
                s->internal_format = GL_R32F;
                s->type = GL_FLOAT;
            }
            break;
        }
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
        case NGL_NODE_BUFFERBYTE:
        case NGL_NODE_BUFFERBVEC2:
        case NGL_NODE_BUFFERBVEC3:
        case NGL_NODE_BUFFERBVEC4:
        case NGL_NODE_BUFFERINT:
        case NGL_NODE_BUFFERIVEC2:
        case NGL_NODE_BUFFERIVEC3:
        case NGL_NODE_BUFFERIVEC4:
        case NGL_NODE_BUFFERSHORT:
        case NGL_NODE_BUFFERSVEC2:
        case NGL_NODE_BUFFERSVEC3:
        case NGL_NODE_BUFFERSVEC4:
        case NGL_NODE_BUFFERUBYTE:
        case NGL_NODE_BUFFERUBVEC2:
        case NGL_NODE_BUFFERUBVEC3:
        case NGL_NODE_BUFFERUBVEC4:
        case NGL_NODE_BUFFERUINT:
        case NGL_NODE_BUFFERUIVEC2:
        case NGL_NODE_BUFFERUIVEC3:
        case NGL_NODE_BUFFERUIVEC4:
        case NGL_NODE_BUFFERUSHORT:
        case NGL_NODE_BUFFERUSVEC2:
        case NGL_NODE_BUFFERUSVEC3:
        case NGL_NODE_BUFFERUSVEC4:
        case NGL_NODE_BUFFERFLOAT:
        case NGL_NODE_BUFFERVEC2:
        case NGL_NODE_BUFFERVEC3:
        case NGL_NODE_BUFFERVEC4: {
            struct buffer *buffer = s->data_src->priv_data;
            if (buffer->count != s->width * s->height) {
                LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%d),"
                    " assuming %dx1", s->width, s->height,
                    buffer->count, buffer->count);
                s->width = buffer->count;
                s->height = 1;
            }
            data = buffer->data;
            s->type = buffer->comp_type;
            switch (buffer->data_comp) {
            case 1: s->internal_format = s->format = GL_RED;  break;
            case 2: s->internal_format = s->format = GL_RG;   break;
            case 3: s->internal_format = s->format = GL_RGB;  break;
            case 4: s->internal_format = s->format = GL_RGBA; break;
            default: ngli_assert(0);
            }
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    texture_alloc(node, data);

    return 0;
}

static int texture2d_prefetch(struct ngl_node *node)
{
    return texture2d_init(node);
}

static void handle_fps_frame(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;
    struct fps *fps = s->data_src->priv_data;
    const int width = fps->data_w;
    const int height = fps->data_h;
    const uint8_t *data = fps->data_buf;

    ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
    if (s->width == width && s->height == height)
        ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, data);
    else
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, data);
    ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
}

static void handle_media_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;

    if (media->frame) {
        ngli_hwupload_upload_frame(node, media->frame);

        sxplayer_release_frame(media->frame);
        media->frame = NULL;
    }
}

static void handle_buffer_frame(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;
    struct buffer *buffer = s->data_src->priv_data;
    const uint8_t *data = buffer->data;

    ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
    if (buffer->count)
        ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, 0, s->width, s->height, s->format, s->type, data);
    ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
}

static int texture2d_update(struct ngl_node *node, double t)
{
    struct texture *s = node->priv_data;

    if (!s->data_src)
        return 0;

    int ret = ngli_node_update(s->data_src, t);
    if (ret < 0)
        return ret;

    switch (s->data_src->class->id) {
        case NGL_NODE_FPS:
            handle_fps_frame(node);
            break;
        case NGL_NODE_MEDIA:
            handle_media_frame(node);
            break;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
            ret = ngli_node_update(s->data_src, t);
            if (ret < 0)
                return ret;
            handle_buffer_frame(node);
            break;
    }

    return 0;
}

static void texture2d_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    ngli_hwupload_uninit(node);

    ngli_glDeleteTextures(gl, 1, &s->local_id);
    s->id = s->local_id = 0;
}

static void texture2d_release(struct ngl_node *node)
{
    texture2d_uninit(node);
}

const struct node_class ngli_texture2d_class = {
    .id        = NGL_NODE_TEXTURE2D,
    .name      = "Texture2D",
    .init      = texture2d_init,
    .prefetch  = texture2d_prefetch,
    .update    = texture2d_update,
    .release   = texture2d_release,
    .uninit    = texture2d_uninit,
    .priv_size = sizeof(struct texture),
    .params    = texture2d_params,
};
