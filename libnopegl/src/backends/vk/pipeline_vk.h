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

#ifndef PIPELINE_VK_H
#define PIPELINE_VK_H

#include <vulkan/vulkan.h>

#include "pipeline.h"
#include "darray.h"

struct gpu_ctx;

struct pipeline_vk {
    struct pipeline parent;

    struct darray buffer_bindings;          // array of struct buffer_binding_vk
    struct darray texture_bindings;         // array of struct texture_binding_vk

    struct darray vertex_attribute_descs;   // array of VkVertexInputAttributeDescription
    struct darray vertex_binding_descs;     // array of VkVertexInputBindingDescription

    VkDescriptorPool desc_pool;
    struct darray desc_set_layout_bindings; // array of VkDescriptorSetLayoutBinding
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet *desc_sets;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
};

struct pipeline *ngli_pipeline_vk_create(struct gpu_ctx *gpu_ctx);
int ngli_pipeline_vk_init(struct pipeline *s);
int ngli_pipeline_vk_update_texture(struct pipeline *s, int32_t index, const struct texture *texture);
int ngli_pipeline_vk_update_buffer(struct pipeline *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size);
void ngli_pipeline_vk_draw(struct pipeline *s, int nb_vertices, int nb_instances);
void ngli_pipeline_vk_draw_indexed(struct pipeline *s, int nb_vertices, int nb_instances);
void ngli_pipeline_vk_dispatch(struct pipeline *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);
void ngli_pipeline_vk_freep(struct pipeline **sp);

#endif
