/*
 * Copyright 2023 Nope Forge
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

#include <string.h>
#include <stdint.h>

#include "buffer_vk.h"
#include "ctx_vk.h"
#include "log.h"
#include "utils/memory.h"
#include "vkcontext.h"
#include "vkutils.h"

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

static VkBufferUsageFlags get_vk_buffer_usage_flags(uint32_t usage)
{
    return (usage & NGPU_BUFFER_USAGE_TRANSFER_SRC_BIT   ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT   : 0) |
           (usage & NGPU_BUFFER_USAGE_TRANSFER_DST_BIT   ? VK_BUFFER_USAGE_TRANSFER_DST_BIT   : 0) |
           (usage & NGPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0) |
           (usage & NGPU_BUFFER_USAGE_STORAGE_BUFFER_BIT ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
           (usage & NGPU_BUFFER_USAGE_INDEX_BUFFER_BIT   ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT   : 0) |
           (usage & NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT  ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  : 0);
}

struct ngpu_buffer *ngpu_buffer_vk_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_buffer_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_buffer *)s;
}

static void unref_cmd_buffer(void *user_arg, void *data)
{
    struct cmd_buffer_vk **cmd_bufferp = data;
    ngli_cmd_buffer_vk_freep(cmd_bufferp);
}

static VkResult buffer_vk_init(struct ngpu_buffer *s)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    ngli_darray_init(&s_priv->cmd_buffers, sizeof(struct cmd_buffer_vk *), 0);
    ngli_darray_set_free_func(&s_priv->cmd_buffers, unref_cmd_buffer, NULL);

    VkMemoryPropertyFlags mem_props;
    if (s->usage & NGPU_BUFFER_USAGE_MAP_READ) {
        mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    } else if (s->usage & NGPU_BUFFER_USAGE_MAP_WRITE ||
               s->usage & NGPU_BUFFER_USAGE_DYNAMIC_BIT) {
        mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    const VkBufferUsageFlags flags = get_vk_buffer_usage_flags(s->usage);
    return create_vk_buffer(vk, s->size, flags, mem_props, &s_priv->buffer, &s_priv->memory);
}

int ngpu_buffer_vk_init(struct ngpu_buffer *s)
{
    VkResult res = buffer_vk_init(s);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize buffer: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

int ngpu_buffer_vk_wait(struct ngpu_buffer *s)
{
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    struct cmd_buffer_vk **cmd_buffers = ngli_darray_data(&s_priv->cmd_buffers);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->cmd_buffers); i++) {
        struct cmd_buffer_vk *cmd_buffer = cmd_buffers[i];
        ngli_cmd_buffer_vk_wait(cmd_buffer);
    }
    ngli_darray_clear(&s_priv->cmd_buffers);

    return 0;
}

static VkResult buffer_vk_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    if (s->usage & NGPU_BUFFER_USAGE_MAP_READ ||
        s->usage & NGPU_BUFFER_USAGE_MAP_WRITE ||
        s->usage & NGPU_BUFFER_USAGE_DYNAMIC_BIT) {
        void *mapped_data;
        VkResult res = vkMapMemory(vk->device, s_priv->memory, offset, size, 0, &mapped_data);
        if (res != VK_SUCCESS)
            return res;
        memcpy(mapped_data, data, size);
        vkUnmapMemory(vk->device, s_priv->memory);
        return VK_SUCCESS;
    }

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

    struct cmd_buffer_vk *cmd_buffer_vk;
    res = ngli_cmd_buffer_vk_begin_transient(s->gpu_ctx, 0, &cmd_buffer_vk);
    if (res != VK_SUCCESS)
        return res;

    const VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = offset,
        .size      = size,
    };
    vkCmdCopyBuffer(cmd_buffer_vk->cmd_buf, s_priv->staging_buffer, s_priv->buffer, 1, &region);

    res = ngli_cmd_buffer_vk_execute_transient(&cmd_buffer_vk);
    if (res != VK_SUCCESS)
        return res;

    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    s_priv->staging_buffer = VK_NULL_HANDLE;
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);
    s_priv->staging_memory = VK_NULL_HANDLE;

    return VK_SUCCESS;
}

int ngpu_buffer_vk_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size)
{
    VkResult res = buffer_vk_upload(s, data, offset, size);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to upload buffer: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static VkResult buffer_vk_map(struct ngpu_buffer *s, size_t offset, size_t size, void **data)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    return vkMapMemory(vk->device, s_priv->memory, offset, size, 0, data);
}

int ngpu_buffer_vk_map(struct ngpu_buffer *s, size_t offset, size_t size, void **data)
{
    VkResult res = buffer_vk_map(s, offset, size, data);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to map buffer: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

void ngpu_buffer_vk_unmap(struct ngpu_buffer *s)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    vkUnmapMemory(vk->device, s_priv->memory);
}

static size_t buffer_vk_find_cmd_buffer(struct ngpu_buffer *s, struct cmd_buffer_vk *cmd_buffer)
{
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    struct cmd_buffer_vk **cmd_buffers = ngli_darray_data(&s_priv->cmd_buffers);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->cmd_buffers); i++) {
        if (cmd_buffers[i] == cmd_buffer)
            return i;
    }

    return SIZE_MAX;
}

int ngpu_buffer_vk_ref_cmd_buffer(struct ngpu_buffer *s, struct cmd_buffer_vk *cmd_buffer)
{
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    size_t index = buffer_vk_find_cmd_buffer(s, cmd_buffer);
    if (index != SIZE_MAX)
        return 0;

    if (!ngli_darray_push(&s_priv->cmd_buffers, cmd_buffer))
        return NGL_ERROR_MEMORY;

    NGLI_RC_REF(cmd_buffer);

    return 0;
}

int ngpu_buffer_vk_unref_cmd_buffer(struct ngpu_buffer *s, struct cmd_buffer_vk *cmd_buffer)
{
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    size_t index = buffer_vk_find_cmd_buffer(s, cmd_buffer);
    if (index == SIZE_MAX)
        return 0;

    ngli_darray_remove(&s_priv->cmd_buffers, index);
    NGLI_RC_UNREFP(&cmd_buffer);

    return 0;
}

void ngpu_buffer_vk_freep(struct ngpu_buffer **sp)
{
    if (!*sp)
        return;

    struct ngpu_buffer *s = *sp;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_buffer_vk *s_priv = (struct ngpu_buffer_vk *)s;

    ngli_darray_reset(&s_priv->cmd_buffers);

    vkDestroyBuffer(vk->device, s_priv->buffer, NULL);
    vkFreeMemory(vk->device, s_priv->memory, NULL);
    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);
    ngli_freep(sp);
}
