/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_RENDERTARGET_VK_H
#define NGPU_RENDERTARGET_VK_H

#include <vulkan/vulkan.h>

#include "ngpu/rendertarget.h"

struct ngpu_rendertarget_vk {
    struct ngpu_rendertarget parent;
    uint32_t nb_attachments;
    VkImageView attachments[2*(NGPU_MAX_COLOR_ATTACHMENTS + 1)];
    struct ngpu_texture *attachments_refs[2*(NGPU_MAX_COLOR_ATTACHMENTS + 1)];
    VkFramebuffer framebuffer;
    VkRenderPass render_pass;
    VkClearValue clear_values[2*(NGPU_MAX_COLOR_ATTACHMENTS + 1)];
    uint32_t nb_clear_values;
    VkBuffer staging_buffer;
    int staging_buffer_size;
    VkDeviceMemory staging_memory;
};

struct ngpu_rendertarget *ngpu_rendertarget_vk_create(struct ngpu_ctx *gpu_ctx);
int ngpu_rendertarget_vk_init(struct ngpu_rendertarget *s);
void ngpu_rendertarget_vk_freep(struct ngpu_rendertarget **sp);

VkResult ngpu_vk_create_compatible_renderpass(struct ngpu_ctx *s, const struct ngpu_rendertarget_layout *layout, VkRenderPass *render_pass);

#endif
