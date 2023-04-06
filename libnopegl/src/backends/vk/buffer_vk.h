/*
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

#ifndef BUFFER_VK_H
#define BUFFER_VK_H

#include <vulkan/vulkan.h>

#include "buffer.h"

struct buffer_vk {
    struct buffer parent;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
};

struct buffer *ngli_buffer_vk_create(struct gpu_ctx *gpu_ctx);
VkResult ngli_buffer_vk_init(struct buffer *s, int size, int usage);
VkResult ngli_buffer_vk_upload(struct buffer *s, const void *data, int size, int offset);
VkResult ngli_buffer_vk_map(struct buffer *s, int size, int offset, void **data);
void ngli_buffer_vk_unmap(struct buffer *s);
void ngli_buffer_vk_freep(struct buffer **sp);

#endif
