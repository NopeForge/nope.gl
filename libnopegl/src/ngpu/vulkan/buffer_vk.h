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

#ifndef NGPU_BUFFER_VK_H
#define NGPU_BUFFER_VK_H

#include <vulkan/vulkan.h>

#include "cmd_buffer_vk.h"
#include "ngpu/buffer.h"
#include "utils/darray.h"

struct ngpu_buffer_vk {
    struct ngpu_buffer parent;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    struct darray cmd_buffers;
};

struct ngpu_buffer *ngpu_buffer_vk_create(struct ngpu_ctx *gpu_ctx);
int ngpu_buffer_vk_init(struct ngpu_buffer *s);
int ngpu_buffer_vk_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size);
int ngpu_buffer_vk_map(struct ngpu_buffer *s, size_t offset, size_t size, void **data);
void ngpu_buffer_vk_unmap(struct ngpu_buffer *s);
int ngpu_buffer_vk_ref_cmd_buffer(struct ngpu_buffer *s, struct cmd_buffer_vk *cmd_buffer);
int ngpu_buffer_vk_unref_cmd_buffer(struct ngpu_buffer *s, struct cmd_buffer_vk *cmd_buffer);
void ngpu_buffer_vk_freep(struct ngpu_buffer **sp);

#endif
