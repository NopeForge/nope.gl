/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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
#include "ctx_vk.h"
#include "format_vk.h"
#include "log.h"
#include "ngpu/format.h"
#include "utils/memory.h"
#include "texture_vk.h"
#include "utils/bits.h"
#include "utils/utils.h"
#include "vkutils.h"

static const VkFilter vk_filter_map[NGPU_NB_FILTER] = {
    [NGPU_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [NGPU_FILTER_LINEAR]  = VK_FILTER_LINEAR,
};

VkFilter ngpu_vk_get_filter(enum ngpu_filter filter)
{
    return vk_filter_map[filter];
}

static const VkSamplerMipmapMode vk_mipmap_mode_map[NGPU_NB_MIPMAP] = {
    [NGPU_MIPMAP_FILTER_NONE]    = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [NGPU_MIPMAP_FILTER_NEAREST] = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [NGPU_MIPMAP_FILTER_LINEAR]  = VK_SAMPLER_MIPMAP_MODE_LINEAR,
};

static VkSamplerMipmapMode get_vk_mipmap_mode(enum ngpu_mipmap_filter mipmap_filter)
{
    return vk_mipmap_mode_map[mipmap_filter];
}

static const VkSamplerAddressMode vk_wrap_map[NGPU_NB_WRAP] = {
    [NGPU_WRAP_CLAMP_TO_EDGE]   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    [NGPU_WRAP_MIRRORED_REPEAT] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    [NGPU_WRAP_REPEAT]          = VK_SAMPLER_ADDRESS_MODE_REPEAT,
};

static VkSamplerAddressMode get_vk_wrap(enum ngpu_wrap wrap)
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

static const VkImageType image_type_map[NGPU_TEXTURE_TYPE_NB] = {
    [NGPU_TEXTURE_TYPE_2D]       = VK_IMAGE_TYPE_2D,
    [NGPU_TEXTURE_TYPE_2D_ARRAY] = VK_IMAGE_TYPE_2D,
    [NGPU_TEXTURE_TYPE_3D]       = VK_IMAGE_TYPE_3D,
    [NGPU_TEXTURE_TYPE_CUBE]     = VK_IMAGE_TYPE_2D,
};

static VkImageType get_vk_image_type(enum ngpu_texture_type type)
{
    return image_type_map[type];
}

static const VkImageViewType image_view_type_map[NGPU_TEXTURE_TYPE_NB] = {
    [NGPU_TEXTURE_TYPE_2D]       = VK_IMAGE_VIEW_TYPE_2D,
    [NGPU_TEXTURE_TYPE_2D_ARRAY] = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    [NGPU_TEXTURE_TYPE_3D]       = VK_IMAGE_VIEW_TYPE_3D,
    [NGPU_TEXTURE_TYPE_CUBE]     = VK_IMAGE_VIEW_TYPE_CUBE,
};

static VkImageViewType get_vk_image_view_type(enum ngpu_texture_type type)
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

VkImageUsageFlags ngpu_vk_get_image_usage_flags(uint32_t usage)
{
    return (usage & NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT             ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT             : 0)
         | (usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT             ? VK_IMAGE_USAGE_TRANSFER_DST_BIT             : 0)
         | (usage & NGPU_TEXTURE_USAGE_SAMPLED_BIT                  ? VK_IMAGE_USAGE_SAMPLED_BIT                  : 0)
         | (usage & NGPU_TEXTURE_USAGE_STORAGE_BIT                  ? VK_IMAGE_USAGE_STORAGE_BIT                  : 0)
         | (usage & NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         : 0)
         | (usage & NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0)
         | (usage & NGPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT     ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT     : 0);
}

static VkFormatFeatureFlags get_vk_format_features(uint32_t usage)
{
    return (usage & NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT             ? VK_FORMAT_FEATURE_TRANSFER_SRC_BIT             : 0)
         | (usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT             ? VK_FORMAT_FEATURE_TRANSFER_DST_BIT             : 0)
         | (usage & NGPU_TEXTURE_USAGE_SAMPLED_BIT                  ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT            : 0)
         | (usage & NGPU_TEXTURE_USAGE_STORAGE_BIT                  ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT            : 0)
         | (usage & NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT         : 0)
         | (usage & NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);
}

static VkFormatFeatureFlags get_vk_texture_format_features(const struct ngpu_texture_params *params)
{
    VkFormatFeatureFlags features = get_vk_format_features(params->usage);

    if (params->usage & NGPU_TEXTURE_USAGE_SAMPLED_BIT && (
            params->min_filter != NGPU_FILTER_NEAREST ||
            params->mag_filter != NGPU_FILTER_NEAREST))
        features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    return features;
}

static int get_mipmap_levels(int32_t width, int32_t height)
{
    return ngli_log2(width | height | 1);
}

static int init_fields(struct ngpu_texture *s, const struct ngpu_texture_params *params)
{
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    s->params = *params;

    ngli_assert(params->width && params->height);

    uint32_t depth = 1;
    if (params->type == NGPU_TEXTURE_TYPE_3D) {
        ngli_assert(params->depth);
        depth = params->depth;
    }
    s->params.depth = depth;

    s_priv->format = ngpu_format_ngl_to_vk(s->params.format);
    s_priv->bytes_per_pixel = ngpu_format_get_bytes_per_pixel(s->params.format);

    s_priv->array_layers = 1;
    if (params->type == NGPU_TEXTURE_TYPE_CUBE) {
        s_priv->array_layers = 6;
    } else if (params->type == NGPU_TEXTURE_TYPE_2D_ARRAY) {
        s_priv->array_layers = params->depth;
    }

    s_priv->mipmap_levels = 1;
    if (params->mipmap_filter != NGPU_MIPMAP_FILTER_NONE) {
        s_priv->mipmap_levels = get_mipmap_levels(params->width, params->height);
    }

    const VkImageUsageFlags usage = ngpu_vk_get_image_usage_flags(s->params.usage);
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

static VkResult create_image_view(struct ngpu_texture *s)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

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

static VkResult create_sampler(struct ngpu_texture *s)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    const VkSamplerCreateInfo sampler_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = ngpu_vk_get_filter(s->params.mag_filter),
        .minFilter               = ngpu_vk_get_filter(s->params.min_filter),
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
        .maxLod                  = (float)s_priv->mipmap_levels,
        .mipLodBias              = 0.0f,
    };
    return vkCreateSampler(vk->device, &sampler_info, NULL, &s_priv->sampler);
}

struct ngpu_texture *ngpu_texture_vk_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_texture_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_texture *)s;
}

static VkResult texture_vk_init(struct ngpu_texture *s, const struct ngpu_texture_params *params)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

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

    const VkFormatFeatureFlags features = get_vk_texture_format_features(params);
    if ((properties.optimalTilingFeatures & features) != features) {
        LOG(ERROR, "unsupported format %d, supported features: 0x%x, requested features: 0x%x",
            s_priv->format, supported_features, features);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    VkImageCreateFlags flags = 0;
    if (s->params.type == NGPU_TEXTURE_TYPE_CUBE) {
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    } else if (s->params.type == NGPU_TEXTURE_TYPE_3D) {
        flags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
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
        .usage         = ngpu_vk_get_image_usage_flags(s->params.usage),
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

    int mem_type_index = NGL_ERROR_NOT_FOUND;
    if (s->params.usage & NGPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
        const VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_props);
    }

    if (mem_type_index < 0) {
        const VkMemoryPropertyFlags mem_pros = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_pros);
        if (mem_type_index < 0)
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    const VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_reqs.size,
        .memoryTypeIndex = mem_type_index,
    };
    res = vkAllocateMemory(vk->device, &alloc_info, NULL, &s_priv->image_memory);
    if (res != VK_SUCCESS)
        return res;

    res = vkBindImageMemory(vk->device, s_priv->image, s_priv->image_memory, 0);
    if (res != VK_SUCCESS)
        return res;

    struct ngpu_cmd_buffer_vk *cmd_buffer_vk;
    res = ngpu_cmd_buffer_vk_begin_transient(s->gpu_ctx, 0, &cmd_buffer_vk);
    if (res != VK_SUCCESS)
        return res;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };

    transition_image_layout(cmd_buffer_vk->cmd_buf,
                            s_priv->image,
                            s_priv->image_layout,
                            s_priv->default_image_layout,
                            &subres_range);

    res = ngpu_cmd_buffer_vk_execute_transient(&cmd_buffer_vk);
    if (res != VK_SUCCESS)
        return res;

    s_priv->image_layout = s_priv->default_image_layout;

    res = create_image_view(s);
    if (res != VK_SUCCESS)
        return res;

    return create_sampler(s);
}

int ngpu_texture_vk_init(struct ngpu_texture *s, const struct ngpu_texture_params *params)
{
    VkResult res = texture_vk_init(s, params);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize texture: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

VkResult ngpu_texture_vk_wrap(struct ngpu_texture *s, const struct ngpu_texture_vk_wrap_params *wrap_params)
{
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

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

void ngpu_texture_vk_transition_layout(struct ngpu_texture *s, VkImageLayout layout)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    if (s_priv->image_layout == layout)
        return;

    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, s);

    VkCommandBuffer cmd_buf = cmd_buffer_vk->cmd_buf;
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

void ngpu_texture_vk_transition_to_default_layout(struct ngpu_texture *s)
{
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;
    ngpu_texture_vk_transition_layout(s, s_priv->default_image_layout);
}

void ngpu_texture_vk_copy_to_buffer(struct ngpu_texture *s, struct ngpu_buffer *buffer)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;
    struct ngpu_buffer_vk *buffer_vk = (struct ngpu_buffer_vk *)buffer;

    ngpu_texture_vk_transition_layout(s, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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

    VkCommandBuffer cmd_buf = gpu_ctx_vk->cur_cmd_buffer->cmd_buf;
    vkCmdCopyImageToBuffer(cmd_buf, s_priv->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer_vk->buffer, 1, &region);
}

static void destroy_staging_buffer(struct ngpu_texture *s)
{
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    if (s_priv->staging_buffer_ptr) {
        ngpu_buffer_unmap(s_priv->staging_buffer);
        s_priv->staging_buffer_ptr = NULL;
    }
    ngpu_buffer_freep(&s_priv->staging_buffer);
}

static int create_staging_buffer(struct ngpu_texture *s, size_t size)
{
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    s_priv->staging_buffer = ngpu_buffer_create(s->gpu_ctx);
    if (!s_priv->staging_buffer)
        return NGL_ERROR_MEMORY;

    const uint32_t usage = NGPU_BUFFER_USAGE_DYNAMIC_BIT |
                           NGPU_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           NGPU_BUFFER_USAGE_MAP_WRITE;
    int ret = ngpu_buffer_init(s_priv->staging_buffer, size, usage);
    if (ret < 0)
        return ret;

    ret = ngpu_buffer_map(s_priv->staging_buffer, 0, size, &s_priv->staging_buffer_ptr);
    if (ret < 0)
        return ret;

    return 0;
}

static int texture_transfer_params_are_equal(const struct ngpu_texture_transfer_params *a, const struct ngpu_texture_transfer_params *b)
{
    return a->x              == b->x              &&
           a->y              == b->y              &&
           a->z              == b->z              &&
           a->width          == b->width          &&
           a->height         == b->height         &&
           a->depth          == b->depth          &&
           a->pixels_per_row == b->pixels_per_row &&
           a->base_layer     == b->base_layer     &&
           a->layer_count    == b->layer_count;
}

static VkResult texture_vk_upload(struct ngpu_texture *s, const uint8_t *data, const struct ngpu_texture_transfer_params *transfer_params)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    const struct ngpu_texture_params *params = &s->params;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    /* Wrapped textures cannot update their content with this function */
    ngli_assert(!s_priv->wrapped_image);
    ngli_assert(params->usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT);

    if (!data)
        return VK_SUCCESS;

    const size_t transfer_layer_size = transfer_params->pixels_per_row * transfer_params->height * transfer_params->depth * s_priv->bytes_per_pixel;
    const size_t transfer_size = transfer_layer_size * transfer_params->layer_count;

    if (s_priv->staging_buffer)
        ngpu_buffer_wait(s_priv->staging_buffer);

    if (!texture_transfer_params_are_equal(&s_priv->last_transfer_params, transfer_params)) {
        destroy_staging_buffer(s);

        int ret = create_staging_buffer(s, transfer_size);
        if (ret < 0)
            return VK_ERROR_UNKNOWN;

        s_priv->last_transfer_params = *transfer_params;
    }

    memcpy(s_priv->staging_buffer_ptr, data, s_priv->staging_buffer->size);

    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    const int cmd_is_transient = cmd_buffer_vk == NULL || ngpu_ctx_is_render_pass_active(gpu_ctx);
    if (cmd_is_transient) {
        VkResult res = ngpu_cmd_buffer_vk_begin_transient(gpu_ctx, 0, &cmd_buffer_vk);
        if (res != VK_SUCCESS)
            return res;
    }
    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, s);
    ngpu_cmd_buffer_vk_ref_buffer(cmd_buffer_vk, s_priv->staging_buffer);

    VkCommandBuffer cmd_buf = cmd_buffer_vk->cmd_buf;

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

    for (int32_t i = transfer_params->base_layer; i < transfer_params->layer_count; i++) {
        const VkDeviceSize offset = i * transfer_layer_size;
        const VkBufferImageCopy region = {
            .bufferOffset      = offset,
            .bufferRowLength   = transfer_params->pixels_per_row,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
                .mipLevel       = 0,
                .baseArrayLayer = i,
                .layerCount     = 1,
            },
            .imageOffset = {
                transfer_params->x,
                transfer_params->y,
                transfer_params->z,
            },
            .imageExtent = {
                transfer_params->width,
                transfer_params->height,
                transfer_params->depth,
            },
        };

        if (!ngli_darray_push(&copy_regions, &region)) {
            ngli_darray_reset(&copy_regions);
            if (cmd_is_transient) {
                ngpu_cmd_buffer_vk_freep(&cmd_buffer_vk);
            }
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    struct ngpu_buffer_vk *staging_buffer_vk = (struct ngpu_buffer_vk *)s_priv->staging_buffer;
    vkCmdCopyBufferToImage(cmd_buf,
                           staging_buffer_vk->buffer,
                           s_priv->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (uint32_t)ngli_darray_count(&copy_regions),
                           ngli_darray_data(&copy_regions));

    ngli_darray_reset(&copy_regions);

    transition_image_layout(cmd_buf,
                            s_priv->image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            s_priv->image_layout,
                            &subres_range);

    if (cmd_is_transient) {
        VkResult res = ngpu_cmd_buffer_vk_execute_transient(&cmd_buffer_vk);
        if (res != VK_SUCCESS)
            return res;
    }

    if (params->mipmap_filter != NGPU_MIPMAP_FILTER_NONE)
        ngpu_texture_generate_mipmap(s);

    return VK_SUCCESS;
}

int ngpu_texture_vk_upload(struct ngpu_texture *s, const uint8_t *data, int linesize)
{
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;
    const struct ngpu_texture_params *params = &s->params;
    const struct ngpu_texture_transfer_params transfer_params = {
        .width = params->width,
        .height = params->height,
        .depth = params->depth,
        .base_layer = 0,
        .layer_count = s_priv->array_layers,
        .pixels_per_row = linesize ? linesize : params->width,
    };
    return ngpu_texture_vk_upload_with_params(s, data, &transfer_params);
}

int ngpu_texture_vk_upload_with_params(struct ngpu_texture *s, const uint8_t *data, const struct ngpu_texture_transfer_params *transfer_params)
{
    VkResult res = texture_vk_upload(s, data, transfer_params);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to upload texture: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static VkResult texture_vk_generate_mipmap(struct ngpu_texture *s)
{
    struct ngpu_ctx *gpu_ctx = (struct ngpu_ctx *)s->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    const struct ngpu_texture_params *params = &s->params;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;

    ngli_assert(params->usage & NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT);
    ngli_assert(params->usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT);

    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    const int cmd_is_transient = cmd_buffer_vk == NULL || ngpu_ctx_is_render_pass_active(gpu_ctx);
    if (cmd_is_transient) {
        VkResult res = ngpu_cmd_buffer_vk_begin_transient(gpu_ctx, 0, &cmd_buffer_vk);
        if (res != VK_SUCCESS)
            return res;
    }
    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, s);

    VkCommandBuffer cmd_buf = cmd_buffer_vk->cmd_buf;

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
            .layerCount     = s_priv->array_layers,
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
                .layerCount     = s_priv->array_layers,
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

    if (cmd_is_transient) {
        VkResult res = ngpu_cmd_buffer_vk_execute_transient(&cmd_buffer_vk);
        if (res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
}

int ngpu_texture_vk_generate_mipmap(struct ngpu_texture *s)
{
    VkResult res = texture_vk_generate_mipmap(s);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to generate texture mipmap: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

void ngpu_texture_vk_freep(struct ngpu_texture **sp)
{
    if (!*sp)
        return;

    struct ngpu_texture *s = *sp;
    struct ngpu_texture_vk *s_priv = (struct ngpu_texture_vk *)s;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    ngli_ycbcr_sampler_vk_unrefp(&s_priv->ycbcr_sampler);
    if (!s_priv->wrapped_sampler)
        vkDestroySampler(vk->device, s_priv->sampler, NULL);
    if (!s_priv->wrapped_image_view)
        vkDestroyImageView(vk->device, s_priv->image_view, NULL);
    if (!s_priv->wrapped_image)
        vkDestroyImage(vk->device, s_priv->image, NULL);
    vkFreeMemory(vk->device, s_priv->image_memory, NULL);

    destroy_staging_buffer(s);

    ngli_freep(sp);
}
