/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "gpu_ctx.h"
#include "log.h"
#include "memory.h"
#include "pipeline.h"
#include "buffer.h"
#include "type.h"
#include "utils.h"

int ngli_pipeline_graphics_copy(struct pipeline_graphics *dst, const struct pipeline_graphics *src)
{
    dst->topology = src->topology;
    dst->state    = src->state;
    dst->rt_layout  = src->rt_layout;

    const struct vertex_buffer_layout *buffers = src->vertex_state.buffers;
    const size_t nb_buffers = src->vertex_state.nb_buffers;
    if (nb_buffers) {
        dst->vertex_state.buffers = ngli_memdup(buffers, nb_buffers * sizeof(*buffers));
        if (!dst->vertex_state.buffers)
            return NGL_ERROR_MEMORY;
    }
    dst->vertex_state.nb_buffers = nb_buffers;

    return 0;
}

void ngli_pipeline_graphics_reset(struct pipeline_graphics *graphics)
{
    ngli_freep(&graphics->vertex_state.buffers);
    memset(graphics, 0, sizeof(*graphics));
}

int ngli_pipeline_layout_copy(struct pipeline_layout *dst, const struct pipeline_layout *src)
{
    dst->texture_descs = ngli_memdup(src->texture_descs, src->nb_texture_descs * sizeof(*src->texture_descs));
    if (!dst->texture_descs)
        return NGL_ERROR_MEMORY;
    dst->nb_texture_descs = src->nb_texture_descs;

    dst->buffer_descs = ngli_memdup(src->buffer_descs, src->nb_buffer_descs * sizeof(*src->buffer_descs));
    if (!dst->buffer_descs)
        return NGL_ERROR_MEMORY;
    dst->nb_buffer_descs = src->nb_buffer_descs;

    return 0;
}

void ngli_pipeline_layout_reset(struct pipeline_layout *layout)
{
    ngli_freep(&layout->texture_descs);
    ngli_freep(&layout->buffer_descs);
    memset(layout, 0, sizeof(*layout));
}

struct pipeline *ngli_pipeline_create(struct gpu_ctx *gpu_ctx)
{
    return gpu_ctx->cls->pipeline_create(gpu_ctx);
}

int ngli_pipeline_init(struct pipeline *s, const struct pipeline_params *params)
{
    s->type     = params->type;

    int ret = ngli_pipeline_graphics_copy(&s->graphics, &params->graphics);
    if (ret < 0)
        return ret;

    s->program  = params->program;

    ret = ngli_pipeline_layout_copy(&s->layout, &params->layout);
    if (ret < 0)
        return ret;

    size_t nb_uniform_buffers_dynamic = 0;
    size_t nb_storage_buffers_dynamic = 0;
    for (size_t i = 0; i < s->layout.nb_buffer_descs; i++) {
        const struct pipeline_resource_desc *desc = &s->layout.buffer_descs[i];
        if (desc->type == NGLI_TYPE_UNIFORM_BUFFER_DYNAMIC)
            nb_uniform_buffers_dynamic++;
        else if (desc->type == NGLI_TYPE_STORAGE_BUFFER_DYNAMIC)
            nb_storage_buffers_dynamic++;
    }
    ngli_assert(nb_uniform_buffers_dynamic <= NGLI_MAX_UNIFORM_BUFFERS_DYNAMIC);
    ngli_assert(nb_storage_buffers_dynamic <= NGLI_MAX_STORAGE_BUFFERS_DYNAMIC);
    s->nb_dynamic_offsets = nb_uniform_buffers_dynamic + nb_storage_buffers_dynamic;

    return s->gpu_ctx->cls->pipeline_init(s);
}

int ngli_pipeline_set_resources(struct pipeline *s, const struct pipeline_resources *resources)
{
    const struct pipeline_layout *layout = &s->layout;
    ngli_assert(layout->nb_buffer_descs == resources->nb_buffers);
    for (size_t i = 0; i < resources->nb_buffers; i++) {
        int ret = ngli_pipeline_update_buffer(s, (int32_t)i, resources->buffers[i], 0, 0);
        if (ret < 0)
            return ret;
    }

    ngli_assert(layout->nb_texture_descs == resources->nb_textures);
    for (size_t i = 0; i < resources->nb_textures; i++) {
        int ret = ngli_pipeline_update_texture(s, (int32_t)i, resources->textures[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ngli_pipeline_update_texture(struct pipeline *s, int32_t index, const struct texture *texture)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    return s->gpu_ctx->cls->pipeline_update_texture(s, index, texture);
}

int ngli_pipeline_update_buffer(struct pipeline *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    if (buffer) {
        if (!size)
            size = buffer->size;
        const struct pipeline_resource_desc *desc = &s->layout.buffer_descs[index];
        const struct gpu_limits *limits = &s->gpu_ctx->limits;
        if (desc->type == NGLI_TYPE_UNIFORM_BUFFER ||
            desc->type == NGLI_TYPE_UNIFORM_BUFFER_DYNAMIC) {
            ngli_assert(buffer->usage & NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            if (size > limits->max_uniform_block_size) {
                LOG(ERROR, "buffer (binding=%d) size (%zu) exceeds max uniform block size (%d)",
                    desc->binding, buffer->size, limits->max_uniform_block_size);
                return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
            }
        } else if (desc->type == NGLI_TYPE_STORAGE_BUFFER ||
                   desc->type == NGLI_TYPE_STORAGE_BUFFER_DYNAMIC) {
            ngli_assert(buffer->usage & NGLI_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            if (size > limits->max_storage_block_size) {
                LOG(ERROR, "buffer (binding=%d) size (%zu) exceeds max storage block size (%d)",
                    desc->binding, buffer->size, limits->max_storage_block_size);
                return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
            }
        }
    }

    return s->gpu_ctx->cls->pipeline_update_buffer(s, index, buffer, offset, size);
}

int ngli_pipeline_update_dynamic_offsets(struct pipeline *s, const uint32_t *offsets, size_t nb_offsets)
{
    ngli_assert(nb_offsets == s->nb_dynamic_offsets);
    memcpy(s->dynamic_offsets, offsets, nb_offsets * sizeof(*s->dynamic_offsets));
    return 0;
}

void ngli_pipeline_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    ngli_pipeline_graphics_reset(&s->graphics);
    ngli_pipeline_layout_reset(&s->layout);

    (*sp)->gpu_ctx->cls->pipeline_freep(sp);
}
