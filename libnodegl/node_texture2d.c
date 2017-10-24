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
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=(const int[]){NGL_NODE_MEDIA, NGL_NODE_FPS, -1}},
    {"access", PARAM_TYPE_INT, OFFSET(access), {.i64=GL_READ_WRITE}},
    {NULL}
};

static int texture_init_2D(struct ngl_node *node)
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
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, NULL);
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

    if (s->data_src) {
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
        default:
            ngli_assert(0);
        }
    }

    texture_init_2D(node);

    if (s->data_src) {
        int ret = ngli_node_init(s->data_src);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void texture2d_prefetch(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    texture2d_init(node);

    if (s->data_src)
        ngli_node_prefetch(s->data_src);
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

static void texture2d_update(struct ngl_node *node, double t)
{
    struct texture *s = node->priv_data;

    if (!s->data_src)
        return;

    ngli_node_update(s->data_src, t);

    if (s->data_src->class->id == NGL_NODE_FPS) {
        handle_fps_frame(node);
    } else if (s->data_src->class->id == NGL_NODE_MEDIA) {
        handle_media_frame(node);
    }
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
    struct texture *s = node->priv_data;

    if (s->data_src)
        ngli_node_release(s->data_src);

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
