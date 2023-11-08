/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright Nope Forge
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

#include "format_vk.h"

VkFormat ngli_format_ngl_to_vk(int format)
{
    static const VkFormat format_map[] = {
        [NGLI_FORMAT_UNDEFINED]            = VK_FORMAT_UNDEFINED,
        [NGLI_FORMAT_R8_UNORM]             = VK_FORMAT_R8_UNORM,
        [NGLI_FORMAT_R8_SNORM]             = VK_FORMAT_R8_SNORM,
        [NGLI_FORMAT_R8_UINT]              = VK_FORMAT_R8_UINT,
        [NGLI_FORMAT_R8_SINT]              = VK_FORMAT_R8_SINT,
        [NGLI_FORMAT_R8G8_UNORM]           = VK_FORMAT_R8G8_UNORM,
        [NGLI_FORMAT_R8G8_SNORM]           = VK_FORMAT_R8G8_SNORM,
        [NGLI_FORMAT_R8G8_UINT]            = VK_FORMAT_R8G8_UINT,
        [NGLI_FORMAT_R8G8_SINT]            = VK_FORMAT_R8G8_SINT,
        [NGLI_FORMAT_R8G8B8_UNORM]         = VK_FORMAT_R8G8B8_UNORM,
        [NGLI_FORMAT_R8G8B8_SNORM]         = VK_FORMAT_R8G8B8_SNORM,
        [NGLI_FORMAT_R8G8B8_UINT]          = VK_FORMAT_R8G8B8_UINT,
        [NGLI_FORMAT_R8G8B8_SINT]          = VK_FORMAT_R8G8B8_SINT,
        [NGLI_FORMAT_R8G8B8_SRGB]          = VK_FORMAT_R8G8B8_SRGB,
        [NGLI_FORMAT_R8G8B8A8_UNORM]       = VK_FORMAT_R8G8B8A8_UNORM,
        [NGLI_FORMAT_R8G8B8A8_SNORM]       = VK_FORMAT_R8G8B8A8_SNORM,
        [NGLI_FORMAT_R8G8B8A8_UINT]        = VK_FORMAT_R8G8B8A8_UINT,
        [NGLI_FORMAT_R8G8B8A8_SINT]        = VK_FORMAT_R8G8B8A8_SINT,
        [NGLI_FORMAT_R8G8B8A8_SRGB]        = VK_FORMAT_R8G8B8A8_SRGB,
        [NGLI_FORMAT_B8G8R8A8_UNORM]       = VK_FORMAT_B8G8R8A8_UNORM,
        [NGLI_FORMAT_B8G8R8A8_SNORM]       = VK_FORMAT_B8G8R8A8_SNORM,
        [NGLI_FORMAT_B8G8R8A8_UINT]        = VK_FORMAT_B8G8R8A8_UINT,
        [NGLI_FORMAT_B8G8R8A8_SINT]        = VK_FORMAT_B8G8R8A8_SINT,
        [NGLI_FORMAT_R16_UNORM]            = VK_FORMAT_R16_UNORM,
        [NGLI_FORMAT_R16_SNORM]            = VK_FORMAT_R16_SNORM,
        [NGLI_FORMAT_R16_UINT]             = VK_FORMAT_R16_UINT,
        [NGLI_FORMAT_R16_SINT]             = VK_FORMAT_R16_SINT,
        [NGLI_FORMAT_R16_SFLOAT]           = VK_FORMAT_R16_SFLOAT,
        [NGLI_FORMAT_R16G16_UNORM]         = VK_FORMAT_R16G16_UNORM,
        [NGLI_FORMAT_R16G16_SNORM]         = VK_FORMAT_R16G16_SNORM,
        [NGLI_FORMAT_R16G16_UINT]          = VK_FORMAT_R16G16_UINT,
        [NGLI_FORMAT_R16G16_SINT]          = VK_FORMAT_R16G16_SINT,
        [NGLI_FORMAT_R16G16_SFLOAT]        = VK_FORMAT_R16G16_SFLOAT,
        [NGLI_FORMAT_R16G16B16_UNORM]      = VK_FORMAT_R16G16B16_UNORM,
        [NGLI_FORMAT_R16G16B16_SNORM]      = VK_FORMAT_R16G16B16_SNORM,
        [NGLI_FORMAT_R16G16B16_UINT]       = VK_FORMAT_R16G16B16_UINT,
        [NGLI_FORMAT_R16G16B16_SINT]       = VK_FORMAT_R16G16B16_SINT,
        [NGLI_FORMAT_R16G16B16_SFLOAT]     = VK_FORMAT_R16G16B16_SFLOAT,
        [NGLI_FORMAT_R16G16B16A16_UNORM]   = VK_FORMAT_R16G16B16A16_UNORM,
        [NGLI_FORMAT_R16G16B16A16_SNORM]   = VK_FORMAT_R16G16B16A16_SNORM,
        [NGLI_FORMAT_R16G16B16A16_UINT]    = VK_FORMAT_R16G16B16A16_UINT,
        [NGLI_FORMAT_R16G16B16A16_SINT]    = VK_FORMAT_R16G16B16A16_SINT,
        [NGLI_FORMAT_R16G16B16A16_SFLOAT]  = VK_FORMAT_R16G16B16A16_SFLOAT,
        [NGLI_FORMAT_R32_UINT]             = VK_FORMAT_R32_UINT,
        [NGLI_FORMAT_R32_SINT]             = VK_FORMAT_R32_SINT,
        [NGLI_FORMAT_R32_SFLOAT]           = VK_FORMAT_R32_SFLOAT,
        [NGLI_FORMAT_R32G32_UINT]          = VK_FORMAT_R32G32_UINT,
        [NGLI_FORMAT_R32G32_SINT]          = VK_FORMAT_R32G32_SINT,
        [NGLI_FORMAT_R32G32_SFLOAT]        = VK_FORMAT_R32G32_SFLOAT,
        [NGLI_FORMAT_R32G32B32_UINT]       = VK_FORMAT_R32G32B32_UINT,
        [NGLI_FORMAT_R32G32B32_SINT]       = VK_FORMAT_R32G32B32_SINT,
        [NGLI_FORMAT_R32G32B32_SFLOAT]     = VK_FORMAT_R32G32B32_SFLOAT,
        [NGLI_FORMAT_R32G32B32A32_UINT]    = VK_FORMAT_R32G32B32A32_UINT,
        [NGLI_FORMAT_R32G32B32A32_SINT]    = VK_FORMAT_R32G32B32A32_SINT,
        [NGLI_FORMAT_R32G32B32A32_SFLOAT]  = VK_FORMAT_R32G32B32A32_SFLOAT,
        [NGLI_FORMAT_D16_UNORM]            = VK_FORMAT_D16_UNORM,
        [NGLI_FORMAT_X8_D24_UNORM_PACK32]  = VK_FORMAT_X8_D24_UNORM_PACK32,
        [NGLI_FORMAT_D32_SFLOAT]           = VK_FORMAT_D32_SFLOAT,
        [NGLI_FORMAT_D24_UNORM_S8_UINT]    = VK_FORMAT_D24_UNORM_S8_UINT,
        [NGLI_FORMAT_D32_SFLOAT_S8_UINT]   = VK_FORMAT_D32_SFLOAT_S8_UINT,
        [NGLI_FORMAT_S8_UINT]              = VK_FORMAT_S8_UINT,
    };

    ngli_assert(format >= 0 && format < NGLI_ARRAY_NB(format_map));
    const VkFormat ret = format_map[format];
    ngli_assert(format == NGLI_FORMAT_UNDEFINED || ret);
    return ret;
}

int ngli_format_vk_to_ngl(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8_UNORM:            return NGLI_FORMAT_R8_UNORM;
    case VK_FORMAT_R8_SNORM:            return NGLI_FORMAT_R8_SNORM;
    case VK_FORMAT_R8_UINT:             return NGLI_FORMAT_R8_UINT;
    case VK_FORMAT_R8_SINT:             return NGLI_FORMAT_R8_SINT;
    case VK_FORMAT_R8G8_UNORM:          return NGLI_FORMAT_R8G8_UNORM;
    case VK_FORMAT_R8G8_SNORM:          return NGLI_FORMAT_R8G8_SNORM;
    case VK_FORMAT_R8G8_UINT:           return NGLI_FORMAT_R8G8_UINT;
    case VK_FORMAT_R8G8_SINT:           return NGLI_FORMAT_R8G8_SINT;
    case VK_FORMAT_R8G8B8_UNORM:        return NGLI_FORMAT_R8G8B8_UNORM;
    case VK_FORMAT_R8G8B8_SNORM:        return NGLI_FORMAT_R8G8B8_SNORM;
    case VK_FORMAT_R8G8B8_UINT:         return NGLI_FORMAT_R8G8B8_UINT;
    case VK_FORMAT_R8G8B8_SINT:         return NGLI_FORMAT_R8G8B8_SINT;
    case VK_FORMAT_R8G8B8_SRGB:         return NGLI_FORMAT_R8G8B8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:      return NGLI_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SNORM:      return NGLI_FORMAT_R8G8B8A8_SNORM;
    case VK_FORMAT_R8G8B8A8_UINT:       return NGLI_FORMAT_R8G8B8A8_UINT;
    case VK_FORMAT_R8G8B8A8_SINT:       return NGLI_FORMAT_R8G8B8A8_SINT;
    case VK_FORMAT_R8G8B8A8_SRGB:       return NGLI_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:      return NGLI_FORMAT_B8G8R8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SNORM:      return NGLI_FORMAT_B8G8R8A8_SNORM;
    case VK_FORMAT_B8G8R8A8_UINT:       return NGLI_FORMAT_B8G8R8A8_UINT;
    case VK_FORMAT_B8G8R8A8_SINT:       return NGLI_FORMAT_B8G8R8A8_SINT;
    case VK_FORMAT_R16_UNORM:           return NGLI_FORMAT_R16_UNORM;
    case VK_FORMAT_R16_SNORM:           return NGLI_FORMAT_R16_SNORM;
    case VK_FORMAT_R16_UINT:            return NGLI_FORMAT_R16_UINT;
    case VK_FORMAT_R16_SINT:            return NGLI_FORMAT_R16_SINT;
    case VK_FORMAT_R16_SFLOAT:          return NGLI_FORMAT_R16_SFLOAT;
    case VK_FORMAT_R16G16_UNORM:        return NGLI_FORMAT_R16G16_UNORM;
    case VK_FORMAT_R16G16_SNORM:        return NGLI_FORMAT_R16G16_SNORM;
    case VK_FORMAT_R16G16_UINT:         return NGLI_FORMAT_R16G16_UINT;
    case VK_FORMAT_R16G16_SINT:         return NGLI_FORMAT_R16G16_SINT;
    case VK_FORMAT_R16G16_SFLOAT:       return NGLI_FORMAT_R16G16_SFLOAT;
    case VK_FORMAT_R16G16B16_UNORM:     return NGLI_FORMAT_R16G16B16_UNORM;
    case VK_FORMAT_R16G16B16_SNORM:     return NGLI_FORMAT_R16G16B16_SNORM;
    case VK_FORMAT_R16G16B16_UINT:      return NGLI_FORMAT_R16G16B16_UINT;
    case VK_FORMAT_R16G16B16_SINT:      return NGLI_FORMAT_R16G16B16_SINT;
    case VK_FORMAT_R16G16B16_SFLOAT:    return NGLI_FORMAT_R16G16B16_SFLOAT;
    case VK_FORMAT_R16G16B16A16_UNORM:  return NGLI_FORMAT_R16G16B16A16_UNORM;
    case VK_FORMAT_R16G16B16A16_SNORM:  return NGLI_FORMAT_R16G16B16A16_SNORM;
    case VK_FORMAT_R16G16B16A16_UINT:   return NGLI_FORMAT_R16G16B16A16_UINT;
    case VK_FORMAT_R16G16B16A16_SINT:   return NGLI_FORMAT_R16G16B16A16_SINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return NGLI_FORMAT_R16G16B16A16_SFLOAT;
    case VK_FORMAT_R32_UINT:            return NGLI_FORMAT_R32_UINT;
    case VK_FORMAT_R32_SINT:            return NGLI_FORMAT_R32_SINT;
    case VK_FORMAT_R32_SFLOAT:          return NGLI_FORMAT_R32_SFLOAT;
    case VK_FORMAT_R32G32_UINT:         return NGLI_FORMAT_R32G32_UINT;
    case VK_FORMAT_R32G32_SINT:         return NGLI_FORMAT_R32G32_SINT;
    case VK_FORMAT_R32G32_SFLOAT:       return NGLI_FORMAT_R32G32_SFLOAT;
    case VK_FORMAT_R32G32B32_UINT:      return NGLI_FORMAT_R32G32B32_UINT;
    case VK_FORMAT_R32G32B32_SINT:      return NGLI_FORMAT_R32G32B32_SINT;
    case VK_FORMAT_R32G32B32_SFLOAT:    return NGLI_FORMAT_R32G32B32_SFLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:   return NGLI_FORMAT_R32G32B32A32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT:   return NGLI_FORMAT_R32G32B32A32_SINT;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return NGLI_FORMAT_R32G32B32A32_SFLOAT;
    case VK_FORMAT_D16_UNORM:           return NGLI_FORMAT_D16_UNORM;
    case VK_FORMAT_X8_D24_UNORM_PACK32: return NGLI_FORMAT_X8_D24_UNORM_PACK32;
    case VK_FORMAT_D32_SFLOAT:          return NGLI_FORMAT_D32_SFLOAT;
    case VK_FORMAT_D24_UNORM_S8_UINT:   return NGLI_FORMAT_D24_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:  return NGLI_FORMAT_D32_SFLOAT_S8_UINT;
    case VK_FORMAT_S8_UINT:             return NGLI_FORMAT_S8_UINT;
    default:                            ngli_assert(0);
    }
}

VkFormatFeatureFlags ngli_format_feature_ngl_to_vk(uint32_t features)
{
    VkFormatFeatureFlags flags = 0;
    if (features & NGLI_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (features & NGLI_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if (features & NGLI_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (features & NGLI_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
        flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    if (features & NGLI_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        flags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return flags;
}

uint32_t ngli_format_feature_vk_to_ngl(VkFormatFeatureFlags features)
{
    uint32_t flags = 0;
    if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        flags |= NGLI_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        flags |= NGLI_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        flags |= NGLI_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
        flags |= NGLI_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        flags |= NGLI_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return flags;
}
