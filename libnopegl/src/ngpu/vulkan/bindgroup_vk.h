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

#ifndef NGPU_BINDGROUP_VK_H
#define NGPU_BINDGROUP_VK_H

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "ngpu/bindgroup.h"

struct ngpu_ctx;

struct texture_binding_vk {
    struct ngpu_bindgroup_layout_entry layout_entry;
    const struct ngpu_texture *texture;
    int use_ycbcr_sampler;
    struct ycbcr_sampler_vk *ycbcr_sampler;
    uint32_t update_desc;
};

struct buffer_binding_vk {
    struct ngpu_bindgroup_layout_entry layout_entry;
    const struct ngpu_buffer *buffer;
    size_t offset;
    size_t size;
    uint32_t update_desc;
};

struct ngpu_bindgroup_layout_vk {
    struct ngpu_bindgroup_layout parent;
    struct darray desc_set_layout_bindings; // array of VkDescriptorSetLayoutBinding
    struct darray immutable_samplers;       // array of ycbcr_sampler_vk pointers
    VkDescriptorSetLayout desc_set_layout;
    uint32_t desc_pool_size_count;
    VkDescriptorPoolSize desc_pool_sizes[NGPU_TYPE_NB];
    uint32_t max_desc_sets;
    struct darray desc_pools;
    size_t desc_pool_index;
};

struct ngpu_bindgroup_vk {
    struct ngpu_bindgroup parent;
    struct darray texture_bindings;   // array of texture_binding_vk
    struct darray buffer_bindings;    // array of buffer_binding_vk
    VkDescriptorSet desc_set;
    struct darray write_desc_sets;    // array of VkWriteDescriptrSet
};

struct ngpu_bindgroup_layout *ngpu_bindgroup_layout_vk_create(struct ngpu_ctx *gpu_ctx);
int ngpu_bindgroup_layout_vk_init(struct ngpu_bindgroup_layout *s);
void ngpu_bindgroup_layout_vk_freep(struct ngpu_bindgroup_layout **sp);

struct ngpu_bindgroup *ngpu_bindgroup_vk_create(struct ngpu_ctx *gpu_ctx);
int ngpu_bindgroup_vk_init(struct ngpu_bindgroup *s, const struct ngpu_bindgroup_params *params);
int ngpu_bindgroup_vk_update_texture(struct ngpu_bindgroup *s, uint32_t index, const struct ngpu_texture_binding *binding);
int ngpu_bindgroup_vk_update_buffer(struct ngpu_bindgroup *s, uint32_t index, const struct ngpu_buffer_binding *binding);
int ngpu_bindgroup_vk_update_descriptor_set(struct ngpu_bindgroup *s);
void ngpu_bindgroup_vk_freep(struct ngpu_bindgroup **sp);

#endif
