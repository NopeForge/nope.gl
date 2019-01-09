/*
 * Copyright 2018 GoPro Inc.
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

#include <CoreVideo/CoreVideo.h>

#include "format.h"
#include "glincludes.h"
#include "hwconv.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

#define NGLI_CFRELEASE(ref) do { \
    if (ref) {                   \
        CFRelease(ref);          \
        ref = NULL;              \
    }                            \
} while (0)

struct hwupload_vt_ios {
    struct hwconv hwconv;
    OSType format;
    CVOpenGLESTextureRef ios_textures[2];
};

static int vt_ios_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    vt->format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    ngli_assert(vt->format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);

    s->data_format = NGLI_FORMAT_B8G8R8A8_UNORM;
    int ret = ngli_format_get_gl_texture_format(gl,
                                                s->data_format,
                                                &s->format,
                                                &s->internal_format,
                                                &s->type);
    if (ret < 0)
        return ret;

    int frame_width = CVPixelBufferGetWidth(cvpixbuf);
    int frame_height = CVPixelBufferGetHeight(cvpixbuf);

    ret = ngli_node_texture_update_data(node, frame_width, frame_height, 0, NULL);
    if (ret < 0)
        return ret;

    ret = ngli_hwconv_init(&vt->hwconv, gl,
                           s->id, s->data_format, s->width, s->height,
                           NGLI_TEXTURE_LAYOUT_NV12);
    if (ret < 0)
        return ret;

    return 0;
}

static void vt_ios_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    ngli_hwconv_reset(&vt->hwconv);

    NGLI_CFRELEASE(vt->ios_textures[0]);
    NGLI_CFRELEASE(vt->ios_textures[1]);
}

static int vt_ios_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVOpenGLESTextureRef textures[2] = {0};
    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    ngli_assert(vt->format == cvformat);

    int width  = CVPixelBufferGetWidth(cvpixbuf);
    int height = CVPixelBufferGetHeight(cvpixbuf);

    int ret = ngli_node_texture_update_data(node, width, height, 0, NULL);
    if (ret < 0)
        return ret;

    if (ret) {
        vt_ios_uninit(node);
        ret = vt_ios_init(node, frame);
        if (ret < 0)
            return ret;
    }

    for (int i = 0; i < 2; i++) {
        GLint gl_format;
        GLint gl_internal_format;
        GLenum gl_type;
        int plane_width;
        int plane_height;
        int plane_format;

        switch (i) {
        case 0:
            plane_width = s->width;
            plane_height = s->height;
            plane_format = NGLI_FORMAT_R8_UNORM;
            break;
        case 1:
            plane_width = (s->width + 1) >> 1;
            plane_height = (s->height + 1) >> 1;
            plane_format = NGLI_FORMAT_R8G8_UNORM;
            break;
        default:
            ngli_assert(0);
        }

        int ret = ngli_format_get_gl_texture_format(gl, plane_format, &gl_format, &gl_internal_format, &gl_type);
        if (ret < 0)
            return ret;

        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    *texture_cache,
                                                                    cvpixbuf,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    gl_internal_format,
                                                                    plane_width,
                                                                    plane_height,
                                                                    gl_format,
                                                                    gl_type,
                                                                    i,
                                                                    &textures[i]);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
            NGLI_CFRELEASE(textures[0]);
            NGLI_CFRELEASE(textures[1]);
            return -1;
        }

        GLint id = CVOpenGLESTextureGetName(textures[i]);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
    }

    const struct texture_plane planes[] = {
        {
            .id = CVOpenGLESTextureGetName(textures[0]),
            .target = GL_TEXTURE_2D,
        }, {
            .id = CVOpenGLESTextureGetName(textures[1]),
            .target = GL_TEXTURE_2D,
        }
    };

    ret = ngli_hwconv_convert(&vt->hwconv, planes, NULL);
    if (ret < 0)
        return ret;

    NGLI_CFRELEASE(textures[0]);
    NGLI_CFRELEASE(textures[1]);

    switch (s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        break;
    }

    return 0;
}

static int vt_ios_dr_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    vt->format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (vt->format) {
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32RGBA:
        s->layout = NGLI_TEXTURE_LAYOUT_DEFAULT;
        s->planes[0].id = 0;
        s->planes[0].target = GL_TEXTURE_2D;
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        s->layout = NGLI_TEXTURE_LAYOUT_NV12;
        for (int i = 0; i < 2; i++) {
            s->planes[i].id = 0;
            s->planes[i].target = GL_TEXTURE_2D;
        }
        break;
    default:
        return -1;
    }

    return 0;
}

static int vt_ios_dr_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    ngli_assert(vt->format == cvformat);

    s->width  = CVPixelBufferGetWidth(cvpixbuf);
    s->height = CVPixelBufferGetHeight(cvpixbuf);

    switch (vt->format) {
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32RGBA: {
        int data_format;
        GLint gl_format;
        GLint gl_internal_format;
        GLenum gl_type;

        switch (vt->format) {
        case kCVPixelFormatType_32BGRA:
            data_format = NGLI_FORMAT_B8G8R8A8_UNORM;
            break;
        case kCVPixelFormatType_32RGBA:
            data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
            break;
        default:
            ngli_assert(0);
        }

        int ret = ngli_format_get_gl_texture_format(gl, data_format, &gl_format, &gl_internal_format, &gl_type);
        if (ret < 0)
            return ret;

        NGLI_CFRELEASE(vt->ios_textures[0]);

        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    *texture_cache,
                                                                    cvpixbuf,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    gl_internal_format,
                                                                    s->width,
                                                                    s->height,
                                                                    gl_format,
                                                                    gl_type,
                                                                    0,
                                                                    &vt->ios_textures[0]);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
            return -1;
        }

        GLint id = CVOpenGLESTextureGetName(vt->ios_textures[0]);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        break;
    }
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
        for (int i = 0; i < 2; i++) {
            int width;
            int height;
            int data_format;
            GLint gl_format;
            GLint gl_internal_format;
            GLenum gl_type;

            switch (i) {
            case 0:
                width = s->width;
                height = s->height;
                data_format = NGLI_FORMAT_R8_UNORM;
                break;
            case 1:
                width = (s->width + 1) >> 1;
                height = (s->height + 1) >> 1;
                data_format = NGLI_FORMAT_R8G8_UNORM;
                break;
            default:
                ngli_assert(0);
            }

            int ret = ngli_format_get_gl_texture_format(gl, data_format, &gl_format, &gl_internal_format, &gl_type);
            if (ret < 0)
                return ret;

            NGLI_CFRELEASE(vt->ios_textures[i]);

            CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                        *texture_cache,
                                                                        cvpixbuf,
                                                                        NULL,
                                                                        GL_TEXTURE_2D,
                                                                        gl_internal_format,
                                                                        width,
                                                                        height,
                                                                        gl_format,
                                                                        gl_type,
                                                                        i,
                                                                        &(vt->ios_textures[i]));
            if (err != noErr) {
                LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
                NGLI_CFRELEASE(vt->ios_textures[0]);
                NGLI_CFRELEASE(vt->ios_textures[1]);
                return -1;
            }

            GLint id = CVOpenGLESTextureGetName(vt->ios_textures[i]);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        }
        break;
        }
    default:
        ngli_assert(0);
    }

    switch (vt->format) {
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32RGBA:
        s->planes[0].id = CVOpenGLESTextureGetName(vt->ios_textures[0]);
        switch (s->min_filter) {
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            ngli_glBindTexture(gl, GL_TEXTURE_2D, s->planes[0].id);
            ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
            break;
        }
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        for (int i = 0; i < 2; i++)
            s->planes[i].id = CVOpenGLESTextureGetName(vt->ios_textures[i]);
        break;
    default:
        ngli_assert(0);
    }

    return 0;
}

static void vt_ios_dr_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    NGLI_CFRELEASE(vt->ios_textures[0]);
    NGLI_CFRELEASE(vt->ios_textures[1]);
}

static const struct hwmap_class hwmap_vt_ios_class = {
    .name      = "videotoolbox (nv12 → rgba)",
    .priv_size = sizeof(struct hwupload_vt_ios),
    .init      = vt_ios_init,
    .map_frame = vt_ios_map_frame,
    .uninit    = vt_ios_uninit,
};

static const struct hwmap_class hwmap_vt_ios_dr_class = {
    .name      = "videotoolbox (zero-copy)",
    .priv_size = sizeof(struct hwupload_vt_ios),
    .init      = vt_ios_dr_init,
    .map_frame = vt_ios_dr_map_frame,
    .uninit    = vt_ios_dr_uninit,
};

static const struct hwmap_class *vt_ios_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
        return &hwmap_vt_ios_dr_class;
    case kCVPixelFormatType_32RGBA:
        return &hwmap_vt_ios_dr_class;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        if (s->direct_rendering)
            return &hwmap_vt_ios_dr_class;
        else
            return &hwmap_vt_ios_class;
    default:
        return NULL;
    }
}

const struct hwupload_class ngli_hwupload_vt_ios_class = {
    .get_hwmap = vt_ios_get_hwmap,
};
