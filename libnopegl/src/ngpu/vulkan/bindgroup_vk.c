/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "bindgroup_vk.h"
#include "buffer_vk.h"
#include "ctx_vk.h"
#include "log.h"
#include "utils/memory.h"
#include "texture_vk.h"
#include "vkcontext.h"
#include "vkutils.h"
#include "ycbcr_sampler_vk.h"

#define INITIAL_MAX_DESC_SETS 32

struct ngpu_bindgroup_layout *ngpu_bindgroup_layout_vk_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_bindgroup_layout_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_bindgroup_layout *)s;
}

static VkShaderStageFlags get_vk_stage_flags(uint32_t stage_flags)
{
    VkShaderStageFlags flags = 0;
    if (NGLI_HAS_ALL_FLAGS(stage_flags, NGPU_PROGRAM_STAGE_VERTEX_BIT))
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (NGLI_HAS_ALL_FLAGS(stage_flags, NGPU_PROGRAM_STAGE_FRAGMENT_BIT))
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (NGLI_HAS_ALL_FLAGS(stage_flags, NGPU_PROGRAM_STAGE_COMPUTE_BIT))
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

static const VkDescriptorType descriptor_type_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_UNIFORM_BUFFER]         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [NGPU_TYPE_STORAGE_BUFFER]         = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [NGPU_TYPE_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [NGPU_TYPE_SAMPLER_2D]             = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGPU_TYPE_SAMPLER_2D_ARRAY]       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGPU_TYPE_SAMPLER_3D]             = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGPU_TYPE_SAMPLER_CUBE]           = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGPU_TYPE_IMAGE_2D]               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [NGPU_TYPE_IMAGE_2D_ARRAY]         = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [NGPU_TYPE_IMAGE_3D]               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [NGPU_TYPE_IMAGE_CUBE]             = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
};

static VkDescriptorType get_vk_descriptor_type(enum ngpu_type type)
{
    const VkDescriptorType descriptor_type = descriptor_type_map[type];
    ngli_assert(descriptor_type);
    return descriptor_type;
}

static void unref_immutable_sampler(void *user_arg, void *data)
{
    struct ycbcr_sampler_vk **ycbcr_samplerp = data;
    ngli_ycbcr_sampler_vk_unrefp(ycbcr_samplerp);
}

static void destroy_desc_pool(void *user_data, void *data)
{
    const struct ngpu_ctx *gpu_ctx = user_data;
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    VkDescriptorPool *desc_pool = data;
    if (!desc_pool)
        return;
    vkResetDescriptorPool(vk->device, *desc_pool, 0);
    vkDestroyDescriptorPool(vk->device, *desc_pool, NULL);
    *desc_pool = VK_NULL_HANDLE;
}

static VkResult allocate_desc_pool(struct ngpu_bindgroup_layout *s, uint32_t factor)
{
    const struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_bindgroup_layout_vk *s_priv = (struct ngpu_bindgroup_layout_vk *)s;

    s_priv->max_desc_sets *= factor;

    for (size_t i = 0; i < s_priv->desc_pool_size_count; i++) {
        s_priv->desc_pool_sizes[i].descriptorCount *= factor;
    }

    const VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = s_priv->desc_pool_size_count,
        .pPoolSizes    = s_priv->desc_pool_sizes,
        .maxSets       = s_priv->max_desc_sets,
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorPool(vk->device, &descriptor_pool_create_info, NULL, &pool);
    if (res != VK_SUCCESS)
        return res;

    if (!ngli_darray_push(&s_priv->desc_pools, &pool)) {
        vkDestroyDescriptorPool(vk->device, pool, NULL);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    const size_t desc_pool_count = ngli_darray_count(&s_priv->desc_pools);
    s_priv->desc_pool_index = desc_pool_count - 1;

    return VK_SUCCESS;
}

static VkResult create_desc_set_layout_bindings(struct ngpu_bindgroup_layout *s)
{
    const struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_bindgroup_layout_vk *s_priv = (struct ngpu_bindgroup_layout_vk *)s;

    ngli_darray_init(&s_priv->desc_set_layout_bindings, sizeof(VkDescriptorSetLayoutBinding), 0);
    ngli_darray_init(&s_priv->immutable_samplers, sizeof(struct ycbcr_sampler_vk *), 0);

    ngli_darray_set_free_func(&s_priv->immutable_samplers, unref_immutable_sampler, NULL);

    VkDescriptorPoolSize desc_pool_size_map[NGPU_TYPE_NB] = {
        [NGPU_TYPE_UNIFORM_BUFFER]         = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        [NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC] = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
        [NGPU_TYPE_STORAGE_BUFFER]         = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        [NGPU_TYPE_STORAGE_BUFFER_DYNAMIC] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
        [NGPU_TYPE_SAMPLER_2D]             = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGPU_TYPE_SAMPLER_2D_ARRAY]       = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGPU_TYPE_SAMPLER_3D]             = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGPU_TYPE_SAMPLER_CUBE]           = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGPU_TYPE_IMAGE_2D]               = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        [NGPU_TYPE_IMAGE_2D_ARRAY]         = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        [NGPU_TYPE_IMAGE_3D]               = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        [NGPU_TYPE_IMAGE_CUBE]             = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
    };

    s_priv->max_desc_sets = INITIAL_MAX_DESC_SETS;

    for (size_t i = 0; i < s->nb_buffers; i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &s->buffers[i];

        const VkDescriptorType type = get_vk_descriptor_type(entry->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = entry->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = get_vk_stage_flags(entry->stage_flags),
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        ngli_assert(desc_pool_size_map[entry->type].type);
        desc_pool_size_map[entry->type].descriptorCount += gpu_ctx->nb_in_flight_frames * s_priv->max_desc_sets;
    }

    for (size_t i = 0; i < s->nb_textures; i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &s->textures[i];

        const VkDescriptorType type = get_vk_descriptor_type(entry->type);
        VkDescriptorSetLayoutBinding binding = {
            .binding            = entry->binding,
            .descriptorType     = type,
            .descriptorCount    = 1,
            .stageFlags         = get_vk_stage_flags(entry->stage_flags),
        };
        if (entry->immutable_sampler) {
            struct ycbcr_sampler_vk *ycbcr_sampler = entry->immutable_sampler;
            binding.pImmutableSamplers =  &ycbcr_sampler->sampler;

            if (!ngli_darray_push(&s_priv->immutable_samplers, &ycbcr_sampler))
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            ngli_ycbcr_sampler_vk_ref(ycbcr_sampler);
        }
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        ngli_assert(desc_pool_size_map[entry->type].type);
        desc_pool_size_map[entry->type].descriptorCount += gpu_ctx->nb_in_flight_frames * s_priv->max_desc_sets;
    }

    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (uint32_t)ngli_darray_count(&s_priv->desc_set_layout_bindings),
        .pBindings    = ngli_darray_data(&s_priv->desc_set_layout_bindings),
    };

    VkResult res = vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, NULL, &s_priv->desc_set_layout);
    if (res != VK_SUCCESS)
        return res;

    s_priv->desc_pool_size_count = 0;
    for (size_t i = 0; i < NGLI_ARRAY_NB(desc_pool_size_map); i++) {
        if (desc_pool_size_map[i].descriptorCount)
            s_priv->desc_pool_sizes[s_priv->desc_pool_size_count++] = desc_pool_size_map[i];
    }

    ngli_darray_init(&s_priv->desc_pools, sizeof(VkDescriptorPool), 0);
    ngli_darray_set_free_func(&s_priv->desc_pools, destroy_desc_pool, (void *)gpu_ctx);

    if (!s_priv->desc_pool_size_count)
        return VK_SUCCESS;

    res = allocate_desc_pool(s, 1);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

static VkResult ngpu_bindgroup_layout_vk_allocate_set(struct ngpu_bindgroup_layout *s, VkDescriptorSet *desc_set)
{
    const struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_bindgroup_layout_vk *s_priv = (struct ngpu_bindgroup_layout_vk *)s;

    *desc_set = VK_NULL_HANDLE;

    for (size_t i = 0; i < ngli_darray_count(&s_priv->desc_pools); i++) {
        const size_t pool_index = (i + s_priv->desc_pool_index) % ngli_darray_count(&s_priv->desc_pools);

        VkDescriptorPool *desc_pool = ngli_darray_get(&s_priv->desc_pools, pool_index);
        const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = *desc_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &s_priv->desc_set_layout,
        };

        VkResult res = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, desc_set);
        if (res == VK_SUCCESS) {
            return VK_SUCCESS;
        } else if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL) {
            /* pass */
        } else {
            return res;
        }
    }

    VkResult res = allocate_desc_pool(s, 2);
    if (res != VK_SUCCESS)
        return res;

    VkDescriptorPool *desc_pool = ngli_darray_get(&s_priv->desc_pools, s_priv->desc_pool_index);
    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = *desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &s_priv->desc_set_layout,
    };

    res = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, desc_set);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

int ngpu_bindgroup_layout_vk_init(struct ngpu_bindgroup_layout *s)
{
    VkResult res = create_desc_set_layout_bindings(s);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    return 0;
}

void ngpu_bindgroup_layout_vk_freep(struct ngpu_bindgroup_layout **sp)
{
    if (!*sp)
        return;

    struct ngpu_bindgroup_layout *s = *sp;
    struct ngpu_bindgroup_layout_vk *s_priv = (struct ngpu_bindgroup_layout_vk *)s;
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    ngli_darray_reset(&s_priv->desc_set_layout_bindings);
    ngli_darray_reset(&s_priv->immutable_samplers);
    ngli_darray_reset(&s_priv->desc_pools);

    vkDestroyDescriptorSetLayout(vk->device, s_priv->desc_set_layout, NULL);

    ngli_freep(sp);
}

struct ngpu_bindgroup *ngpu_bindgroup_vk_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_bindgroup_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_bindgroup *)s;
}

static void unref_texture_binding(void *user_arg, void *data)
{
    struct texture_binding_vk *binding = data;
    NGLI_RC_UNREFP(&binding->texture);
}

static void unref_buffer_binding(void *user_arg, void *data)
{
    struct buffer_binding_vk *binding = data;
    NGLI_RC_UNREFP(&binding->buffer);
}

int ngpu_bindgroup_vk_init(struct ngpu_bindgroup *s, const struct ngpu_bindgroup_params *params)
{
    struct ngpu_bindgroup_vk *s_priv = (struct ngpu_bindgroup_vk *)s;

    if (params->resources.nb_buffers > 0)
        ngli_assert(params->resources.nb_buffers == params->layout->nb_buffers);

    if (params->resources.nb_textures > 0)
        ngli_assert(params->resources.nb_textures == params->layout->nb_textures);

    s->layout = NGLI_RC_REF(params->layout);

    ngli_darray_init(&s_priv->texture_bindings, sizeof(struct texture_binding_vk), 0);
    ngli_darray_init(&s_priv->buffer_bindings, sizeof(struct buffer_binding_vk), 0);

    ngli_darray_set_free_func(&s_priv->texture_bindings, unref_texture_binding, NULL);
    ngli_darray_set_free_func(&s_priv->buffer_bindings, unref_buffer_binding, NULL);

    VkResult res = ngpu_bindgroup_layout_vk_allocate_set(s->layout, &s_priv->desc_set);
    if (res != VK_SUCCESS) {
        return res;
    }

    const struct ngpu_bindgroup_layout *layout = s->layout;
    for (size_t i = 0; i < layout->nb_buffers; i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &layout->buffers[i];
        const struct buffer_binding_vk binding = {.layout_entry = *entry};
        if (!ngli_darray_push(&s_priv->buffer_bindings, &binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (size_t i = 0; i < layout->nb_textures; i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &layout->textures[i];
        const struct texture_binding_vk binding = {.layout_entry = *entry};
        if (!ngli_darray_push(&s_priv->texture_bindings, &binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (size_t i = 0; i < params->resources.nb_buffers; i++) {
        const struct ngpu_buffer_binding *binding = &params->resources.buffers[i];
        int ret = ngpu_bindgroup_update_buffer(s, (int32_t) i, binding);
        if (ret < 0)
            return ret;
    }

    for (size_t i = 0; i < params->resources.nb_textures; i++) {
        const struct ngpu_texture_binding *binding = &params->resources.textures[i];
        int ret = ngpu_bindgroup_update_texture(s, (int32_t) i, binding);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ngpu_bindgroup_vk_update_texture(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_texture_binding *binding)
{
    struct ngpu_bindgroup_vk *s_priv = (struct ngpu_bindgroup_vk *)s;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;

    struct texture_binding_vk *binding_vk = ngli_darray_get(&s_priv->texture_bindings, index);

    NGLI_RC_UNREFP(&binding_vk->texture);

    const struct ngpu_texture *texture = binding->texture;
    if (!texture)
        texture = gpu_ctx_vk->dummy_texture;

    binding_vk->texture = NGLI_RC_REF(texture);
    binding_vk->update_desc = 1;

    return 0;
}

int ngpu_bindgroup_vk_update_buffer(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_buffer_binding *binding)
{
    struct ngpu_bindgroup_vk *s_priv = (struct ngpu_bindgroup_vk *)s;

    struct buffer_binding_vk *binding_vk = ngli_darray_get(&s_priv->buffer_bindings, index);

    NGLI_RC_UNREFP(&binding_vk->buffer);

    const struct ngpu_buffer *buffer = binding->buffer;
    if (buffer)
        buffer = NGLI_RC_REF(binding->buffer);

    binding_vk->buffer = buffer;
    binding_vk->offset = binding->offset;
    binding_vk->size   = binding->size;
    binding_vk->update_desc = 1;

    return 0;
}

int ngpu_bindgroup_vk_update_descriptor_set(struct ngpu_bindgroup *s)
{
    struct ngpu_bindgroup_vk *s_priv = (struct ngpu_bindgroup_vk *)s;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    struct texture_binding_vk *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        struct texture_binding_vk *binding = &texture_bindings[i];
        if (binding->update_desc) {
            const struct ngpu_texture_vk *texture_vk = (struct ngpu_texture_vk *)binding->texture;
            const VkDescriptorImageInfo image_info = {
                .imageLayout = texture_vk->default_image_layout,
                .imageView   = texture_vk->image_view,
                .sampler     = texture_vk->sampler,
            };
            const struct ngpu_bindgroup_layout_entry *desc = &binding->layout_entry;
            const VkWriteDescriptorSet write_descriptor_set = {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet           = s_priv->desc_set,
                .dstBinding       = desc->binding,
                .dstArrayElement  = 0,
                .descriptorType   = get_vk_descriptor_type(desc->type),
                .descriptorCount  = 1,
                .pImageInfo       = &image_info,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
            binding->update_desc = 0;
        }
    }

    struct buffer_binding_vk *buffer_bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        struct buffer_binding_vk *binding = &buffer_bindings[i];
        if (binding->update_desc) {
            const struct ngpu_bindgroup_layout_entry *desc = &binding->layout_entry;
            const struct ngpu_buffer_vk *buffer_vk = (struct ngpu_buffer_vk *)(binding->buffer);
            const VkDescriptorBufferInfo descriptor_buffer_info = {
                .buffer = buffer_vk->buffer,
                .offset = binding->offset,
                .range  = binding->size,
            };
            const VkWriteDescriptorSet write_descriptor_set = {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet           = s_priv->desc_set,
                .dstBinding       = desc->binding,
                .dstArrayElement  = 0,
                .descriptorType   = get_vk_descriptor_type(desc->type),
                .descriptorCount  = 1,
                .pBufferInfo      = &descriptor_buffer_info,
                .pImageInfo       = NULL,
                .pTexelBufferView = NULL,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
            binding->update_desc = 0;
        }
    }

    return 0;
}

void ngpu_bindgroup_vk_freep(struct ngpu_bindgroup **sp)
{
    if (!*sp)
        return;

    struct ngpu_bindgroup *s = *sp;
    struct ngpu_bindgroup_vk *s_priv = (struct ngpu_bindgroup_vk *)s;

    NGLI_RC_UNREFP(&s->layout);
    ngli_darray_reset(&s_priv->texture_bindings);
    ngli_darray_reset(&s_priv->buffer_bindings);

    ngli_freep(sp);
}
