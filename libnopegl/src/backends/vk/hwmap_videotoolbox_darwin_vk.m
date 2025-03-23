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
#include <nopemd.h>

#include <CoreVideo/CoreVideo.h>
#if defined(TARGET_DARWIN)
#include <IOSurface/IOSurface.h>
#elif defined(TARGET_IPHONE)
#include <IOSurface/IOSurfaceObjC.h>
#endif
#include <Metal/Metal.h>

#include <MoltenVK/mvk_vulkan.h>
#include <vulkan/vulkan.h>

#include "hwmap.h"
#include "image.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/vulkan/ctx_vk.h"
#include "ngpu/vulkan/format_vk.h"
#include "ngpu/vulkan/texture_vk.h"
#include "ngpu/vulkan/vkutils.h"
#include "ngpu/format.h"
#include "ngpu/type.h"
#include "nopegl.h"

struct format_desc {
    enum image_layout layout;
    size_t nb_planes;
    struct {
        enum ngpu_format format;
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
        desc->planes[0].format = NGPU_FORMAT_B8G8R8A8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12;
        desc->nb_planes = 2;
        desc->planes[0].format = NGPU_FORMAT_R8_UNORM;
        desc->planes[1].format = NGPU_FORMAT_R8G8_UNORM;
        break;
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        desc->layout = NGLI_IMAGE_LAYOUT_NV12;
        desc->nb_planes = 2;
        desc->planes[0].format = NGPU_FORMAT_R16_UNORM;
        desc->planes[1].format = NGPU_FORMAT_R16G16_UNORM;
        break;
    default:
        LOG(ERROR, "unsupported pixel format %d", format);
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

struct hwmap_vt_darwin {
    struct nmd_frame *frame;
    struct ngpu_texture *planes[2];
    OSType format;
    struct format_desc format_desc;
    id<MTLDevice> device;
    CVMetalTextureCacheRef texture_cache;
};

static int vt_darwin_map_frame(struct hwmap *hwmap, struct nmd_frame *frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;
    const struct hwmap_params *params = &hwmap->params;

    nmd_frame_releasep(&vt->frame);
    vt->frame = frame;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->datap[0];
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    for (size_t i = 0; i < vt->format_desc.nb_planes; i++) {
        struct ngpu_texture *plane = vt->planes[i];
        struct ngpu_texture_vk *plane_vk = (struct ngpu_texture_vk *)plane;

        const size_t width = CVPixelBufferGetWidthOfPlane(cvpixbuf, i);
        const size_t height = CVPixelBufferGetHeightOfPlane(cvpixbuf, i);
        const enum ngpu_format format = vt->format_desc.planes[i].format;
        const int vk_format = ngpu_format_ngl_to_vk(format);
        const MTLPixelFormat mtl_format = vt_get_mtl_format(vk_format);

        CVMetalTextureRef texture_ref = NULL;
        CVReturn status = CVMetalTextureCacheCreateTextureFromImage(NULL, vt->texture_cache, cvpixbuf, NULL, mtl_format, width, height, i, &texture_ref);
        if (status != kCVReturnSuccess) {
            LOG(ERROR, "could not create texture from image on plane %zu: %d", i, status);
            return NGL_ERROR_GRAPHICS_GENERIC;
        }

        const VkImportMetalTextureInfoEXT mtl_texture_info = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT,
            .plane = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .mtlTexture = CVMetalTextureGetTexture(texture_ref),
        };

        const VkImageCreateInfo image_create_info = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext         = &mtl_texture_info,
            .imageType     = VK_IMAGE_TYPE_2D,
            .extent        = {width, height, 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .format        = vk_format,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage         = ngpu_vk_get_image_usage_flags(params->texture_usage),
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res = vkCreateImage(vk->device, &image_create_info, NULL, &plane_vk->image);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not create image: %s", ngli_vk_res2str(res));
            CFRelease(texture_ref);
            return ngli_vk_res2ret(res);
        }

        const struct ngpu_texture_params plane_params = {
            .type             = NGPU_TEXTURE_TYPE_2D,
            .format           = format,
            .width            = width,
            .height           = height,
            .min_filter       = params->texture_min_filter,
            .mag_filter       = params->texture_mag_filter,
            .wrap_s           = params->texture_wrap_s,
            .wrap_t           = params->texture_wrap_t,
            .usage            = params->texture_usage,
        };

        const struct ngpu_texture_vk_wrap_params wrap_params = {
            .params       = &plane_params,
            .image        = plane_vk->image,
            .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        res = ngpu_texture_vk_wrap(vt->planes[i], &wrap_params);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not wrap texture: %s", ngli_vk_res2str(res));
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

    int direct_rendering = params->image_layouts & NGLI_IMAGE_LAYOUT_NV12_BIT;

    if (direct_rendering && params->texture_mipmap_filter) {
        LOG(WARNING, "IOSurface buffers do not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering;
}

static int vt_darwin_init(struct hwmap *hwmap, struct nmd_frame * frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->datap[0];
    vt->format = CVPixelBufferGetPixelFormatType(cvpixbuf);

    int ret = vt_get_format_desc(vt->format, &vt->format_desc);
    if (ret < 0)
        return ret;

    VkExportMetalDeviceInfoEXT mtl_device_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT,
    };

    VkExportMetalObjectsInfoEXT mtl_objects_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
        .pNext = &mtl_device_info,
    };

    vkExportMetalObjectsEXT(gpu_ctx_vk->vkcontext->device, &mtl_objects_info);
    vt->device = mtl_device_info.mtlDevice;

    CVReturn status = CVMetalTextureCacheCreate(NULL, NULL, vt->device, NULL, &vt->texture_cache);
    if (status != kCVReturnSuccess) {
        LOG(ERROR, "could not create Metal texture cache: %d", status);
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    for (size_t i = 0; i < 2; i++) {
        vt->planes[i] = ngpu_texture_create(gpu_ctx);
        if (!vt->planes[i])
            return NGL_ERROR_MEMORY;
    }

    const struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = vt->format_desc.layout,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_nopemd_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vt->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

    return 0;
}

static void vt_darwin_uninit(struct hwmap *hwmap)
{
    struct hwmap_vt_darwin *vt = hwmap->hwmap_priv_data;

    for (size_t i = 0; i < 2; i++)
        ngpu_texture_freep(&vt->planes[i]);

    nmd_frame_releasep(&vt->frame);
}

const struct hwmap_class ngli_hwmap_vt_darwin_vk_class = {
    .name      = "videotoolbox (iosurface → nv12)",
    .hwformat  = NMD_PIXFMT_VT,
    .layouts   = (const enum image_layout[]){
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
