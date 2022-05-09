/*
 * Copyright 2019 GoPro Inc.
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

#include <string.h>
#include <stdint.h>

#include "buffer_vk.h"
#include "gpu_ctx_vk.h"
#include "internal.h"
#include "memory.h"
#include "vkcontext.h"

static VkResult create_vk_buffer(struct vkcontext *vk,
                                 VkDeviceSize size,
                                 VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags mem_props,
                                 VkBuffer *bufferp,
                                 VkDeviceMemory *memoryp)
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    const VkBufferCreateInfo buffer_create_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkResult res = vkCreateBuffer(vk->device, &buffer_create_info, NULL, &buffer);
    if (res != VK_SUCCESS)
        goto fail;

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(vk->device, buffer, &mem_reqs);

    int mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_props);
    if (mem_type_index < 0) {
        /* Cached memory might not be supported, falling back on uncached memory */
        mem_props &= ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        mem_type_index = ngli_vkcontext_find_memory_type(vk, mem_reqs.memoryTypeBits, mem_props);
        if (mem_type_index < 0) {
            res = VK_ERROR_FORMAT_NOT_SUPPORTED;
            goto fail;
        }
    }

    const VkMemoryAllocateInfo memory_allocate_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_reqs.size,
        .memoryTypeIndex = mem_type_index,
    };
    res = vkAllocateMemory(vk->device, &memory_allocate_info, NULL, &memory);
    if (res != VK_SUCCESS)
        goto fail;

    res = vkBindBufferMemory(vk->device, buffer, memory, 0);
    if (res != VK_SUCCESS)
        goto fail;

    *bufferp = buffer;
    *memoryp = memory;

    return VK_SUCCESS;

fail:
    vkDestroyBuffer(vk->device, buffer, NULL);
    vkFreeMemory(vk->device, memory, NULL);
    return res;
}

static VkBufferUsageFlags get_vk_buffer_usage_flags(int usage)
{
    return (usage & NGLI_BUFFER_USAGE_TRANSFER_SRC_BIT   ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT   : 0) |
           (usage & NGLI_BUFFER_USAGE_TRANSFER_DST_BIT   ? VK_BUFFER_USAGE_TRANSFER_DST_BIT   : 0) |
           (usage & NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0) |
           (usage & NGLI_BUFFER_USAGE_STORAGE_BUFFER_BIT ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
           (usage & NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT   ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT   : 0) |
           (usage & NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT  ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  : 0);
}

struct buffer *ngli_buffer_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct buffer_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct buffer *)s;
}

VkResult ngli_buffer_vk_init(struct buffer *s, int size, int usage)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    s->size = size;
    s->usage = usage;

    VkMemoryPropertyFlags mem_props;
    if (usage & NGLI_BUFFER_USAGE_MAP_READ) {
        mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    } else if (usage & NGLI_BUFFER_USAGE_MAP_WRITE ||
               usage & NGLI_BUFFER_USAGE_DYNAMIC_BIT) {
        mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    const VkBufferUsageFlags flags = get_vk_buffer_usage_flags(usage);
    return create_vk_buffer(vk, size, flags, mem_props, &s_priv->buffer, &s_priv->memory);
}

VkResult ngli_buffer_vk_upload(struct buffer *s, const void *data, int size, int offset)
{
    if (s->usage & NGLI_BUFFER_USAGE_MAP_READ ||
        s->usage & NGLI_BUFFER_USAGE_MAP_WRITE ||
        s->usage & NGLI_BUFFER_USAGE_DYNAMIC_BIT) {
        void *mapped_data;
        VkResult res = ngli_buffer_vk_map(s, size, offset, &mapped_data);
        if (res != VK_SUCCESS)
            return res;
        memcpy(mapped_data, data, size);
        ngli_buffer_vk_unmap(s);
        return VK_SUCCESS;
    }

    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkResult res = create_vk_buffer(vk, s->size, usage, mem_props,
                                    &s_priv->staging_buffer, &s_priv->staging_memory);
    if (res != VK_SUCCESS)
        return res;

    uint8_t *mapped_data;
    res = vkMapMemory(vk->device, s_priv->staging_memory, 0, s->size, 0, (void *)&mapped_data);
    if (res != VK_SUCCESS)
        return res;
    memcpy(mapped_data + offset, data, size);
    vkUnmapMemory(vk->device, s_priv->staging_memory);

    struct cmd_vk *cmd_vk;
    res = ngli_cmd_vk_begin_transient(s->gpu_ctx, 0, &cmd_vk);
    if (res != VK_SUCCESS)
        return res;

    const VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = offset,
        .size      = size,
    };
    vkCmdCopyBuffer(cmd_vk->cmd_buf, s_priv->staging_buffer, s_priv->buffer, 1, &region);

    res = ngli_cmd_vk_execute_transient(&cmd_vk);
    if (res != VK_SUCCESS)
        return res;

    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    s_priv->staging_buffer = VK_NULL_HANDLE;
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);
    s_priv->staging_memory = VK_NULL_HANDLE;

    return VK_SUCCESS;
}

VkResult ngli_buffer_vk_map(struct buffer *s, int size, int offset, void **data)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    return vkMapMemory(vk->device, s_priv->memory, offset, size, 0, data);
}

void ngli_buffer_vk_unmap(struct buffer *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    vkUnmapMemory(vk->device, s_priv->memory);
}

void ngli_buffer_vk_freep(struct buffer **sp)
{
    if (!*sp)
        return;

    struct buffer *s = *sp;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    vkDestroyBuffer(vk->device, s_priv->buffer, NULL);
    vkFreeMemory(vk->device, s_priv->memory, NULL);
    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);
    ngli_freep(sp);
}
