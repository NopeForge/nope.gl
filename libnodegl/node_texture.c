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

#ifdef __ANDROID__
#include <libavcodec/mediacodec.h>
#endif

#ifdef __APPLE__
#include <CoreVideo/CoreVideo.h>
#endif

#include "glincludes.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct texture, x)
static const struct node_param texture_params[] = {
    {"target", PARAM_TYPE_INT, OFFSET(target), {.i64=GL_TEXTURE_2D}},
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
    {NULL}
};

static int texture_init_2D(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    gl->GenTextures(1, &s->local_id);
    gl->BindTexture(GL_TEXTURE_2D, s->local_id);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
    if (s->width && s->height) {
        gl->TexImage2D(GL_TEXTURE_2D, 0, s->format, s->width, s->height, 0, s->internal_format, s->type, NULL);
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    s->local_target = GL_TEXTURE_2D;

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

    if (s->target == GL_TEXTURE_2D)
        texture_init_2D(node);

    s->id = s->local_id;
    s->target = s->local_target;

    memcpy(s->coordinates_matrix, coordinates_matrix, sizeof(s->coordinates_matrix));

    if (s->data_src) {
        int ret = ngli_node_init(s->data_src);
        if (ret < 0)
            return ret;
    }

#ifdef TARGET_IPHONE
    struct glcontext *glcontext = node->ctx->glcontext;
    CVEAGLContext *eaglcontext = ngli_glcontext_get_handle(glcontext);

    CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                                NULL,
                                                *eaglcontext,
                                                NULL,
                                                &s->cache);

    if (err != noErr) {
        LOG(ERROR, "Could not create CoreVideo texture cache: %d", err);
        return -1;
    }
#endif

    return 0;
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

    gl->BindTexture(GL_TEXTURE_2D, s->id);
    if (s->width == width && s->height == height)
        gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, data);
    else
        gl->TexImage2D(GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, data);
    gl->BindTexture(GL_TEXTURE_2D, 0);
}

#if defined(TARGET_ANDROID)
static void handle_mc_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;

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
}
#endif

#if defined(TARGET_DARWIN)
static void handle_vt_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    CVPixelBufferLockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    int width = CVPixelBufferGetBytesPerRow(cvpixbuf) >> 2;
    int height = CVPixelBufferGetHeight(cvpixbuf);
    float padding = CVPixelBufferGetWidth(cvpixbuf) / (float)width;
    uint8_t *data = CVPixelBufferGetBaseAddress(cvpixbuf);
    OSType format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (format) {
    case kCVPixelFormatType_32BGRA:
        s->format = GL_BGRA;
        break;
    case kCVPixelFormatType_32RGBA:
        s->format = GL_RGBA;
        break;
    default:
        ngli_assert(0);
    }
    s->internal_format = GL_RGBA;
    s->type = GL_UNSIGNED_BYTE;
    s->coordinates_matrix[0] = padding;

    gl->BindTexture(GL_TEXTURE_2D, s->id);
    if (s->width == width && s->height == height)
        gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, data);
    else
        gl->TexImage2D(GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, data);

    CVPixelBufferUnlockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    switch(s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        gl->GenerateMipmap(GL_TEXTURE_2D);
        break;
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    s->width = width;
    s->height = height;
}
#endif

#if defined(TARGET_IPHONE)
static void handle_vt_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    CVOpenGLESTextureRef texture = NULL;
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;

    int width = CVPixelBufferGetBytesPerRow(cvpixbuf) >> 2;
    int height = CVPixelBufferGetHeight(cvpixbuf);
    float padding = CVPixelBufferGetWidth(cvpixbuf) / (float)width;
    OSType format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (format) {
    case kCVPixelFormatType_32BGRA:
        s->format = GL_BGRA;
        break;
    case kCVPixelFormatType_32RGBA:
        s->format = GL_RGBA;
        break;
    default:
        ngli_assert(0);
    }
    s->internal_format = GL_RGBA;
    s->type = GL_UNSIGNED_BYTE;
    s->coordinates_matrix[0] = padding;

    CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                s->cache,
                                                                cvpixbuf,
                                                                NULL,
                                                                GL_TEXTURE_2D,
                                                                s->internal_format,
                                                                width,
                                                                height,
                                                                s->format,
                                                                s->type,
                                                                0,
                                                                &texture);

    if (err != noErr) {
        LOG(ERROR, "Could not create CoreVideo texture from image: %d", err);
        s->id = s->local_id;
        return;
    }

    if (s->texture)
        CFRelease(s->texture);

    s->texture = texture;
    s->id = s->media_id = CVOpenGLESTextureGetName(texture);

    gl->BindTexture(GL_TEXTURE_2D, s->id);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
    switch(s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        gl->GenerateMipmap(GL_TEXTURE_2D);
        break;
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    s->width = width;
    s->height = height;
}
#endif

static void handle_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    int width = frame->linesize >> 2;
    int height = frame->height;
    float padding = frame->width / (float)width;

    s->id = s->local_id;
    s->target = s->local_target;
    s->coordinates_matrix[0] = padding;

    switch (frame->pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
        s->format = GL_RGBA;
        s->internal_format = GL_RGBA;
        s->type = GL_UNSIGNED_BYTE;
        break;
    case SXPLAYER_PIXFMT_BGRA:
        s->format = GL_BGRA;
        s->internal_format = GL_RGBA;
        s->type = GL_UNSIGNED_BYTE;
        break;
    case SXPLAYER_SMPFMT_FLT:
        s->format = GL_RED;
        s->internal_format = GL_R32F;
        s->type = GL_FLOAT;
        break;
    default:
        ngli_assert(0);
    }

    gl->BindTexture(GL_TEXTURE_2D, s->id);
    if (s->width == width && s->height == height) {
        gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, s->format, s->type, frame->data);
    } else {
        gl->TexImage2D(GL_TEXTURE_2D, 0, s->internal_format, width, height, 0, s->format, s->type, frame->data);
    }

    switch(s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        gl->GenerateMipmap(GL_TEXTURE_2D);
        break;
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    s->width = width;
    s->height = height;
}

static void handle_media_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;
    struct sxplayer_frame *frame = media->frame;

    if (frame) {
        switch(frame->pix_fmt) {
#if defined(TARGET_ANDROID)
        case SXPLAYER_PIXFMT_MEDIACODEC:
            handle_mc_frame(node, frame);
            break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
        case SXPLAYER_PIXFMT_VT:
            handle_vt_frame(node, frame);
            break;
#endif
        default:
            handle_frame(node, frame);
            break;
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
        handle_fps_frame(node);
    } else if (s->data_src->class->id == NGL_NODE_MEDIA) {
        handle_media_frame(node);
    }
}

static void texture_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    gl->DeleteTextures(1, &s->local_id);

#ifdef TARGET_IPHONE
    if (s->texture)
        CFRelease(s->texture);
    if (s->cache)
        CFRelease(s->cache);
#endif
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
