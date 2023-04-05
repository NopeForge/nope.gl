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
#include "memory.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "type.h"

struct pipeline_compat {
    struct gpu_ctx *gpu_ctx;
    struct pipeline *pipeline;
    const struct pgcraft_compat_info *compat_info;
    struct buffer *ubuffers[NGLI_PROGRAM_SHADER_NB];
    uint8_t *mapped_datas[NGLI_PROGRAM_SHADER_NB];
};

struct pipeline_compat *ngli_pipeline_compat_create(struct gpu_ctx *gpu_ctx)
{
    struct pipeline_compat *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->gpu_ctx = gpu_ctx;
    return s;
}

static int get_pipeline_ubo_index(const struct pipeline_params *params, int binding, int stage)
{
    const struct pipeline_layout *layout = &params->layout;
    for (int i = 0; i < layout->nb_buffers; i++) {
        if (layout->buffers_desc[i].type == NGLI_TYPE_UNIFORM_BUFFER &&
            layout->buffers_desc[i].stage == stage &&
            layout->buffers_desc[i].binding == binding) {
            return i;
        }
    }
    return -1;
}

static int init_blocks_buffers(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const struct block *block = &s->compat_info->ublocks[i];
        if (!block->size)
            continue;

        struct buffer *buffer = ngli_buffer_create(gpu_ctx);
        if (!buffer)
            return NGL_ERROR_MEMORY;
        s->ubuffers[i] = buffer;

        int ret = ngli_buffer_init(buffer,
                                   block->size,
                                   NGLI_BUFFER_USAGE_DYNAMIC_BIT |
                                   NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   NGLI_BUFFER_USAGE_MAP_WRITE);
        if (ret < 0)
            return ret;

        ret = ngli_buffer_map(buffer, buffer->size, 0, (void **)&s->mapped_datas[i]);
        if (ret < 0)
            return ret;

        const struct pipeline_params *pipeline_params = params->params;
        const int index = get_pipeline_ubo_index(pipeline_params, s->compat_info->ubindings[i], i);
        ngli_pipeline_update_buffer(s->pipeline, index, buffer, 0, buffer->size);
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

    s->compat_info = params->compat_info;
    if (s->compat_info->use_ublocks) {
        ret = init_blocks_buffers(s, params);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ngli_pipeline_compat_update_attribute(struct pipeline_compat *s, int index, const struct buffer *buffer)
{
    return ngli_pipeline_update_attribute(s->pipeline, index, buffer);
}

int ngli_pipeline_compat_update_uniform(struct pipeline_compat *s, int index, const void *value)
{
    if (!s->compat_info->use_ublocks)
        return ngli_pipeline_update_uniform(s->pipeline, index, value);

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    const int stage = index >> 16;
    const int field_index = index & 0xffff;
    const struct block *block = &s->compat_info->ublocks[stage];
    const struct block_field *fields = ngli_darray_data(&block->fields);
    const struct block_field *field = &fields[field_index];
    if (value) {
        uint8_t *dst = s->mapped_datas[stage] + field->offset;
        ngli_block_field_copy(field, dst, value);
    }

    return 0;
}

int ngli_pipeline_compat_update_texture(struct pipeline_compat *s, int index, const struct texture *texture)
{
    return ngli_pipeline_update_texture(s->pipeline, index, texture);
}

void ngli_pipeline_compat_update_texture_info(struct pipeline_compat *s, const struct pgcraft_texture_info *info)
{
    const struct pgcraft_texture_info_field *fields = info->fields;
    const struct image *image = info->image;

    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_TIMESTAMP].index, &image->ts);

    if (image->params.layout) {
        const float dimensions[] = {image->params.width, image->params.height, image->params.depth};
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
    for (int i = 0; i < NGLI_ARRAY_NB(samplers); i++) {
        const int sampler = samplers[i];
        const int index = fields[sampler].index;
        const struct texture *texture = textures[sampler];
        ret &= ngli_pipeline_compat_update_texture(s, index, texture);
    };

    const int layout = ret < 0 ? NGLI_IMAGE_LAYOUT_NONE : image->params.layout;
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &layout);
}

int ngli_pipeline_compat_update_buffer(struct pipeline_compat *s, int index, const struct buffer *buffer, int offset, int size)
{
    return ngli_pipeline_update_buffer(s->pipeline, index, buffer, offset, size);
}

void ngli_pipeline_compat_draw(struct pipeline_compat *s, int nb_vertices, int nb_instances)
{
    ngli_pipeline_draw(s->pipeline, nb_vertices, nb_instances);
}

void ngli_pipeline_compat_draw_indexed(struct pipeline_compat *s, const struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    ngli_pipeline_draw_indexed(s->pipeline, indices, indices_format, nb_indices, nb_instances);
}

void ngli_pipeline_compat_dispatch(struct pipeline_compat *s, int nb_group_x, int nb_group_y, int nb_group_z)
{
    ngli_pipeline_dispatch(s->pipeline, nb_group_x, nb_group_y, nb_group_z);
}

void ngli_pipeline_compat_freep(struct pipeline_compat **sp)
{
    struct pipeline_compat *s = *sp;
    if (!s)
        return;
    ngli_pipeline_freep(&s->pipeline);
    if (s->compat_info && s->compat_info->use_ublocks) {
        for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
            if (s->ubuffers[i]) {
                ngli_buffer_unmap(s->ubuffers[i]);
                ngli_buffer_freep(&s->ubuffers[i]);
            }
        }
    }
    ngli_freep(sp);
}
