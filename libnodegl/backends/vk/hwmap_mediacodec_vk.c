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
#include <sxplayer.h>
#include <libavcodec/mediacodec.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/hardware_buffer.h>

#include "android_imagereader.h"
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
#include "vkcontext.h"
#include "vkutils.h"
#include "ycbcr_sampler_vk.h"

struct hwmap_mc {
    struct android_image *android_image;
    struct texture *texture;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView image_view;
    struct ycbcr_sampler_vk *ycbcr_sampler;
};

static int support_direct_rendering(struct hwmap *hwmap)
{
    const struct hwmap_params *params = &hwmap->params;

    if (params->texture_mipmap_filter) {
        LOG(WARNING, "samplers with YCbCr conversion enabled do not support mipmapping: "
            "disabling direct rendering");
        return 0;
    } else if (params->texture_wrap_s != NGLI_WRAP_CLAMP_TO_EDGE ||
               params->texture_wrap_t != NGLI_WRAP_CLAMP_TO_EDGE) {
        LOG(WARNING, "samplers with YCbCr conversion enabled only support clamp to edge wrapping: "
            "disabling direct rendering");
        return 0;
    } else if (params->texture_min_filter != params->texture_mag_filter) {
        LOG(WARNING, "samplers with YCbCr conversion enabled must have the same min/mag filters: "
            "disabling direct_rendering");
        return 0;
    }

    return 1;
}

static int mc_init(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, &mc->texture);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

    return 0;
}

static void mc_release_frame_resources(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    hwmap->mapped_image.planes[0] = NULL;
    ngli_texture_freep(&mc->texture);

    vkDestroyImageView(vk->device, mc->image_view, NULL);
    mc->image_view = VK_NULL_HANDLE;

    vkDestroyImage(vk->device, mc->image, NULL);
    mc->image = VK_NULL_HANDLE;

    vkFreeMemory(vk->device, mc->memory, NULL);
    mc->memory = VK_NULL_HANDLE;

    ngli_android_image_freep(&mc->android_image);
}

static int mc_map_frame(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    const struct hwmap_params *params = &hwmap->params;
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct android_ctx *android_ctx = &ctx->android_ctx;

    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;
    int ret = av_mediacodec_release_buffer(buffer, 1);
    if (ret < 0)
        return ret;

    struct android_image *android_image;
    ret = ngli_android_imagereader_acquire_next_image(params->android_imagereader, &android_image);
    if (ret < 0)
        return ret;

    mc_release_frame_resources(hwmap);
    mc->android_image = android_image;

    AHardwareBuffer *hardware_buffer = ngli_android_image_get_hardware_buffer(mc->android_image);
    if (!hardware_buffer)
        return NGL_ERROR_EXTERNAL;

    AHardwareBuffer_Desc desc;
    android_ctx->AHardwareBuffer_describe(hardware_buffer, &desc);

    AImageCropRect crop_rect;
    ret = ngli_android_image_get_crop_rect(mc->android_image, &crop_rect);
    if (ret < 0)
        return ret;

    float *matrix = hwmap->mapped_image.coordinates_matrix;
    const int filtering = params->texture_min_filter || params->texture_mag_filter;
    ngli_android_get_crop_matrix(matrix, &desc, &crop_rect, filtering);

    VkAndroidHardwareBufferFormatPropertiesANDROID ahb_format_props = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
    };

    VkAndroidHardwareBufferPropertiesANDROID ahb_props = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
        .pNext = &ahb_format_props,
    };

    VkResult res = vk->GetAndroidHardwareBufferPropertiesANDROID(vk->device, hardware_buffer, &ahb_props);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not get android hardware buffer properties: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    VkExternalFormatANDROID external_format = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
        .externalFormat = 0,
    };

    if (ahb_format_props.format == VK_FORMAT_UNDEFINED)
        external_format.externalFormat = ahb_format_props.externalFormat;

    VkExternalMemoryImageCreateInfo external_memory_image_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = &external_format,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };

    VkImageUsageFlags usage_flags = 0;
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
        usage_flags = usage_flags | VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    VkImageCreateFlags create_flags = 0;
    if (desc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) {
        create_flags = VK_IMAGE_CREATE_PROTECTED_BIT;
    }

    VkImportAndroidHardwareBufferInfoANDROID import_ahb_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
        .buffer = hardware_buffer,
    };

    const VkImageCreateInfo img_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext         = &external_memory_image_info,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = {desc.width, desc.height, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = ahb_format_props.format,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = usage_flags,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .flags         = create_flags,
    };

    res = vkCreateImage(vk->device, &img_info, NULL, &mc->image);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not create image: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    VkMemoryDedicatedAllocateInfoKHR mem_ded_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
        .pNext = &import_ahb_info,
        .image = mc->image,
    };

    const VkMemoryRequirements mem_reqs = {
      .size = ahb_props.allocationSize,
      .memoryTypeBits = ahb_props.memoryTypeBits,
    };

    const VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const int32_t mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_props);
    if (mem_type_index < 0) {
        LOG(ERROR, "could not find required memory type");
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    const VkMemoryAllocateInfo mem_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &mem_ded_info,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type_index,
    };

    res = vkAllocateMemory(vk->device, &mem_info, NULL, &mc->memory);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not allocate memory: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_MEMORY;
    }

    res = vkBindImageMemory(vk->device, mc->image, mc->memory, 0);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not bind image memory: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    const struct ycbcr_sampler_vk_params sampler_params = {
        /* Conversion params */
        .android_external_format = external_format.externalFormat,
        .format                  = VK_FORMAT_UNDEFINED,
        .ycbcr_model             = ahb_format_props.suggestedYcbcrModel,
        .ycbcr_range             = ahb_format_props.suggestedYcbcrRange,
        .components              = ahb_format_props.samplerYcbcrConversionComponents,
        .x_chroma_offset         = ahb_format_props.suggestedXChromaOffset,
        .y_chroma_offset         = ahb_format_props.suggestedYChromaOffset,
        /* Sampler params */
        .filter                  = ngli_vk_get_filter(params->texture_min_filter),
    };

    if (!mc->ycbcr_sampler || !ngli_ycbcr_sampler_vk_is_compat(mc->ycbcr_sampler, &sampler_params)) {
        ngli_ycbcr_sampler_vk_unrefp(&mc->ycbcr_sampler);

        mc->ycbcr_sampler = ngli_ycbcr_sampler_vk_create(gpu_ctx);
        if (!mc->ycbcr_sampler)
            return NGL_ERROR_MEMORY;

        res = ngli_ycbcr_sampler_vk_init(mc->ycbcr_sampler, &sampler_params);
        if (res != VK_SUCCESS) {
            ngli_ycbcr_sampler_vk_unrefp(&mc->ycbcr_sampler);
            return res;
        }
    }

    const VkSamplerYcbcrConversionInfoKHR sampler_ycbcr_conv_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR,
        .conversion = mc->ycbcr_sampler->conv,
    };

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    const VkImageViewCreateInfo view_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext            = &sampler_ycbcr_conv_info,
        .image            = mc->image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_UNDEFINED,
        .subresourceRange = subres_range,
    };

    res = vkCreateImageView(vk->device, &view_info, NULL, &mc->image_view);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not create image view: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    const VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = vk->graphics_queue_index,
        .image               = mc->image,
        .subresourceRange    = subres_range,
    };

    VkCommandBuffer cmd_buf = gpu_ctx_vk->cur_cmd->cmd_buf;
    vkCmdPipelineBarrier(cmd_buf,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &barrier);

    mc->texture = ngli_texture_create(gpu_ctx);
    if (!mc->texture)
        return NGL_ERROR_MEMORY;

    int ngl_format = NGLI_FORMAT_UNDEFINED;
    if (ahb_format_props.format)
        ngl_format = ngli_format_ngl_to_vk(ahb_format_props.format);

    const struct texture_params texture_params = {
        .type             = NGLI_TEXTURE_TYPE_2D,
        .format           = ngl_format,
        .width            = desc.width,
        .height           = desc.height,
        .min_filter       = params->texture_min_filter,
        .mag_filter       = params->texture_min_filter,
        .wrap_s           = NGLI_WRAP_CLAMP_TO_EDGE,
        .wrap_t           = NGLI_WRAP_CLAMP_TO_EDGE,
        .usage            = params->texture_usage,
    };

    const struct texture_vk_wrap_params wrap_params = {
        .params        = &texture_params,
        .image         = mc->image,
        .image_layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image_view    = mc->image_view,
        .sampler       = VK_NULL_HANDLE,
        .ycbcr_sampler = mc->ycbcr_sampler,
    };

    res = ngli_texture_vk_wrap(mc->texture, &wrap_params);
    if (res != VK_SUCCESS)
        return NGL_ERROR_GRAPHICS_GENERIC;

    ngli_texture_vk_transition_to_default_layout(mc->texture);

    hwmap->mapped_image.planes[0] = mc->texture;

    return 0;
}

static void mc_uninit(struct hwmap *hwmap)
{
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;

    mc_release_frame_resources(hwmap);
    ngli_ycbcr_sampler_vk_unrefp(&mc->ycbcr_sampler);
}

const struct hwmap_class ngli_hwmap_mc_vk_class = {
    .name      = "mediacodec (hw buffer → vk image)",
    .hwformat  = SXPLAYER_PIXFMT_MEDIACODEC,
    .layouts   = (const int[]){
        NGLI_IMAGE_LAYOUT_DEFAULT,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .priv_size = sizeof(struct hwmap_mc),
    .init      = mc_init,
    .map_frame = mc_map_frame,
    .uninit    = mc_uninit,
};
