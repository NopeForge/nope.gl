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

#ifndef TEXTURE_VK_H
#define TEXTURE_VK_H

#include <vulkan/vulkan.h>

#include "buffer.h"
#include "texture.h"
#include "vkcontext.h"
#include "ycbcr_sampler_vk.h"

struct texture_vk_wrap_params {
    const struct texture_params *params;
    VkImage image;
    VkImageLayout image_layout;
    VkImageView image_view;
    VkSampler sampler;
    struct ycbcr_sampler_vk *ycbcr_sampler;
};

struct texture_vk {
    struct texture parent;
    VkFormat format;
    int bytes_per_pixel;
    int array_layers;
    int mipmap_levels;
    VkImage image;
    int wrapped_image;
    VkImageLayout default_image_layout;
    VkImageLayout image_layout;
    VkDeviceMemory image_memory;
    VkImageView image_view;
    int wrapped_image_view;
    VkSampler sampler;
    int wrapped_sampler;
    int use_ycbcr_sampler;
    struct ycbcr_sampler_vk *ycbcr_sampler;
    struct buffer *staging_buffer;
    VkDeviceSize staging_buffer_row_length;
    void *staging_buffer_ptr;
};

struct texture *ngli_texture_vk_create(struct gpu_ctx *gpu_ctx);
VkResult ngli_texture_vk_init(struct texture *s, const struct texture_params *params);
VkResult ngli_texture_vk_wrap(struct texture *s, const struct texture_vk_wrap_params *wrap_params);
VkResult ngli_texture_vk_upload(struct texture *s, const uint8_t *data, int linesize);
VkResult ngli_texture_vk_generate_mipmap(struct texture *s);
void ngli_texture_vk_transition_layout(struct texture *s, VkImageLayout layout);
void ngli_texture_vk_transition_to_default_layout(struct texture *s);
void ngli_texture_vk_copy_to_buffer(struct texture *s, struct buffer *buffer);
void ngli_texture_vk_freep(struct texture **sp);

VkFilter ngli_vk_get_filter(int filter);
VkImageUsageFlags ngli_vk_get_image_usage_flags(int usage);

#endif
