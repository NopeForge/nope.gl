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
#include "gpu_ctx_gl.h"
#include "glincludes.h"
#include "hwmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "internal.h"
#include "texture_gl.h"

struct format_desc {
    int layout;
    int nb_planes;
    struct {
        int format;
    } planes[2];
};

static int vt_get_format_desc(OSType format, struct format_desc *desc)
{
    switch (format) {
    case kCVPixelFormatType_32BGRA:
        desc->layout = NGLI_IMAGE_LAYOUT_RECTANGLE;
        desc->nb_planes = 1;
        desc->planes[0].format = NGLI_FORMAT_B8G8R8A8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12_RECTANGLE;
        desc->nb_planes = 2;
        desc->planes[0].format = NGLI_FORMAT_R8_UNORM;
        desc->planes[1].format = NGLI_FORMAT_R8G8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12_RECTANGLE;
        desc->nb_planes = 2;
        desc->planes[0].format = NGLI_FORMAT_R16_UNORM;
        desc->planes[1].format = NGLI_FORMAT_R16G16_UNORM;
        break;
    default:
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

struct hwmap_vt_darwin {
    struct sxplayer_frame *frame;
    struct texture *planes[2];
    OSType format;
    struct format_desc format_desc;
};

static int vt_darwin_map_plane(struct hwmap *hwmap, IOSurfaceRef surface, int index)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)ctx->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    struct texture *plane = vt->planes[index];
    struct texture_gl *plane_gl = (struct texture_gl *)plane;

    ngli_glBindTexture(gl, plane_gl->target, plane_gl->id);

    int width = IOSurfaceGetWidthOfPlane(surface, index);
    int height = IOSurfaceGetHeightOfPlane(surface, index);
    ngli_texture_gl_set_dimensions(plane, width, height, 0);

    /* CGLTexImageIOSurface2D() requires GL_UNSIGNED_INT_8_8_8_8_REV instead of GL_UNSIGNED_SHORT to map BGRA IOSurface2D */
    const GLenum format_type = plane_gl->format == GL_BGRA ? GL_UNSIGNED_INT_8_8_8_8_REV : plane_gl->format_type;

    CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(), plane_gl->target,
                                          plane_gl->internal_format, width, height,
                                          plane_gl->format, format_type, surface, index);
    if (err != kCGLNoError) {
        LOG(ERROR, "could not bind IOSurface plane %d to texture %d: %s", index, plane_gl->id, CGLErrorString(err));
        return -1;
    }

    ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, 0);

    return 0;
}

static int vt_darwin_map_frame(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);
    ngli_assert(vt->format == cvformat);

    sxplayer_release_frame(vt->frame);
    vt->frame = frame;

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return -1;
    }

    for (int i = 0; i < vt->format_desc.nb_planes; i++) {
        int ret = vt_darwin_map_plane(hwmap, surface, i);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int support_direct_rendering(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    const struct hwmap_params *params = &hwmap->params;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);
    int direct_rendering = 1;

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
        direct_rendering = params->image_layouts & (1 << NGLI_IMAGE_LAYOUT_RECTANGLE);
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        direct_rendering = params->image_layouts & (1 << NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
        break;
    default:
        ngli_assert(0);
    }

    if (direct_rendering) {
        if (params->texture_mipmap_filter) {
            LOG(WARNING, "Videotoolbox textures do not support mipmapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        }
    }

    return direct_rendering;
}

static int vt_darwin_init(struct hwmap *hwmap, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    const struct hwmap_params *params = &hwmap->params;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    vt->format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    int ret = vt_get_format_desc(vt->format, &vt->format_desc);
    if (ret < 0)
        return ret;

    for (int i = 0; i < vt->format_desc.nb_planes; i++) {
        const struct texture_params plane_params = {
            .type             = NGLI_TEXTURE_TYPE_2D,
            .format           = vt->format_desc.planes[i].format,
            .min_filter       = params->texture_min_filter,
            .mag_filter       = params->texture_mag_filter,
            .wrap_s           = params->texture_wrap_s,
            .wrap_t           = params->texture_wrap_t,
            .usage            = NGLI_TEXTURE_USAGE_SAMPLED_BIT,
            .rectangle        = 1,
            .external_storage = 1,
        };

        vt->planes[i] = ngli_texture_create(gpu_ctx);
        if (!vt->planes[i])
            return NGL_ERROR_MEMORY;

        int ret = ngli_texture_init(vt->planes[i], &plane_params);
        if (ret < 0)
            return ret;
    }

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = vt->format_desc.layout,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vt->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap, frame);

    return 0;
}

static void vt_darwin_uninit(struct hwmap *hwmap)
{
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;

    for (int i = 0; i < 2; i++)
        ngli_texture_freep(&vt->planes[i]);

    sxplayer_release_frame(vt->frame);
    vt->frame = NULL;
}

const struct hwmap_class ngli_hwmap_vt_darwin_gl_class = {
    .name      = "videotoolbox (iosurface)",
    .hwformat  = SXPLAYER_PIXFMT_VT,
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};
