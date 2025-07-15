/*
 * Copyright 2018-2022 GoPro Inc.
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
#include <nopemd.h>

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLIOSurface.h>

#include "hwmap.h"
#include "image.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/format.h"
#include "ngpu/opengl/ctx_gl.h"
#include "ngpu/opengl/glincludes.h"
#include "ngpu/opengl/texture_gl.h"
#include "nopegl.h"

struct format_desc {
    enum image_layout layout;
    size_t nb_planes;
    struct {
        enum ngpu_format format;
    } planes[2];
};

static int vt_get_format_desc(OSType format, struct format_desc *desc)
{
    switch (format) {
    case kCVPixelFormatType_32BGRA:
        desc->layout = NGLI_IMAGE_LAYOUT_RECTANGLE;
        desc->nb_planes = 1;
        desc->planes[0].format = NGPU_FORMAT_B8G8R8A8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12_RECTANGLE;
        desc->nb_planes = 2;
        desc->planes[0].format = NGPU_FORMAT_R8_UNORM;
        desc->planes[1].format = NGPU_FORMAT_R8G8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12_RECTANGLE;
        desc->nb_planes = 2;
        desc->planes[0].format = NGPU_FORMAT_R16_UNORM;
        desc->planes[1].format = NGPU_FORMAT_R16G16_UNORM;
        break;
    default:
        LOG(ERROR, "unsupported pixel format %u", (uint32_t)format);
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

struct hwmap_vt_darwin {
    struct nmd_frame *frame;
    struct ngpu_texture *planes[2];
    GLuint gl_planes[2];
    OSType format;
    struct format_desc format_desc;
};

static int vt_darwin_map_plane(struct hwmap *hwmap, IOSurfaceRef surface, size_t index)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct ngpu_limits *gpu_limits = &gpu_ctx->limits;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)ctx->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    struct ngpu_texture *plane = vt->planes[index];
    struct ngpu_texture_gl *plane_gl = (struct ngpu_texture_gl *)plane;

    gl->funcs.BindTexture(GL_TEXTURE_RECTANGLE, plane_gl->id);

    size_t width = IOSurfaceGetWidthOfPlane(surface, index);
    size_t height = IOSurfaceGetHeightOfPlane(surface, index);

    const uint32_t max_dimension = gpu_limits->max_texture_dimension_2d;
    if (width > max_dimension || height > max_dimension) {
        LOG(ERROR, "plane dimensions (%zux%zu) exceed GPU limits (%ux%u)",
            width, height, max_dimension, max_dimension);
        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }
    ngpu_texture_gl_set_dimensions(plane, (uint32_t)width, (uint32_t)height, 0);

    /* CGLTexImageIOSurface2D() requires GL_UNSIGNED_INT_8_8_8_8_REV instead of GL_UNSIGNED_SHORT to map BGRA IOSurface2D */
    const GLenum format_type = plane_gl->format == GL_BGRA ? GL_UNSIGNED_INT_8_8_8_8_REV : plane_gl->format_type;

    CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(), GL_TEXTURE_RECTANGLE,
                                          plane_gl->internal_format, (GLsizei)width, (GLsizei)height,
                                          plane_gl->format, format_type, surface, (GLuint)index);
    if (err != kCGLNoError) {
        LOG(ERROR, "could not bind IOSurface plane %zu to texture %u: %s", index, plane_gl->id, CGLErrorString(err));
        return NGL_ERROR_EXTERNAL;
    }

    gl->funcs.BindTexture(GL_TEXTURE_RECTANGLE, 0);

    return 0;
}

static int vt_darwin_map_frame(struct hwmap *hwmap, struct nmd_frame *frame)
{
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->datap[0];
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);
    ngli_assert(vt->format == cvformat);

    nmd_frame_releasep(&vt->frame);
    vt->frame = frame;

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return NGL_ERROR_EXTERNAL;
    }

    for (size_t i = 0; i < vt->format_desc.nb_planes; i++) {
        int ret = vt_darwin_map_plane(hwmap, surface, i);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int support_direct_rendering(struct hwmap *hwmap, struct nmd_frame *frame)
{
    const struct hwmap_params *params = &hwmap->params;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->datap[0];
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);
    int direct_rendering = 1;

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
        direct_rendering = NGLI_HAS_ALL_FLAGS(params->image_layouts, NGLI_IMAGE_LAYOUT_RECTANGLE_BIT);
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        direct_rendering = NGLI_HAS_ALL_FLAGS(params->image_layouts, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE_BIT);
        break;
    default:
        ngli_assert(0);
    }

    if (direct_rendering) {
        if (params->texture_mipmap_filter) {
            LOG(WARNING, "Videotoolbox textures do not support mipmapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        } else if (params->texture_wrap_s != NGPU_WRAP_CLAMP_TO_EDGE ||
                   params->texture_wrap_t != NGPU_WRAP_CLAMP_TO_EDGE) {
            LOG(WARNING, "Videotoolbox textures only support clamp to edge wrapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        }
    }

    return direct_rendering;
}

static int vt_darwin_init(struct hwmap *hwmap, struct nmd_frame * frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    const struct hwmap_params *params = &hwmap->params;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->datap[0];
    vt->format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    int ret = vt_get_format_desc(vt->format, &vt->format_desc);
    if (ret < 0)
        return ret;

    gl->funcs.GenTextures(2, vt->gl_planes);

    for (size_t i = 0; i < vt->format_desc.nb_planes; i++) {
        const GLint min_filter = ngpu_texture_get_gl_min_filter(params->texture_min_filter, NGPU_MIPMAP_FILTER_NONE);
        const GLint mag_filter = ngpu_texture_get_gl_mag_filter(params->texture_mag_filter);

        gl->funcs.BindTexture(GL_TEXTURE_RECTANGLE, vt->gl_planes[i]);
        gl->funcs.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, min_filter);
        gl->funcs.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, mag_filter);
        gl->funcs.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->funcs.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->funcs.BindTexture(GL_TEXTURE_RECTANGLE, 0);

        const struct ngpu_texture_params plane_params = {
            .type             = NGPU_TEXTURE_TYPE_2D,
            .format           = vt->format_desc.planes[i].format,
            .min_filter       = params->texture_min_filter,
            .mag_filter       = params->texture_mag_filter,
            .wrap_s           = NGPU_WRAP_CLAMP_TO_EDGE,
            .wrap_t           = NGPU_WRAP_CLAMP_TO_EDGE,
            .usage            = NGPU_TEXTURE_USAGE_SAMPLED_BIT,
        };

        const struct ngpu_texture_gl_wrap_params wrap_params = {
            .params  = &plane_params,
            .texture = vt->gl_planes[i],
            .target  = GL_TEXTURE_RECTANGLE,
        };

        vt->planes[i] = ngpu_texture_create(gpu_ctx);
        if (!vt->planes[i])
            return NGL_ERROR_MEMORY;

        ret = ngpu_texture_gl_wrap(vt->planes[i], &wrap_params);
        if (ret < 0)
            return ret;
    }

    const struct image_params image_params = {
        .width = (uint32_t)frame->width,
        .height = (uint32_t)frame->height,
        .layout = vt->format_desc.layout,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_nopemd_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vt->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap, frame);

    return 0;
}

static void vt_darwin_uninit(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;

    for (size_t i = 0; i < 2; i++)
        ngpu_texture_freep(&vt->planes[i]);

    gl->funcs.DeleteTextures(2, vt->gl_planes);

    nmd_frame_releasep(&vt->frame);
}

const struct hwmap_class ngli_hwmap_vt_darwin_gl_class = {
    .name      = "videotoolbox (iosurface)",
    .hwformat  = NMD_PIXFMT_VT,
    .layouts   = (const enum image_layout[]){
        NGLI_IMAGE_LAYOUT_RECTANGLE,
        NGLI_IMAGE_LAYOUT_NV12_RECTANGLE,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};
