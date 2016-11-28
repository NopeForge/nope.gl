/*
 * Copyright 2016 Clément Bœsch <cboesch@gopro.com>
 * Copyright 2016 Matthieu Bouron <mbouron@gopro.com>
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

#ifdef __ANDROID__
#include <libavcodec/mediacodec.h>
#endif

#ifdef __APPLE__
#include <CoreVideo/CoreVideo.h>
#endif

#include "gl_utils.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct texture, x)
static const struct node_param texture_params[] = {
    {"target", PARAM_TYPE_INT, OFFSET(target), {.i64=GL_TEXTURE_2D}},
    {"format", PARAM_TYPE_INT, OFFSET(format), {.i64=GL_NONE}},
    {"internal_format", PARAM_TYPE_INT, OFFSET(internal_format), {.i64=GL_NONE}},
    {"type", PARAM_TYPE_INT, OFFSET(type), {.i64=GL_UNSIGNED_BYTE}},
    {"width", PARAM_TYPE_INT, OFFSET(width), {.i64=0}},
    {"height", PARAM_TYPE_INT, OFFSET(height), {.i64=0}},
    {"min_filter", PARAM_TYPE_INT, OFFSET(min_filter), {.i64=GL_NEAREST}},
    {"mag_filter", PARAM_TYPE_INT, OFFSET(mag_filter), {.i64=GL_NEAREST}},
    {"wrap_s", PARAM_TYPE_INT, OFFSET(wrap_s), {.i64=GL_CLAMP_TO_EDGE}},
    {"wrap_t", PARAM_TYPE_INT, OFFSET(wrap_t), {.i64=GL_CLAMP_TO_EDGE}},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=(const int[]){NGL_NODE_MEDIA, NGL_NODE_FPS, -1}},
    {NULL}
};

static int texture_init_2D(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    glGenTextures(1, &s->local_id);
    glBindTexture(GL_TEXTURE_2D, s->local_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (s->width && s->height) {
        glTexImage2D(GL_TEXTURE_2D, 0, s->format, s->width, s->height, 0, s->internal_format, s->type, NULL);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    s->id = s->local_id;
    s->target = s->local_target = GL_TEXTURE_2D;

    return 0;
}

static int texture_init(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    static const float coordinates_matrix[] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    if (s->id)
        return 0;

    if (s->data_src && s->data_src->class->id == NGL_NODE_FPS) {
        if (s->format == GL_NONE)
            s->format = GL_LUMINANCE;
        if (s->internal_format == GL_NONE)
            s->internal_format = GL_LUMINANCE;
    } else {
        if (s->format == GL_NONE)
            s->format = GL_RGBA;
        if (s->internal_format == GL_NONE)
            s->internal_format = GL_RGBA;
    }

    if (s->target == GL_TEXTURE_2D) {
        texture_init_2D(node);
    }
    memcpy(s->coordinates_matrix, coordinates_matrix, sizeof(s->coordinates_matrix));

    if (s->data_src)
        ngli_node_init(s->data_src);

    return 0;
}

static void handle_fps_frame(struct texture *s)
{
    struct fps *fps = s->data_src->priv_data;
    const int width = fps->data_w;
    const int height = fps->data_h;
    const uint8_t *data = fps->data_buf;

    glBindTexture(GL_TEXTURE_2D, s->id);
    if (s->width == width && s->height == height)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, data);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void handle_media_frame(struct texture *s)
{
    struct media *media = s->data_src->priv_data;
    struct sxplayer_frame *frame = media->frame;

    if (frame) {
        if (frame->pix_fmt == SXPLAYER_PIXFMT_MEDIACODEC) {
#ifdef __ANDROID__
            AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;
            float matrix[4*4] = {
                1.0f,  0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f,  0.0f, 1.0f, 0.0f,
                0.0f,  1.0f, 0.0f, 1.0f,
            };

            av_android_surface_render_buffer(media->android_surface, buffer, s->coordinates_matrix);
            ngli_mat4_mul(s->coordinates_matrix, matrix, s->coordinates_matrix);

            s->id = s->media_id = media->android_texture_id;
            s->target = s->media_target = media->android_texture_target;
            s->width = frame->width;
            s->height = frame->height;
#endif
        } else if (frame->pix_fmt == SXPLAYER_PIXFMT_VT) {
#ifdef __APPLE__
            CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
            CVPixelBufferLockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

            int width = CVPixelBufferGetBytesPerRow(cvpixbuf) >> 2;
            int height = CVPixelBufferGetHeight(cvpixbuf);
            float padding = CVPixelBufferGetWidth(cvpixbuf) / (float)width;
            uint8_t *data = CVPixelBufferGetBaseAddress(cvpixbuf);

            s->id = s->local_id;
            s->target = s->local_target;
            s->format = GL_BGRA;
            s->internal_format = GL_RGBA;
            s->coordinates_matrix[0] = padding;

            glBindTexture(GL_TEXTURE_2D, s->id);
            if (s->width == width && s->height == height) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, data);
            } else {
                glTexImage2D(GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, data);
            }

            CVPixelBufferUnlockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

            switch(s->min_filter) {
            case GL_NEAREST_MIPMAP_NEAREST:
            case GL_NEAREST_MIPMAP_LINEAR:
            case GL_LINEAR_MIPMAP_NEAREST:
            case GL_LINEAR_MIPMAP_LINEAR:
                glGenerateMipmap(GL_TEXTURE_2D);
                break;
            }
            glBindTexture(GL_TEXTURE_2D, 0);

            s->width = width;
            s->height = height;
#endif
        } else {
            int width = frame->linesize >> 2;
            int height = frame->height;
            float padding = frame->width / (float)width;

            s->id = s->local_id;
            s->target = s->local_target;
            s->coordinates_matrix[0] = padding;

            glBindTexture(GL_TEXTURE_2D, s->id);
            if (s->width == width && s->height == height) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, frame->data);
            } else {
                glTexImage2D(GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, frame->data);
            }

            switch(s->min_filter) {
            case GL_NEAREST_MIPMAP_NEAREST:
            case GL_NEAREST_MIPMAP_LINEAR:
            case GL_LINEAR_MIPMAP_NEAREST:
            case GL_LINEAR_MIPMAP_LINEAR:
                glGenerateMipmap(GL_TEXTURE_2D);
                break;
            }
            glBindTexture(GL_TEXTURE_2D, 0);

            s->width = width;
            s->height = height;
        }

        sxplayer_release_frame(media->frame);
        media->frame = frame = NULL;
    }
}

static void texture_update(struct ngl_node *node, double t)
{
    struct texture *s = node->priv_data;

    if (!s->data_src)
        return;

    ngli_node_update(s->data_src, t);

    if (s->data_src->class->id == NGL_NODE_FPS) {
        handle_fps_frame(s);
    } else if (s->data_src->class->id == NGL_NODE_MEDIA) {
        handle_media_frame(s);
    }
}

static void texture_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    glDeleteTextures(1, &s->local_id);
}

const struct node_class ngli_texture_class = {
    .id        = NGL_NODE_TEXTURE,
    .name      = "Texture",
    .init      = texture_init,
    .update    = texture_update,
    .uninit    = texture_uninit,
    .priv_size = sizeof(struct texture),
    .params    = texture_params,
};
