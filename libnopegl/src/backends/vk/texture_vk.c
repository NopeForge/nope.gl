/*
 * Copyright 2019-2022 GoPro Inc.
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

#include <string.h>
#include <stdint.h>

#include "buffer_vk.h"
#include "format.h"
#include "format_vk.h"
#include "gpu_ctx_vk.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "texture_vk.h"
#include "utils.h"
#include "vkutils.h"

static const VkFilter vk_filter_map[NGLI_NB_FILTER] = {
    [NGLI_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [NGLI_FILTER_LINEAR]  = VK_FILTER_LINEAR,
};

VkFilter ngli_vk_get_filter(int filter)
{
    return vk_filter_map[filter];
}

static const VkSamplerMipmapMode vk_mipmap_mode_map[NGLI_NB_MIPMAP] = {
    [NGLI_MIPMAP_FILTER_NONE]    = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [NGLI_MIPMAP_FILTER_NEAREST] = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [NGLI_MIPMAP_FILTER_LINEAR]  = VK_SAMPLER_MIPMAP_MODE_LINEAR,
};

static VkSamplerMipmapMode get_vk_mipmap_mode(int mipmap_filter)
{
    return vk_mipmap_mode_map[mipmap_filter];
}

static const VkSamplerAddressMode vk_wrap_map[NGLI_NB_WRAP] = {
    [NGLI_WRAP_CLAMP_TO_EDGE]   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    [NGLI_WRAP_MIRRORED_REPEAT] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    [NGLI_WRAP_REPEAT]          = VK_SAMPLER_ADDRESS_MODE_REPEAT,
};

static VkSamplerAddressMode get_vk_wrap(int wrap)
{
    return vk_wrap_map[wrap];
}

static VkImageAspectFlags get_vk_image_aspect_flags(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

static const VkImageType image_type_map[NGLI_TEXTURE_TYPE_NB] = {
    [NGLI_TEXTURE_TYPE_2D]   = VK_IMAGE_TYPE_2D,
    [NGLI_TEXTURE_TYPE_3D]   = VK_IMAGE_TYPE_3D,
    [NGLI_TEXTURE_TYPE_CUBE] = VK_IMAGE_TYPE_2D,
};

static VkImageType get_vk_image_type(int type)
{
    return image_type_map[type];
}

static const VkImageViewType image_view_type_map[NGLI_TEXTURE_TYPE_NB] = {
    [NGLI_TEXTURE_TYPE_2D]   = VK_IMAGE_VIEW_TYPE_2D,
    [NGLI_TEXTURE_TYPE_3D]   = VK_IMAGE_VIEW_TYPE_3D,
    [NGLI_TEXTURE_TYPE_CUBE] = VK_IMAGE_VIEW_TYPE_CUBE,
};

static VkImageViewType get_vk_image_view_type(int type)
{
    return image_view_type_map[type];
}

static VkAccessFlagBits get_vk_access_mask_from_image_layout(VkImageLayout layout, int dst_mask)
{
    VkPipelineStageFlags access_mask = 0;
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        ngli_assert(!dst_mask);
        break;
    case VK_IMAGE_LAYOUT_GENERAL:
        access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        ngli_assert(dst_mask);
        access_mask = VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        /* TODO */
        break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
        /* TODO */
        break;
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        /* TODO */
        break;
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        access_mask = VK_ACCESS_MEMORY_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
        /* TODO */
        break;
    default:
        LOG(ERROR, "unexpected image layout: 0x%x", layout);
        ngli_assert(0);
    }

    return access_mask;
}

static void transition_image_layout(VkCommandBuffer cmd_buf,
                                    VkImage image,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout,
                                    const VkImageSubresourceRange *subres_range)
{
    const VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    const VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = get_vk_access_mask_from_image_layout(old_layout, 0),
        .dstAccessMask       = get_vk_access_mask_from_image_layout(new_layout, 1),
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = *subres_range,
    };

    vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

VkImageUsageFlags ngli_vk_get_image_usage_flags(int usage)
{
    return (usage & NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT             ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT             ? VK_IMAGE_USAGE_TRANSFER_DST_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_SAMPLED_BIT                  ? VK_IMAGE_USAGE_SAMPLED_BIT                  : 0)
         | (usage & NGLI_TEXTURE_USAGE_STORAGE_BIT                  ? VK_IMAGE_USAGE_STORAGE_BIT                  : 0)
         | (usage & NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         : 0)
         | (usage & NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0)
         | (usage & NGLI_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT     ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT     : 0);
}

static VkFormatFeatureFlags get_vk_format_features(int usage)
{
    return (usage & NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT             ? VK_FORMAT_FEATURE_TRANSFER_SRC_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT             ? VK_FORMAT_FEATURE_TRANSFER_DST_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_SAMPLED_BIT                  ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT            : 0)
         | (usage & NGLI_TEXTURE_USAGE_STORAGE_BIT                  ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT            : 0)
         | (usage & NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT         : 0)
         | (usage & NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);
}

static int get_mipmap_levels(int width, int height)
{
    int mipmap_levels = 1;
    while ((width | height) >> mipmap_levels)
        mipmap_levels++;
    return mipmap_levels;
}

static int init_fields(struct texture *s, const struct texture_params *params)
{
    struct texture_vk *s_priv = (struct texture_vk *)s;

    s->params = *params;

    uint32_t depth = 1;
    if (params->type == NGLI_TEXTURE_TYPE_3D) {
        ngli_assert(params->depth);
        depth = params->depth;
    }
    s->params.depth = depth;

    s_priv->format = ngli_format_ngl_to_vk(s->params.format);
    s_priv->bytes_per_pixel = ngli_format_get_bytes_per_pixel(s->params.format);

    s_priv->array_layers = 1;
    if (params->type == NGLI_TEXTURE_TYPE_CUBE) {
        s_priv->array_layers = 6;
    }

    s_priv->mipmap_levels = 1;
    if (params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE) {
        s_priv->mipmap_levels = get_mipmap_levels(params->width, params->height);
    }

    const VkImageUsageFlags usage = ngli_vk_get_image_usage_flags(s->params.usage);
    s_priv->default_image_layout = VK_IMAGE_LAYOUT_GENERAL;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        s_priv->default_image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        s_priv->default_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        s_priv->default_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
        s_priv->default_image_layout = VK_IMAGE_LAYOUT_GENERAL;

    return 0;
}

static VkResult create_image_view(struct texture *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    const VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = s_priv->image,
        .viewType = get_vk_image_view_type(s->params.type),
        .format   = s_priv->format,
        .subresourceRange = {
            .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
            .baseMipLevel   = 0,
            .levelCount     = s_priv->mipmap_levels,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        }
    };

    return vkCreateImageView(vk->device, &view_info, NULL, &s_priv->image_view);
}

static VkResult create_sampler(struct texture *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    const VkSamplerCreateInfo sampler_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = ngli_vk_get_filter(s->params.mag_filter),
        .minFilter               = ngli_vk_get_filter(s->params.min_filter),
        .addressModeU            = get_vk_wrap(s->params.wrap_s),
        .addressModeV            = get_vk_wrap(s->params.wrap_t),
        .addressModeW            = get_vk_wrap(s->params.wrap_r),
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .mipmapMode              = get_vk_mipmap_mode(s->params.mipmap_filter),
        .minLod                  = 0.0f,
        .maxLod                  = s_priv->mipmap_levels,
        .mipLodBias              = 0.0f,
    };
    return vkCreateSampler(vk->device, &sampler_info, NULL, &s_priv->sampler);
}

struct texture *ngli_texture_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct texture_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct texture *)s;
}

VkResult ngli_texture_vk_init(struct texture *s, const struct texture_params *params)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    VkResult res = init_fields(s, params);
    if (res != VK_SUCCESS)
        return res;

    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(vk->phy_device, s_priv->format, &properties);
    const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

    VkFormatFeatureFlags supported_features;
    switch (tiling) {
    case VK_IMAGE_TILING_LINEAR:  supported_features = properties.linearTilingFeatures;  break;
    case VK_IMAGE_TILING_OPTIMAL: supported_features = properties.optimalTilingFeatures; break;
    default:
        ngli_assert(0);
    }

    const VkFormatFeatureFlags features = get_vk_format_features(s->params.usage);
    if ((properties.optimalTilingFeatures & features) != features) {
        LOG(ERROR, "unsupported format %d, supported features: 0x%x, requested features: 0x%x",
            s_priv->format, supported_features, features);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    VkImageCreateFlags flags = 0;
    if (s->params.type == NGLI_TEXTURE_TYPE_CUBE) {
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    const VkImageCreateInfo image_create_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = get_vk_image_type(s->params.type),
        .extent.width  = s->params.width,
        .extent.height = s->params.height,
        .extent.depth  = s->params.depth,
        .mipLevels     = s_priv->mipmap_levels,
        .arrayLayers   = s_priv->array_layers,
        .format        = s_priv->format,
        .tiling        = tiling,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = ngli_vk_get_image_usage_flags(s->params.usage),
        .samples       = ngli_ngl_samples_to_vk(s->params.samples),
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .flags         = flags,
    };

    res = vkCreateImage(vk->device, &image_create_info, NULL, &s_priv->image);
    if (res != VK_SUCCESS)
       return res;

    s_priv->image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(vk->device, s_priv->image, &mem_reqs);

    int lazy_allocated = 0;
    int mem_type_index = NGL_ERROR_NOT_FOUND;
    if (s->params.usage & NGLI_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
        const VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_props);
        lazy_allocated = mem_type_index >= 0;
    }

    if (mem_type_index < 0) {
        const VkMemoryPropertyFlags mem_pros = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_pros);
        if (mem_type_index < 0)
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    const VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = lazy_allocated ? 0 : mem_reqs.size,
        .memoryTypeIndex = mem_type_index,
    };
    res = vkAllocateMemory(vk->device, &alloc_info, NULL, &s_priv->image_memory);
    if (res != VK_SUCCESS)
        return res;

    res = vkBindImageMemory(vk->device, s_priv->image, s_priv->image_memory, 0);
    if (res != VK_SUCCESS)
        return res;

    struct cmd_vk *cmd_vk;
    res = ngli_cmd_vk_begin_transient(s->gpu_ctx, 0, &cmd_vk);
    if (res != VK_SUCCESS)
        return res;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };

    transition_image_layout(cmd_vk->cmd_buf,
                            s_priv->image,
                            s_priv->image_layout,
                            s_priv->default_image_layout,
                            &subres_range);

    res = ngli_cmd_vk_execute_transient(&cmd_vk);
    if (res != VK_SUCCESS)
        return res;

    s_priv->image_layout = s_priv->default_image_layout;

    res = create_image_view(s);
    if (res != VK_SUCCESS)
        return res;

    return create_sampler(s);
}

VkResult ngli_texture_vk_wrap(struct texture *s, const struct texture_vk_wrap_params *wrap_params)
{
    struct texture_vk *s_priv = (struct texture_vk *)s;

    VkResult res = init_fields(s, wrap_params->params);
    if (res != VK_SUCCESS)
        return res;

    s_priv->image = wrap_params->image;
    s_priv->wrapped_image = 1;
    s_priv->image_layout = wrap_params->image_layout;
    s_priv->image_view = wrap_params->image_view;
    s_priv->wrapped_image_view = wrap_params->image_view != VK_NULL_HANDLE;
    s_priv->sampler = wrap_params->sampler;
    s_priv->wrapped_sampler = wrap_params->sampler != VK_NULL_HANDLE;
    if (wrap_params->ycbcr_sampler) {
        ngli_assert(s_priv->sampler == VK_NULL_HANDLE);
        s_priv->use_ycbcr_sampler = 1;
        s_priv->ycbcr_sampler = ngli_ycbcr_sampler_vk_ref(wrap_params->ycbcr_sampler);
    }

    if (!s_priv->image_view) {
        res = create_image_view(s);
        if (res != VK_SUCCESS)
            return res;
    }

    if (!s_priv->sampler) {
        res = create_sampler(s);
        if (res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
}

void ngli_texture_vk_transition_layout(struct texture *s, VkImageLayout layout)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    if (s_priv->image_layout == layout)
        return;

    VkCommandBuffer cmd_buf = gpu_ctx_vk->cur_cmd->cmd_buf;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    transition_image_layout(cmd_buf, s_priv->image, s_priv->image_layout, layout, &subres_range);

    s_priv->image_layout = layout;
}

void ngli_texture_vk_transition_to_default_layout(struct texture *s)
{
    struct texture_vk *s_priv = (struct texture_vk *)s;
    ngli_texture_vk_transition_layout(s, s_priv->default_image_layout);
}

void ngli_texture_vk_copy_to_buffer(struct texture *s, struct buffer *buffer)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct texture_vk *s_priv = (struct texture_vk *)s;
    struct buffer_vk *buffer_vk = (struct buffer_vk *)buffer;

    ngli_texture_vk_transition_layout(s, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {s->params.width, s->params.height, 1},
    };

    VkCommandBuffer cmd_buf = gpu_ctx_vk->cur_cmd->cmd_buf;
    vkCmdCopyImageToBuffer(cmd_buf, s_priv->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer_vk->buffer, 1, &region);
}

VkResult ngli_texture_vk_upload(struct texture *s, const uint8_t *data, int linesize)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct texture_params *params = &s->params;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    /* Wrapped textures cannot update their content with this function */
    ngli_assert(!s_priv->wrapped_image);
    ngli_assert(params->usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT);

    if (!data)
        return VK_SUCCESS;

    if (!s_priv->staging_buffer || s_priv->staging_buffer_row_length != linesize) {
        const int32_t width = linesize ? linesize : s->params.width;
        const int32_t staging_buffer_size = width * s->params.height * s->params.depth * s_priv->bytes_per_pixel * s_priv->array_layers;

        if (s_priv->staging_buffer_ptr) {
            ngli_buffer_vk_unmap(s_priv->staging_buffer);
            s_priv->staging_buffer_ptr = NULL;
        }
        ngli_buffer_vk_freep(&s_priv->staging_buffer);

        s_priv->staging_buffer = ngli_buffer_vk_create(s->gpu_ctx);
        if (!s_priv->staging_buffer)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const int usage = NGLI_BUFFER_USAGE_DYNAMIC_BIT |
                          NGLI_BUFFER_USAGE_TRANSFER_SRC_BIT |
                          NGLI_BUFFER_USAGE_MAP_WRITE;
        VkResult res = ngli_buffer_vk_init(s_priv->staging_buffer, staging_buffer_size, usage);
        if (res != VK_SUCCESS)
            return res;

        s_priv->staging_buffer_row_length = linesize;

        res = ngli_buffer_vk_map(s_priv->staging_buffer, staging_buffer_size, 0, &s_priv->staging_buffer_ptr);
        if (res != VK_SUCCESS)
            return res;
    }

    memcpy(s_priv->staging_buffer_ptr, data, s_priv->staging_buffer->size);

    struct cmd_vk *cmd_vk = gpu_ctx_vk->cur_cmd;
    if (!cmd_vk) {
        VkResult res = ngli_cmd_vk_begin_transient(s->gpu_ctx, 0, &cmd_vk);
        if (res != VK_SUCCESS)
            return res;
    }
    VkCommandBuffer cmd_buf = cmd_vk->cmd_buf;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    transition_image_layout(cmd_buf,
                            s_priv->image,
                            s_priv->image_layout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            &subres_range);

    struct darray copy_regions;
    ngli_darray_init(&copy_regions, sizeof(VkBufferImageCopy), 0);

    const VkDeviceSize layer_size = s->params.width * s->params.height * s_priv->bytes_per_pixel;
    for (int32_t i = 0; i < s_priv->array_layers; i++) {
        const VkDeviceSize offset = i * layer_size;
        const VkBufferImageCopy region = {
            .bufferOffset      = offset,
            .bufferRowLength   = linesize,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
                .mipLevel       = 0,
                .baseArrayLayer = i,
                .layerCount     = 1,
            },
            .imageExtent = {
                params->width,
                params->height,
                params->depth,
            }
        };

        if (!ngli_darray_push(&copy_regions, &region)) {
            ngli_darray_reset(&copy_regions);
            if (!gpu_ctx_vk->cur_cmd) {
                ngli_cmd_vk_freep(&cmd_vk);
            }
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    struct buffer_vk *staging_buffer_vk = (struct buffer_vk *)s_priv->staging_buffer;
    vkCmdCopyBufferToImage(cmd_buf,
                           staging_buffer_vk->buffer,
                           s_priv->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           ngli_darray_count(&copy_regions),
                           ngli_darray_data(&copy_regions));

    ngli_darray_reset(&copy_regions);

    transition_image_layout(cmd_buf,
                            s_priv->image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            s_priv->image_layout,
                            &subres_range);

    if (!gpu_ctx_vk->cur_cmd) {
        VkResult res = ngli_cmd_vk_execute_transient(&cmd_vk);
        if (res != VK_SUCCESS)
            return res;
    }

    if (params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE)
        ngli_texture_generate_mipmap(s);

    return VK_SUCCESS;
}

VkResult ngli_texture_vk_generate_mipmap(struct texture *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct texture_params *params = &s->params;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    ngli_assert(params->usage & NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT);
    ngli_assert(params->usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT);

    struct cmd_vk *cmd_vk = gpu_ctx_vk->cur_cmd;
    if (!cmd_vk) {
        VkResult res = ngli_cmd_vk_begin_transient(s->gpu_ctx, 0, &cmd_vk);
        if (res != VK_SUCCESS)
            return res;
    }
    VkCommandBuffer cmd_buf = cmd_vk->cmd_buf;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    transition_image_layout(cmd_buf,
                            s_priv->image,
                            s_priv->image_layout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            &subres_range);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = s_priv->image,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseArrayLayer = 0,
            .layerCount     = params->type == NGLI_TEXTURE_TYPE_CUBE ? 6 : 1,
            .levelCount     = 1,
        }
    };

    int32_t mipmap_width  = s->params.width;
    int32_t mipmap_height = s->params.height;
    for (uint32_t i = 1; i < s_priv->mipmap_levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd_buf,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, NULL,
                             0, NULL,
                             1, &barrier);

        const VkImageBlit blit = {
            .srcSubresource = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = i - 1,
                .baseArrayLayer = 0,
                .layerCount     = s_priv->array_layers,
            },
            .srcOffsets = {
                {0, 0, 0},
                {mipmap_width, mipmap_height, 1},
            },
            .dstSubresource = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = i,
                .baseArrayLayer = 0,
                .layerCount     = params->type == NGLI_TEXTURE_TYPE_CUBE ? 6 : 1,
            },
            .dstOffsets = {
                {0, 0, 0},
                {NGLI_MAX(mipmap_width >> 1, 1), NGLI_MAX(mipmap_height >> 1, 1), 1},
            },
        };

        vkCmdBlitImage(cmd_buf,
                       s_priv->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       s_priv->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = s_priv->image_layout;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd_buf,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, NULL,
                             0, NULL,
                             1, &barrier);

        mipmap_width  = NGLI_MAX(mipmap_width >> 1, 1);
        mipmap_height = NGLI_MAX(mipmap_height >> 1, 1);
    }

    barrier.subresourceRange.baseMipLevel = s_priv->mipmap_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = s_priv->image_layout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buf,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &barrier);

    if (!gpu_ctx_vk->cur_cmd) {
        VkResult res = ngli_cmd_vk_execute_transient(&cmd_vk);
        if (res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
}

void ngli_texture_vk_freep(struct texture **sp)
{
    if (!*sp)
        return;

    struct texture *s = *sp;
    struct texture_vk *s_priv = (struct texture_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    ngli_ycbcr_sampler_vk_unrefp(&s_priv->ycbcr_sampler);
    if (!s_priv->wrapped_sampler)
        vkDestroySampler(vk->device, s_priv->sampler, NULL);
    if (!s_priv->wrapped_image_view)
        vkDestroyImageView(vk->device, s_priv->image_view, NULL);
    if (!s_priv->wrapped_image)
        vkDestroyImage(vk->device, s_priv->image, NULL);
    vkFreeMemory(vk->device, s_priv->image_memory, NULL);

    if (s_priv->staging_buffer_ptr)
        ngli_buffer_vk_unmap(s_priv->staging_buffer);
    ngli_buffer_freep(&s_priv->staging_buffer);

    ngli_freep(sp);
}
