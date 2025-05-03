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

#ifndef NGPU_TEXTURE_VK_H
#define NGPU_TEXTURE_VK_H

#include <vulkan/vulkan.h>

#include "ngpu/buffer.h"
#include "ngpu/texture.h"
#include "vkcontext.h"
#include "ycbcr_sampler_vk.h"

struct ngpu_texture_vk_wrap_params {
    const struct ngpu_texture_params *params;
    VkImage image;
    VkImageLayout image_layout;
    VkImageView image_view;
    VkSampler sampler;
    struct ycbcr_sampler_vk *ycbcr_sampler;
};

struct ngpu_texture_vk {
    struct ngpu_texture parent;
    VkFormat format;
    size_t bytes_per_pixel;
    uint32_t array_layers;
    uint32_t mipmap_levels;
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
    struct ngpu_buffer *staging_buffer;
    void *staging_buffer_ptr;
    struct ngpu_texture_transfer_params last_transfer_params;
};

struct ngpu_texture *ngpu_texture_vk_create(struct ngpu_ctx *gpu_ctx);
int ngpu_texture_vk_init(struct ngpu_texture *s, const struct ngpu_texture_params *params);
VkResult ngpu_texture_vk_wrap(struct ngpu_texture *s, const struct ngpu_texture_vk_wrap_params *wrap_params);
int ngpu_texture_vk_upload(struct ngpu_texture *s, const uint8_t *data, int linesize);
int ngpu_texture_vk_upload_with_params(struct ngpu_texture *s, const uint8_t *data, const struct ngpu_texture_transfer_params *transfer_params);
int ngpu_texture_vk_generate_mipmap(struct ngpu_texture *s);
void ngpu_texture_vk_transition_layout(struct ngpu_texture *s, VkImageLayout layout);
void ngpu_texture_vk_transition_to_default_layout(struct ngpu_texture *s);
void ngpu_texture_vk_copy_to_buffer(struct ngpu_texture *s, struct ngpu_buffer *buffer);
void ngpu_texture_vk_freep(struct ngpu_texture **sp);

VkFilter ngpu_vk_get_filter(enum ngpu_filter filter);
VkImageUsageFlags ngpu_vk_get_image_usage_flags(uint32_t usage);

#endif
