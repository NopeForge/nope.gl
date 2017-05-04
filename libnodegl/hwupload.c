/*
 * Copyright 2017 GoPro Inc.
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

#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

enum {
    HWUPLOAD_FMT_NONE,
    HWUPLOAD_FMT_COMMON,
    HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA,
    HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA,
};

struct hwupload_config {
    int format;
    int width;
    int height;
    int linesize;
    float xscale;
    GLint gl_format;
    GLint gl_internal_format;
    GLint gl_type;
};

static int get_config_from_frame(struct sxplayer_frame *frame, struct hwupload_config *config)
{
    config->width = frame->width;
    config->height = frame->height;
    config->linesize = frame->linesize;
    config->xscale = config->width ? 1.0 : (config->linesize >> 2) / (float)config->width;

    switch (frame->pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
        config->format = HWUPLOAD_FMT_COMMON;
        config->gl_format = GL_RGBA;
        config->gl_internal_format = GL_RGBA;
        config->gl_type = GL_UNSIGNED_BYTE;
        break;
    case SXPLAYER_PIXFMT_BGRA:
        config->format = HWUPLOAD_FMT_COMMON;
        config->gl_format = GL_BGRA;
        config->gl_internal_format = GL_RGBA;
        config->gl_type = GL_UNSIGNED_BYTE;
        break;
    case SXPLAYER_SMPFMT_FLT:
        config->format = HWUPLOAD_FMT_COMMON;
        config->gl_format = GL_RED;
        config->gl_internal_format = GL_R32F;
        config->gl_type = GL_FLOAT;
        break;
#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case SXPLAYER_PIXFMT_VT: {
        CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
        OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

        config->width = CVPixelBufferGetWidth(cvpixbuf);
        config->height = CVPixelBufferGetHeight(cvpixbuf);
        config->linesize = CVPixelBufferGetBytesPerRow(cvpixbuf);
        if (config->width)
            config->xscale = (config->linesize >> 2) / (float)config->width;

        switch (cvformat) {
        case kCVPixelFormatType_32BGRA:
            config->format = HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA;
            config->gl_format = GL_BGRA;
            break;
        case kCVPixelFormatType_32RGBA:
            config->format = HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA;
            config->gl_format = GL_RGBA;
            break;
        default:
            ngli_assert(0);
        };
        config->gl_internal_format = GL_RGBA;
        config->gl_type = GL_UNSIGNED_BYTE;
        break;
    }
#endif
    default:
        ngli_assert(0);
    }

    return 0;
}

static int init_common(struct ngl_node *node, struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    s->upload_fmt = config->format;

    return 0;
}

static int upload_common_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    int dimension_changed = s->width != (config->linesize >> 2) || s->height != config->height;

    s->id                    = s->local_id;
    s->target                = s->local_target;
    s->format                = config->gl_format;
    s->internal_format       = config->gl_internal_format;
    s->type                  = config->gl_type;
    s->width                 = config->linesize >> 2;
    s->height                = config->height;
    s->coordinates_matrix[0] = config->xscale;

    gl->BindTexture(GL_TEXTURE_2D, s->id);
    if (dimension_changed)
        gl->TexImage2D(GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, frame->data);
    else
        gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s->width, s->height, s->format, s->type, frame->data);

    switch(s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        gl->GenerateMipmap(GL_TEXTURE_2D);
        break;
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

#if defined(TARGET_DARWIN)
static int init_vt(struct ngl_node *node, struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    s->upload_fmt = config->format;

    return 0;
}

static int upload_vt_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    CVPixelBufferLockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    uint8_t *data = CVPixelBufferGetBaseAddress(cvpixbuf);

    int dimension_changed = s->width != (config->linesize >> 2) || s->height != config->height;

    s->format                = config->gl_format;
    s->internal_format       = config->gl_internal_format;
    s->type                  = config->gl_type;
    s->width                 = config->linesize >> 2;
    s->height                = config->height;
    s->coordinates_matrix[0] = config->xscale;

    gl->BindTexture(GL_TEXTURE_2D, s->id);
    if (dimension_changed)
        gl->TexImage2D(GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, data);
    else
        gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s->width, s->height, s->format, s->type, data);

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

    return 0;
}
#endif

#if defined(TARGET_IPHONE)
static int init_vt(struct ngl_node *node, struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    if (s->upload_fmt == config->format)
        return 0;

    s->upload_fmt = config->format;

    return 0;
}

static int upload_vt_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    CVOpenGLESTextureRef texture = NULL;
    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(glcontext);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;

    s->format                = config->gl_format;
    s->internal_format       = config->gl_internal_format;
    s->type                  = config->gl_type;
    s->width                 = config->linesize >> 2;
    s->height                = config->height;
    s->coordinates_matrix[0] = config->xscale;

    CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                *texture_cache,
                                                                cvpixbuf,
                                                                NULL,
                                                                GL_TEXTURE_2D,
                                                                s->internal_format,
                                                                s->width,
                                                                s->height,
                                                                s->format,
                                                                s->type,
                                                                0,
                                                                &texture);
    if (err != noErr) {
        LOG(ERROR, "Could not create CoreVideo texture from image: %d", err);
        s->id = s->local_id;
        return -1;
    }

    if (s->texture)
        CFRelease(s->texture);

    s->texture = texture;
    s->id = CVOpenGLESTextureGetName(texture);

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

    return 0;
}
#endif

static int hwupload_init(struct ngl_node *node, struct hwupload_config *config)
{
    int ret = 0;

    switch (config->format) {
    case HWUPLOAD_FMT_COMMON:
        ret = init_common(node, config);
        break;
#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA:
        ret = init_vt(node, config);
        break;
#endif
    }

    return ret;
}

int ngli_hwupload_upload_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    int ret;
    struct hwupload_config config = { 0 };

    ret = get_config_from_frame(frame, &config);
    if (ret < 0)
        return ret;

    ret = hwupload_init(node, &config);
    if (ret < 0)
        return ret;

    if (frame) {
        switch(frame->pix_fmt) {
        case SXPLAYER_PIXFMT_BGRA:
        case SXPLAYER_PIXFMT_RGBA:
        case SXPLAYER_SMPFMT_FLT:
            ret = upload_common_frame(node, &config, frame);
            break;
#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
        case SXPLAYER_PIXFMT_VT:
            ret = upload_vt_frame(node, &config, frame);
            break;
#endif
        default:
            ngli_assert(0);
        }
    }

    return ret;
}

void ngli_hwupload_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    s->upload_fmt = HWUPLOAD_FMT_NONE;

    ngl_node_unrefp(&s->quad);
    ngl_node_unrefp(&s->shader);
    ngl_node_unrefp(&s->tshape);
    ngl_node_unrefp(&s->textures[0]);
    ngl_node_unrefp(&s->textures[1]);
    ngl_node_unrefp(&s->textures[2]);
    ngl_node_unrefp(&s->target_texture);
    ngl_node_unrefp(&s->rtt);

#if defined(TARGET_IPHONE)
    if (s->texture)
        CFRelease(s->texture);
#endif
}
