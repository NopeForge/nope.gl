/*
 * Copyright 2022 GoPro Inc.
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

#ifndef COMMAND_VK_H
#define COMMAND_VK_H

#include <vulkan/vulkan.h>

#include "darray.h"

struct cmd_vk {
    struct gpu_ctx *gpu_ctx;
    int type;
    VkCommandPool pool;
    VkCommandBuffer cmd_buf;
    VkFence fence;
    struct darray wait_sems;
    struct darray wait_stages;
    struct darray signal_sems;
};

struct cmd_vk *ngli_cmd_vk_create(struct gpu_ctx *gpu_ctx);
void ngli_cmd_vk_freep(struct cmd_vk **sp);
VkResult ngli_cmd_vk_init(struct cmd_vk *s, int type);
VkResult ngli_cmd_vk_add_wait_sem(struct cmd_vk *s, VkSemaphore *sem, VkPipelineStageFlags stage);
VkResult ngli_cmd_vk_add_signal_sem(struct cmd_vk *s, VkSemaphore *sem);
VkResult ngli_cmd_vk_begin(struct cmd_vk *s);
VkResult ngli_cmd_vk_submit(struct cmd_vk *s);
VkResult ngli_cmd_vk_wait(struct cmd_vk *s);

VkResult ngli_cmd_vk_begin_transient(struct gpu_ctx *gpu_ctx, int type, struct cmd_vk **sp);
VkResult ngli_cmd_vk_execute_transient(struct cmd_vk **sp);

#endif
