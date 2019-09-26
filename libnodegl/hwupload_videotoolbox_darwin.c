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
    struct sxplayer_frame *frame;
    struct hwconv hwconv;
    struct texture planes[2];
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

static int vt_darwin_common_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
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
        struct texture *plane = &vt->planes[i];

        ngli_glBindTexture(gl, plane->target, plane->id);

        int width = IOSurfaceGetWidthOfPlane(surface, i);
        int height = IOSurfaceGetHeightOfPlane(surface, i);
        ngli_texture_set_dimensions(plane, width, height, 0);

        CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(), plane->target,
                                              plane->internal_format, width, height,
                                              plane->format, plane->format_type, surface, i);
        if (err != kCGLNoError) {
            LOG(ERROR, "could not bind IOSurface plane %d to texture %d: %d", i, s->texture.id, err);
            return -1;
        }

        ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, 0);
    }

    return 0;
}

static int vt_darwin_init(struct ngl_node *node, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    struct texture_params params = s->params;
    params.format = vt_get_data_format(frame);
    params.width  = frame->width;
    params.height = frame->height;

    int ret = ngli_texture_init(&s->texture, ctx, &params);
    if (ret < 0)
        return ret;

    ret = ngli_hwconv_init(&vt->hwconv, ctx, &s->texture, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
    if (ret < 0)
        return ret;

    ngli_image_init(&s->image, NGLI_IMAGE_LAYOUT_DEFAULT, &s->texture);

    for (int i = 0; i < 2; i++) {
        struct texture *plane = &vt->planes[i];
        struct texture_params plane_params = NGLI_TEXTURE_PARAM_DEFAULTS;
        plane_params.format = i == 0 ? NGLI_FORMAT_R8_UNORM : NGLI_FORMAT_R8G8_UNORM;
        plane_params.rectangle = 1;
        plane_params.external_storage = 1;

        int ret = ngli_texture_init(plane, ctx, &plane_params);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int vt_darwin_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    int ret = vt_darwin_common_map_frame(node, frame);
    if (ret < 0)
        return ret;

    if (!ngli_texture_match_dimensions(&s->texture, frame->width, frame->height, 0)) {
        ngli_hwconv_reset(&vt->hwconv);
        ngli_texture_reset(&s->texture);

        struct texture_params params = s->params;
        params.format = vt_get_data_format(frame);
        params.width  = frame->width;
        params.height = frame->height;

        int ret = ngli_texture_init(&s->texture, ctx, &params);
        if (ret < 0)
            return ret;

        ret = ngli_hwconv_init(&vt->hwconv, ctx, &s->texture, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
        if (ret < 0)
            return ret;
    }

    ngli_hwconv_convert(&vt->hwconv, vt->planes, NULL);

    if (ngli_texture_has_mipmap(&s->texture))
        ngli_texture_generate_mipmap(&s->texture);

    return 0;
}

static void vt_darwin_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    ngli_hwconv_reset(&vt->hwconv);
    ngli_texture_reset(&s->texture);

    for (int i = 0; i < 2; i++)
        ngli_texture_reset(&vt->planes[i]);

    sxplayer_release_frame(vt->frame);
    vt->frame = NULL;
}

static int vt_darwin_dr_init(struct ngl_node *node, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    for (int i = 0; i < 2; i++) {
        struct texture *plane = &vt->planes[i];
        struct texture_params plane_params = NGLI_TEXTURE_PARAM_DEFAULTS;
        plane_params.format = i == 0 ? NGLI_FORMAT_R8_UNORM : NGLI_FORMAT_R8G8_UNORM;
        plane_params.rectangle = 1;
        plane_params.external_storage = 1;

        int ret = ngli_texture_init(plane, ctx, &plane_params);
        if (ret < 0)
            return ret;
    }

    ngli_image_init(&s->image, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE, &vt->planes[0], &vt->planes[1]);

    return 0;
}

static void vt_darwin_dr_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_vt_darwin *vt = s->hwupload_priv_data;

    for (int i = 0; i < 2; i++)
        ngli_texture_reset(&vt->planes[i]);

    sxplayer_release_frame(vt->frame);
    vt->frame = NULL;
}

static const struct hwmap_class hwmap_vt_darwin_dr_class = {
    .name      = "videotoolbox (iosurface → nv12)",
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwupload_vt_darwin),
    .init      = vt_darwin_dr_init,
    .map_frame = vt_darwin_common_map_frame,
    .uninit    = vt_darwin_dr_uninit,
};

static const struct hwmap_class hwmap_vt_darwin_class = {
    .name      = "videotoolbox (iosurface → nv12 → rgba)",
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwupload_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};

static const struct hwmap_class *vt_darwin_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    int direct_rendering = s->supported_image_layouts & (1 << NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);

    if (direct_rendering && s->params.mipmap_filter) {
        LOG(WARNING, "IOSurface NV12 buffers do not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering ? &hwmap_vt_darwin_dr_class : &hwmap_vt_darwin_class;
}

const struct hwupload_class ngli_hwupload_vt_darwin_class = {
    .get_hwmap = vt_darwin_get_hwmap,
};
