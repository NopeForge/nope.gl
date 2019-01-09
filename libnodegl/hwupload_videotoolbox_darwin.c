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
#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLIOSurface.h>

#include "format.h"
#include "glincludes.h"
#include "hwconv.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

struct hwupload_vt_darwin {
    GLuint textures[2];
    struct sxplayer_frame *frame;
    struct hwconv hwconv;
    struct texture_plane planes[2];
};

static int vt_get_data_format(struct sxplayer_frame *frame)
{
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (cvformat) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_32BGRA:
        return NGLI_FORMAT_B8G8R8A8_UNORM;
    case kCVPixelFormatType_32RGBA:
        return NGLI_FORMAT_R8G8B8A8_UNORM;
    default:
        return -1;
    }
}

static int vt_darwin_init(struct ngl_node *node, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    s->data_format = vt_get_data_format(frame);
    if (s->data_format < 0)
        return -1;

    int ret = ngli_format_get_gl_texture_format(gl, s->data_format,
                                                &s->format, &s->internal_format, &s->type);
    if (ret < 0)
        return ret;

    ret = ngli_node_texture_update_data(node, frame->width, frame->height, 0, NULL);
    if (ret < 0)
        return ret;

    ret = ngli_hwconv_init(&vt->hwconv, gl, s->id, s->data_format,
                           frame->width, frame->height, NGLI_TEXTURE_LAYOUT_NV12_RECTANGLE);
    if (ret < 0)
        return ret;

    for (int i = 0; i < 2; i++) {
        ngli_glGenTextures(gl, 1, &vt->textures[i]);
        ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, vt->textures[i]);
        ngli_glTexParameteri(gl, GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ngli_glTexParameteri(gl, GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        ngli_glTexParameteri(gl, GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ngli_glTexParameteri(gl, GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, 0);

        vt->planes[i].id = vt->textures[i];
        vt->planes[i].target = GL_TEXTURE_RECTANGLE;
    }

    return 0;
}

static int vt_darwin_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    sxplayer_release_frame(vt->frame);
    vt->frame = frame;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return -1;
    }

    OSType format = IOSurfaceGetPixelFormat(surface);
    if (format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
        LOG(ERROR, "unsupported IOSurface format: 0x%x", format);
        return -1;
    }

    for (int i = 0; i < 2; i++) {
        ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, vt->textures[i]);

        int width = IOSurfaceGetWidthOfPlane(surface, i);
        int height = IOSurfaceGetHeightOfPlane(surface, i);

        vt->planes[i].width = width;
        vt->planes[i].height = height;

        GLint format;
        GLint internal_format;
        GLenum type;

        int data_format = i == 0 ? NGLI_FORMAT_R8_UNORM : NGLI_FORMAT_R8G8_UNORM;
        int ret = ngli_format_get_gl_texture_format(gl, data_format, &format, &internal_format, &type);
        if (ret < 0)
            return ret;

        CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(), GL_TEXTURE_RECTANGLE,
                                              internal_format, width, height, format, type, surface, i);
        if (err != kCGLNoError) {
            LOG(ERROR, "could not bind IOSurface plane %d to texture %d: %d", i, s->id, err);
            return -1;
        }

        ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, 0);
    }

    int ret = ngli_node_texture_update_data(node, frame->width, frame->height, 0, NULL);
    if (ret < 0)
        return ret;

    ngli_hwconv_convert(&vt->hwconv, vt->planes, NULL);

    if (ngli_node_texture_has_mipmap(node)) {
        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
    }

    return 0;
}

static void vt_darwin_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    ngli_hwconv_reset(&vt->hwconv);

    sxplayer_release_frame(vt->frame);
    vt->frame = NULL;
}

static const struct hwmap_class hwmap_vt_darwin_class = {
    .name      = "videotoolbox (copy)",
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwupload_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};

static const struct hwmap_class *vt_darwin_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    return &hwmap_vt_darwin_class;
}

const struct hwupload_class ngli_hwupload_vt_darwin_class = {
    .get_hwmap = vt_darwin_get_hwmap,
};
