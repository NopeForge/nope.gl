/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef GPU_BINDGROUP_VK_H
#define GPU_BINDGROUP_VK_H

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "gpu_bindgroup.h"

struct gpu_ctx;

struct gpu_bindgroup_layout_vk {
    struct gpu_bindgroup_layout parent;
    struct darray desc_set_layout_bindings; // array of VkDescriptorSetLayoutBinding
    struct darray immutable_samplers;       // array of ycbcr_sampler_vk pointers
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorPool desc_pool;
};

struct gpu_bindgroup_vk {
    struct gpu_bindgroup parent;
    struct darray texture_bindings;   // array of texture_binding_vk
    struct darray buffer_bindings;    // array of buffer_binding_vk
    VkDescriptorSet desc_set;
    struct darray write_desc_sets;    // array of VkWriteDescriptrSet
};

struct gpu_bindgroup_layout *ngli_gpu_bindgroup_layout_vk_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_bindgroup_layout_vk_init(struct gpu_bindgroup_layout *s);
void ngli_gpu_bindgroup_layout_vk_freep(struct gpu_bindgroup_layout **sp);

struct gpu_bindgroup *ngli_gpu_bindgroup_vk_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_bindgroup_vk_init(struct gpu_bindgroup *s, const struct gpu_bindgroup_params *params);
int ngli_gpu_bindgroup_vk_update_texture(struct gpu_bindgroup *s, int32_t index, const struct gpu_texture_binding *binding);
int ngli_gpu_bindgroup_vk_update_buffer(struct gpu_bindgroup *s, int32_t index, const struct gpu_buffer_binding *binding);
int ngli_gpu_bindgroup_vk_update_descriptor_set(struct gpu_bindgroup *s);
int ngli_gpu_bindgroup_vk_bind(struct gpu_bindgroup *s);
void ngli_gpu_bindgroup_vk_freep(struct gpu_bindgroup **sp);

#endif
