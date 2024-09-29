/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef GPU_BUFFER_VK_H
#define GPU_BUFFER_VK_H

#include <vulkan/vulkan.h>

#include "gpu_buffer.h"

struct gpu_buffer_vk {
    struct gpu_buffer parent;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
};

struct gpu_buffer *ngli_gpu_buffer_vk_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_buffer_vk_init(struct gpu_buffer *s);
int ngli_gpu_buffer_vk_upload(struct gpu_buffer *s, const void *data, size_t offset, size_t size);
int ngli_gpu_buffer_vk_map(struct gpu_buffer *s, size_t offset, size_t size, void **data);
void ngli_gpu_buffer_vk_unmap(struct gpu_buffer *s);
void ngli_gpu_buffer_vk_freep(struct gpu_buffer **sp);

#endif
