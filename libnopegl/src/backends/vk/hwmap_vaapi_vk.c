/*
 * Copyright 2022 GoPro Inc.
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
#include <unistd.h>
#include <nopemd.h>

#include <va/va.h>
#include <va/va_drmcommon.h>
#include <libdrm/drm_fourcc.h>

#include "hwmap.h"
#include "image.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/ctx.h"
#include "ngpu/vulkan/ctx_vk.h"
#include "ngpu/vulkan/format_vk.h"
#include "ngpu/vulkan/texture_vk.h"
#include "ngpu/vulkan/vkutils.h"
#include "nopegl.h"
#include "utils/utils.h"

struct format_desc {
    int layout;
    size_t nb_planes;
    int log2_chroma_width;
    int log2_chroma_height;
    enum ngpu_format formats[2];
};

static int vaapi_get_format_desc(uint32_t format, struct format_desc *desc)
{
    switch (format) {
    case VA_FOURCC_NV12:
        *desc = (struct format_desc) {
            .layout = NGLI_IMAGE_LAYOUT_NV12,
            .nb_planes = 2,
            .log2_chroma_width = 1,
            .log2_chroma_height = 1,
            .formats[0] = NGPU_FORMAT_R8_UNORM,
            .formats[1] = NGPU_FORMAT_R8G8_UNORM,
        };
        break;
    case VA_FOURCC_P010:
    case VA_FOURCC_P016:
        *desc = (struct format_desc) {
            .layout = NGLI_IMAGE_LAYOUT_NV12,
            .nb_planes = 2,
            .log2_chroma_width = 1,
            .log2_chroma_height = 1,
            .formats[0] = NGPU_FORMAT_R16_UNORM,
            .formats[1] = NGPU_FORMAT_R16G16_UNORM,
        };
        break;
    default:
        LOG(ERROR, "unsupported vaapi surface format %d", format);
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

struct hwmap_vaapi {
    struct nmd_frame *frame;
    struct ngpu_texture *planes[2];
    VkImage images[2];
    VkDeviceMemory memories[2];
    int fds[2];
    VADRMPRIMESurfaceDescriptor surface_descriptor;
    int surface_acquired;
};

static int support_direct_rendering(struct hwmap *hwmap)
{
    const struct hwmap_params *params = &hwmap->params;

    int direct_rendering = NGLI_HAS_ALL_FLAGS(params->image_layouts, NGLI_IMAGE_LAYOUT_NV12_BIT);

    if (direct_rendering && params->texture_mipmap_filter) {
        LOG(WARNING,
            "vaapi direct rendering does not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering;
}

static int vaapi_init(struct hwmap *hwmap, struct nmd_frame *frame)
{
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    for (size_t i = 0; i < 2; i++)
        vaapi->fds[i] = -1;

    const struct image_params image_params = {
        .width = (uint32_t)frame->width,
        .height = (uint32_t)frame->height,
        .layout = NGLI_IMAGE_LAYOUT_NV12,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_nopemd_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vaapi->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

    return 0;
}

static void vaapi_release_frame_resources(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    if (vaapi->surface_acquired) {
        for (size_t i = 0; i < 2; i++) {
            hwmap->mapped_image.planes[i] = NULL;
            ngpu_texture_freep(&vaapi->planes[i]);

            if (vaapi->images[i]) {
                vkDestroyImage(vk->device, vaapi->images[i], NULL);
                vaapi->images[i] = VK_NULL_HANDLE;
            }
            if (vaapi->memories[i]) {
                vkFreeMemory(vk->device, vaapi->memories[i], NULL);
                vaapi->memories[i] = VK_NULL_HANDLE;
            }
            if (vaapi->fds[i] != -1) {
                close(vaapi->fds[i]);
                vaapi->fds[i] = -1;
            }
        }
        for (uint32_t i = 0; i < vaapi->surface_descriptor.num_objects; i++) {
            close(vaapi->surface_descriptor.objects[i].fd);
        }
        vaapi->surface_acquired = 0;
    }

    nmd_frame_releasep(&vaapi->frame);
}

static void vaapi_uninit(struct hwmap *hwmap)
{
    vaapi_release_frame_resources(hwmap);
}

static int vaapi_map_frame(struct hwmap *hwmap, struct nmd_frame *frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct vaapi_ctx *vaapi_ctx = &ctx->vaapi_ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;
    const struct hwmap_params *params = &hwmap->params;

    vaapi_release_frame_resources(hwmap);
    vaapi->frame = frame;

    VASurfaceID surface_id = (VASurfaceID)(intptr_t)frame->datap[0];
    VAStatus status = vaExportSurfaceHandle(vaapi_ctx->va_display,
                                            surface_id,
                                            VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_READ_ONLY |
                                            VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                            &vaapi->surface_descriptor);
    if (status != VA_STATUS_SUCCESS) {
        LOG(ERROR, "failed to export vaapi surface handle: %s", vaErrorStr(status));
        return NGL_ERROR_EXTERNAL;
    }
    vaapi->surface_acquired = 1;

    status = vaSyncSurface(vaapi_ctx->va_display, surface_id);
    if (status != VA_STATUS_SUCCESS)
        LOG(WARNING, "failed to sync surface: %s", vaErrorStr(status));

    struct format_desc desc;
    int ret = vaapi_get_format_desc(vaapi->surface_descriptor.fourcc, &desc);
    if (ret < 0)
        return ret;

    const size_t nb_layers = vaapi->surface_descriptor.num_layers;
    if (nb_layers != desc.nb_planes) {
        LOG(ERROR, "surface layer count (%zu) does not match plane count (%zu)",
            nb_layers, desc.nb_planes);
        return NGL_ERROR_UNSUPPORTED;
    }

    for (size_t i = 0; i < nb_layers; i++) {
        const enum ngpu_format ngl_format = desc.formats[i];
        const VkFormat format = ngpu_format_ngl_to_vk(ngl_format);
        const uint32_t width = (uint32_t)(i == 0 ? frame->width : NGLI_CEIL_RSHIFT(frame->width, desc.log2_chroma_width));
        const uint32_t height = (uint32_t)(i == 0 ? frame->height : NGLI_CEIL_RSHIFT(frame->height, desc.log2_chroma_height));

        const uint32_t id = vaapi->surface_descriptor.layers[i].object_index[0];
        const int fd = vaapi->surface_descriptor.objects[id].fd;
        const uint32_t size = vaapi->surface_descriptor.objects[id].size;
        const uint32_t offset = vaapi->surface_descriptor.layers[i].offset[0];
        const uint32_t pitch = vaapi->surface_descriptor.layers[i].pitch[0];
        const uint64_t drm_format_modifier = vaapi->surface_descriptor.objects[id].drm_format_modifier;

        const VkImageDrmFormatModifierExplicitCreateInfoEXT drm_explicit_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
            .drmFormatModifier = drm_format_modifier,
            .drmFormatModifierPlaneCount = 1,
            .pPlaneLayouts = &(const VkSubresourceLayout) {
                .rowPitch   = pitch,
                .depthPitch = 0,
                .offset     = offset,
            },
        };

        const VkExternalMemoryImageCreateInfoKHR ext_mem_info = {
            .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
            .pNext       = &drm_explicit_info,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        };

        const VkImageCreateInfo img_info = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext         = &ext_mem_info,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = format,
            .extent        = {.width = width, .height = height, .depth = 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
            .usage         = ngpu_vk_get_image_usage_flags(params->texture_usage),
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        };

        const VkPhysicalDeviceImageDrmFormatModifierInfoEXT drm_info = {
            .sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
            .drmFormatModifier = drm_explicit_info.drmFormatModifier,
            .sharingMode       = img_info.sharingMode,
        };

        const VkPhysicalDeviceExternalImageFormatInfoKHR ext_fmt_info = {
            .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR,
            .pNext      = &drm_info,
            .handleType = ext_mem_info.handleTypes,
        };

        const VkPhysicalDeviceImageFormatInfo2KHR fmt_info = {
            .sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR,
            .pNext  = &ext_fmt_info,
            .format = img_info.format,
            .type   = img_info.imageType,
            .tiling = img_info.tiling,
            .usage  = img_info.usage,
            .flags  = img_info.flags,
        };

        VkExternalImageFormatPropertiesKHR ext_fmt_props = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR,
        };

        VkImageFormatProperties2KHR fmt_props = {
            .pNext = &ext_fmt_props,
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR,
        };

        VkResult res = vkGetPhysicalDeviceImageFormatProperties2(vk->phy_device, &fmt_info, &fmt_props);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not get image format properties: %s", ngli_vk_res2str(res));
            return NGL_ERROR_GRAPHICS_GENERIC;
        }

        const VkExtent3D max = fmt_props.imageFormatProperties.maxExtent;
        if (width > max.width || height > max.height) {
            LOG(ERROR, "plane dimensions (%dx%d) exceed GPU limits (%dx%d)",
                width, height, max.width, max.height);
            return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
        }

        res = vkCreateImage(vk->device, &img_info, NULL, &vaapi->images[i]);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "failed to create image: %s", ngli_vk_res2str(res));
            return NGL_ERROR_GRAPHICS_GENERIC;
        }

        VkMemoryDedicatedRequirements mem_ded_reqs = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR,
        };

        VkMemoryRequirements2 mem_reqs = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
            .pNext = &mem_ded_reqs,
        };

        const VkImageMemoryRequirementsInfo2 mem_reqs_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
            .image = vaapi->images[i],
        };
        vkGetImageMemoryRequirements2(vk->device, &mem_reqs_info, &mem_reqs);

        VkMemoryFdPropertiesKHR fd_props = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
        };
        res = vk->GetMemoryFdPropertiesKHR(vk->device,
                                           VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                           fd,
                                           &fd_props);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not get fd properties (fd=%d): %s", fd, ngli_vk_res2str(res));
            return NGL_ERROR_GRAPHICS_GENERIC;
        }

        vaapi->fds[i] = dup(fd);
        if (vaapi->fds[i] == -1) {
            LOG(ERROR, "could not dup file descriptor (fd=%d)", fd);
            return NGL_ERROR_EXTERNAL;
        }

        VkImportMemoryFdInfoKHR fd_info = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
            .fd = vaapi->fds[i],
        };

        const VkMemoryDedicatedAllocateInfoKHR mem_ded_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
            .image = vaapi->images[i],
        };

        VkMemoryAllocateInfo mem_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &fd_info,
            .allocationSize = size,
        };
        if (mem_ded_reqs.prefersDedicatedAllocation) {
            fd_info.pNext = &mem_ded_alloc_info,
            mem_alloc_info.allocationSize = mem_reqs.memoryRequirements.size;
        }

        res = vkAllocateMemory(vk->device, &mem_alloc_info, NULL, &vaapi->memories[i]);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not allocate memory: %s", ngli_vk_res2str(res));
            return NGL_ERROR_GRAPHICS_MEMORY;
        }
        /*
         * According to the VkImportMemoryFdInfoKHR documentation, importing
         * the memory from a file descriptor transfers ownership of the file
         * descriptor to the Vulkan implementation. Moreover, the application
         * must not perform any operation on the file descriptor after a
         * successful import.
         * See: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkImportMemoryFdInfoKHR.html
         */
        vaapi->fds[i] = -1;

        res = vkBindImageMemory(vk->device, vaapi->images[i], vaapi->memories[i], 0);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not bind image memory: %s", ngli_vk_res2str(res));
            return NGL_ERROR_GRAPHICS_GENERIC;
        }

        vaapi->planes[i] = ngpu_texture_create(gpu_ctx);
        if (!vaapi->planes[i])
            return NGL_ERROR_MEMORY;

        const struct ngpu_texture_params plane_params = {
            .type             = NGPU_TEXTURE_TYPE_2D,
            .format           = ngl_format,
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
            .image        = vaapi->images[i],
            .image_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        res = ngpu_texture_vk_wrap(vaapi->planes[i], &wrap_params);
        if (res != VK_SUCCESS)
            return NGL_ERROR_GRAPHICS_GENERIC;

        ngpu_texture_vk_transition_layout(vaapi->planes[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        hwmap->mapped_image.planes[i] = vaapi->planes[i];
    }

    return 0;
}

const struct hwmap_class ngli_hwmap_vaapi_vk_class = {
    .name      = "vaapi (dma buf → vk image)",
    .hwformat  = NMD_PIXFMT_VAAPI,
    .layouts   = (const enum image_layout[]){
        NGLI_IMAGE_LAYOUT_NV12,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_vaapi),
    .init      = vaapi_init,
    .map_frame = vaapi_map_frame,
    .uninit    = vaapi_uninit,
};
