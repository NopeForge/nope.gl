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

#ifndef RENDERTARGET_VK_H
#define RENDERTARGET_VK_H

#include <vulkan/vulkan.h>

#include "rendertarget.h"

struct rendertarget_vk {
    struct rendertarget parent;
    int nb_attachments;
    VkImageView attachments[2*(NGLI_MAX_COLOR_ATTACHMENTS + 1)];
    VkFramebuffer framebuffer;
    VkRenderPass render_pass;
    VkClearValue clear_values[2*(NGLI_MAX_COLOR_ATTACHMENTS + 1)];
    int nb_clear_values;
    VkBuffer staging_buffer;
    int staging_buffer_size;
    VkDeviceMemory staging_memory;
};

struct rendertarget *ngli_rendertarget_vk_create(struct gpu_ctx *gpu_ctx);
VkResult ngli_rendertarget_vk_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_vk_freep(struct rendertarget **sp);

VkResult ngli_vk_create_compatible_renderpass(struct gpu_ctx *s, const struct rendertarget_desc *desc, VkRenderPass *render_pass);

#endif
