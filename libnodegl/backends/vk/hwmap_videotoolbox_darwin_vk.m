/*
 * Copyright 2020-2022 GoPro Inc.
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

#include <vulkan/vulkan.h>

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurface.h>
#include <Metal/Metal.h>

#include <MoltenVK/vk_mvk_moltenvk.h>

#include "format.h"
#include "format_vk.h"
#include "gpu_ctx_vk.h"
#include "hwmap.h"
#include "image.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "texture_vk.h"
#include "type.h"
#include "vkutils.h"

struct format_desc {
    int layout;
    int nb_planes;
    struct {
        int format;
    } planes[2];
};

static MTLPixelFormat vt_get_mtl_format(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM: return MTLPixelFormatBGRA8Unorm;
    case VK_FORMAT_R8_UNORM:       return MTLPixelFormatR8Unorm;
    case VK_FORMAT_R16_UNORM:      return MTLPixelFormatR16Unorm;
    case VK_FORMAT_R8G8_UNORM:     return MTLPixelFormatRG8Unorm;
    case VK_FORMAT_R16G16_UNORM:   return MTLPixelFormatRG16Unorm;
    default: ngli_assert(0);
    }
}

static int vt_get_format_desc(OSType format, struct format_desc *desc)
{
    switch (format) {
    case kCVPixelFormatType_32BGRA:
        desc->layout = NGLI_IMAGE_LAYOUT_DEFAULT;
        desc->nb_planes = 1;
        desc->planes[0].format = NGLI_FORMAT_B8G8R8A8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12;
        desc->nb_planes = 2;
        desc->planes[0].format = NGLI_FORMAT_R8_UNORM;
        desc->planes[1].format = NGLI_FORMAT_R8G8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12;
        desc->nb_planes = 2;
        desc->planes[0].format = NGLI_FORMAT_R16_UNORM;
        desc->planes[1].format = NGLI_FORMAT_R16G16_UNORM;
        break;
    default:
        LOG(ERROR, "unsupported pixel format %d", format);
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

struct hwmap_vt_darwin {
    struct sxplayer_frame *frame;
    struct texture *planes[2];
    OSType format;
    struct format_desc format_desc;
    id<MTLDevice> device;
    CVMetalTextureCacheRef texture_cache;
};

static int vt_darwin_map_frame(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    const struct hwmap_params *params = &hwmap->params;

    sxplayer_release_frame(vt->frame);
    vt->frame = frame;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    for (int i = 0; i < vt->format_desc.nb_planes; i++) {
        struct texture *plane = vt->planes[i];
        struct texture_vk *plane_vk = (struct texture_vk *)plane;

        const size_t width = CVPixelBufferGetWidthOfPlane(cvpixbuf, i);
        const size_t height = CVPixelBufferGetHeightOfPlane(cvpixbuf, i);
        const int format = vt->format_desc.planes[i].format;
        const int vk_format = ngli_format_ngl_to_vk(format);
        const MTLPixelFormat mtl_format = vt_get_mtl_format(vk_format);

        CVMetalTextureRef texture_ref = NULL;
        CVReturn status = CVMetalTextureCacheCreateTextureFromImage(NULL, vt->texture_cache, cvpixbuf, NULL, mtl_format, width, height, i, &texture_ref);
        if (status != kCVReturnSuccess) {
            LOG(ERROR, "could not create texture from image on plane %d: %d", i, status);
            return NGL_ERROR_GRAPHICS_GENERIC;
        }

        const VkImageCreateInfo image_create_info = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .extent        = {width, height, 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .format        = vk_format,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage         = ngli_vk_get_image_usage_flags(params->texture_usage),
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res = vkCreateImage(vk->device, &image_create_info, NULL, &plane_vk->image);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not create image: %s", ngli_vk_res2str(res));
            CFRelease(texture_ref);
            return ngli_vk_res2ret(res);
        }

        const struct texture_params plane_params = {
            .type             = NGLI_TEXTURE_TYPE_2D,
            .format           = format,
            .width            = width,
            .height           = height,
            .min_filter       = params->texture_min_filter,
            .mag_filter       = params->texture_mag_filter,
            .wrap_s           = params->texture_wrap_s,
            .wrap_t           = params->texture_wrap_t,
            .usage            = params->texture_usage,
        };

        const struct texture_vk_wrap_params wrap_params = {
            .params       = &plane_params,
            .image        = plane_vk->image,
            .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        res = ngli_texture_vk_wrap(vt->planes[i], &wrap_params);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not wrap texture: %s", ngli_vk_res2str(res));
            CFRelease(texture_ref);
            return ngli_vk_res2ret(res);
        }

        id<MTLTexture> mtl_texture = CVMetalTextureGetTexture(texture_ref);
        res = vkSetMTLTextureMVK(plane_vk->image, mtl_texture);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not set Metal texture: %s", ngli_vk_res2str(res));
            CFRelease(texture_ref);
            return ngli_vk_res2ret(res);
        }

        CFRelease(texture_ref);
    }

    return 0;
}

static int support_direct_rendering(struct hwmap *hwmap)
{
    const struct hwmap_params *params = &hwmap->params;

    int direct_rendering = params->image_layouts & (1 << NGLI_IMAGE_LAYOUT_NV12);

    if (direct_rendering && params->texture_mipmap_filter) {
        LOG(WARNING, "IOSurface buffers do not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering;
}

static int vt_darwin_init(struct hwmap *hwmap, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)gpu_ctx;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    vt->format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    int ret = vt_get_format_desc(vt->format, &vt->format_desc);
    if (ret < 0)
        return ret;

    vkGetMTLDeviceMVK(gpu_ctx_vk->vkcontext->phy_device, &vt->device);
    CVReturn status = CVMetalTextureCacheCreate(NULL, NULL, vt->device, NULL, &vt->texture_cache);
    if (status != kCVReturnSuccess) {
        LOG(ERROR, "could not create Metal texture cache: %d", status);
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    for (int i = 0; i < 2; i++) {
        vt->planes[i] = ngli_texture_create(gpu_ctx);
        if (!vt->planes[i])
            return NGL_ERROR_MEMORY;
    }

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = vt->format_desc.layout,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vt->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

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

const struct hwmap_class ngli_hwmap_vt_darwin_vk_class = {
    .name      = "videotoolbox (iosurface → nv12)",
    .hwformat  = SXPLAYER_PIXFMT_VT,
    .layouts   = (const int[]){
        NGLI_IMAGE_LAYOUT_DEFAULT,
        NGLI_IMAGE_LAYOUT_NV12,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};
