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

#include "darray.h"
#include "gpu_ctx.h"
#include "gpu_limits.h"
#include "memory.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "type.h"

struct pipeline_compat {
    struct gpu_ctx *gpu_ctx;
    struct pipeline *pipeline;
    const struct buffer **vertex_buffers;
    size_t nb_vertex_buffers;
    const struct pgcraft_compat_info *compat_info;
    struct buffer *ubuffers[NGLI_PROGRAM_SHADER_NB];
    uint8_t *mapped_datas[NGLI_PROGRAM_SHADER_NB];
};

static int map_buffer(struct pipeline_compat *s, int stage)
{
    if (s->mapped_datas[stage])
        return 0;

    struct buffer *buffer = s->ubuffers[stage];
    return ngli_buffer_map(buffer, buffer->size, 0, (void **)&s->mapped_datas[stage]);
}

static void unmap_buffers(struct pipeline_compat *s)
{
    for (size_t i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        if (s->mapped_datas[i]) {
            ngli_buffer_unmap(s->ubuffers[i]);
            s->mapped_datas[i] = NULL;
        }
    }
}

struct pipeline_compat *ngli_pipeline_compat_create(struct gpu_ctx *gpu_ctx)
{
    struct pipeline_compat *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->gpu_ctx = gpu_ctx;
    return s;
}

static int init_blocks_buffers(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    for (size_t i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const size_t block_size = ngli_block_get_size(&s->compat_info->ublocks[i], 0);
        if (!block_size)
            continue;

        struct buffer *buffer = ngli_buffer_create(gpu_ctx);
        if (!buffer)
            return NGL_ERROR_MEMORY;
        s->ubuffers[i] = buffer;

        int ret = ngli_buffer_init(buffer,
                                   block_size,
                                   NGLI_BUFFER_USAGE_DYNAMIC_BIT |
                                   NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   NGLI_BUFFER_USAGE_MAP_WRITE);
        if (ret < 0)
            return ret;

        if (gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT) {
            ret = ngli_buffer_map(buffer, buffer->size, 0, (void **)&s->mapped_datas[i]);
            if (ret < 0)
                return ret;
        }

        ngli_pipeline_update_buffer(s->pipeline, s->compat_info->uindices[i], buffer, 0, buffer->size);
    }

    return 0;
}

int ngli_pipeline_compat_init(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    s->pipeline = ngli_pipeline_create(gpu_ctx);
    if (!s->pipeline)
        return NGL_ERROR_MEMORY;

    const struct pipeline_params *pipeline_params = params->params;
    const struct pipeline_resources *pipeline_resources = params->resources;

    int ret;
    if ((ret = ngli_pipeline_init(s->pipeline, pipeline_params)) < 0 ||
        (ret = ngli_pipeline_set_resources(s->pipeline, pipeline_resources)) < 0)
        return ret;

    const size_t nb_attributes = pipeline_resources->nb_attributes;
    if (nb_attributes) {
        s->vertex_buffers = ngli_calloc(nb_attributes, sizeof(struct buffer *));
        for (size_t i = 0; i < nb_attributes; i++)
            s->vertex_buffers[i] = pipeline_resources->attributes[i];
        s->nb_vertex_buffers = nb_attributes;
    }

    s->compat_info = params->compat_info;
    ret = init_blocks_buffers(s, params);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_pipeline_compat_update_vertex_buffer(struct pipeline_compat *s, int32_t index, const struct buffer *buffer)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_vertex_buffers);
    s->vertex_buffers[index] = buffer;
    return 0;
}

int ngli_pipeline_compat_update_uniform_count(struct pipeline_compat *s, int32_t index, const void *value, size_t count)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    const int32_t stage = index >> 16;
    const int32_t field_index = index & 0xffff;
    const struct block *block = &s->compat_info->ublocks[stage];
    const struct block_field *fields = ngli_darray_data(&block->fields);
    const struct block_field *field = &fields[field_index];
    if (value) {
        if (!(gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT)) {
            int ret = map_buffer(s, stage);
            if (ret < 0)
                return ret;
        }
        uint8_t *dst = s->mapped_datas[stage] + field->offset;
        ngli_block_field_copy_count(field, dst, value, count);
    }

    return 0;
}

int ngli_pipeline_compat_update_uniform(struct pipeline_compat *s, int32_t index, const void *value)
{
    return ngli_pipeline_compat_update_uniform_count(s, index, value, 0);
}

int ngli_pipeline_compat_update_texture(struct pipeline_compat *s, int32_t index, const struct texture *texture)
{
    return ngli_pipeline_update_texture(s->pipeline, index, texture);
}

int ngli_pipeline_compat_update_dynamic_offsets(struct pipeline_compat *s, const uint32_t *offsets, size_t nb_offsets)
{
    return ngli_pipeline_update_dynamic_offsets(s->pipeline, offsets, nb_offsets);
}

void ngli_pipeline_compat_update_texture_info(struct pipeline_compat *s, const struct pgcraft_texture_info *info)
{
    const struct pgcraft_texture_info_field *fields = info->fields;
    const struct image *image = info->image;

    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_TIMESTAMP].index, &image->ts);

    if (image->params.layout) {
        const float dimensions[] = {(float)image->params.width, (float)image->params.height, (float)image->params.depth};
        ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_DIMENSIONS].index, dimensions);
    }

    const struct texture *textures[NGLI_INFO_FIELD_NB] = {0};
    switch (image->params.layout) {
    case NGLI_IMAGE_LAYOUT_DEFAULT:
        textures[NGLI_INFO_FIELD_SAMPLER_0] = image->planes[0];
        break;
    case NGLI_IMAGE_LAYOUT_NV12:
        textures[NGLI_INFO_FIELD_SAMPLER_0] = image->planes[0];
        textures[NGLI_INFO_FIELD_SAMPLER_1] = image->planes[1];
        break;
    case NGLI_IMAGE_LAYOUT_NV12_RECTANGLE:
        textures[NGLI_INFO_FIELD_SAMPLER_RECT_0] = image->planes[0];
        textures[NGLI_INFO_FIELD_SAMPLER_RECT_1] = image->planes[1];
        break;
    case NGLI_IMAGE_LAYOUT_MEDIACODEC:
        textures[NGLI_INFO_FIELD_SAMPLER_OES] = image->planes[0];
        break;
    case NGLI_IMAGE_LAYOUT_YUV:
        textures[NGLI_INFO_FIELD_SAMPLER_0] = image->planes[0];
        textures[NGLI_INFO_FIELD_SAMPLER_1] = image->planes[1];
        textures[NGLI_INFO_FIELD_SAMPLER_2] = image->planes[2];
        break;
    case NGLI_IMAGE_LAYOUT_RECTANGLE:
        textures[NGLI_INFO_FIELD_SAMPLER_RECT_0] = image->planes[0];
        break;
    default:
        break;
    }

    static const int samplers[] = {
        NGLI_INFO_FIELD_SAMPLER_0,
        NGLI_INFO_FIELD_SAMPLER_1,
        NGLI_INFO_FIELD_SAMPLER_2,
        NGLI_INFO_FIELD_SAMPLER_OES,
        NGLI_INFO_FIELD_SAMPLER_RECT_0,
        NGLI_INFO_FIELD_SAMPLER_RECT_1,
    };

    int ret = 1;
    for (size_t i = 0; i < NGLI_ARRAY_NB(samplers); i++) {
        const int sampler = samplers[i];
        const int32_t index = fields[sampler].index;
        const struct texture *texture = textures[sampler];
        ret &= ngli_pipeline_compat_update_texture(s, index, texture);
    };

    const int layout = ret < 0 ? NGLI_IMAGE_LAYOUT_NONE : image->params.layout;
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &layout);
}

int ngli_pipeline_compat_update_buffer(struct pipeline_compat *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size)
{
    return ngli_pipeline_update_buffer(s->pipeline, index, buffer, offset, size);
}

void ngli_pipeline_compat_draw(struct pipeline_compat *s, int nb_vertices, int nb_instances)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct pipeline *pipeline = s->pipeline;

    if (!(gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT))
       unmap_buffers(s);

    ngli_gpu_ctx_set_pipeline(gpu_ctx, pipeline);
    for (size_t i = 0; i < s->nb_vertex_buffers; i++)
        ngli_gpu_ctx_set_vertex_buffer(gpu_ctx, (uint32_t)i, s->vertex_buffers[i]);
    ngli_gpu_ctx_draw(gpu_ctx, nb_vertices, nb_instances);
}

void ngli_pipeline_compat_draw_indexed(struct pipeline_compat *s, const struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct pipeline *pipeline = s->pipeline;

    if (!(gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT))
       unmap_buffers(s);

    ngli_gpu_ctx_set_pipeline(gpu_ctx, pipeline);
    for (size_t i = 0; i < s->nb_vertex_buffers; i++)
        ngli_gpu_ctx_set_vertex_buffer(gpu_ctx, (uint32_t)i, s->vertex_buffers[i]);
    ngli_gpu_ctx_set_index_buffer(gpu_ctx, indices, indices_format);
    ngli_gpu_ctx_draw_indexed(gpu_ctx, nb_indices, nb_instances);
}

void ngli_pipeline_compat_dispatch(struct pipeline_compat *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct pipeline *pipeline = s->pipeline;

    if (!(gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT))
       unmap_buffers(s);

    ngli_gpu_ctx_set_pipeline(gpu_ctx, pipeline);
    ngli_gpu_ctx_dispatch(gpu_ctx, nb_group_x, nb_group_y, nb_group_z);
}

void ngli_pipeline_compat_freep(struct pipeline_compat **sp)
{
    struct pipeline_compat *s = *sp;
    if (!s)
        return;
    ngli_pipeline_freep(&s->pipeline);
    ngli_freep(&s->vertex_buffers);
    if (s->compat_info) {
        for (size_t i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
            if (s->ubuffers[i]) {
                if (s->mapped_datas[i])
                    ngli_buffer_unmap(s->ubuffers[i]);
                ngli_buffer_freep(&s->ubuffers[i]);
            }
        }
    }
    ngli_freep(sp);
}
