/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <string.h>

#include "math_utils.h"
#include "ngpu/bindgroup.h"
#include "ngpu/ctx.h"
#include "ngpu/limits.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"

#define NB_BINDGROUPS 16

struct pipeline_compat {
    struct ngpu_ctx *gpu_ctx;
    enum ngpu_pipeline_type type;
    struct ngpu_pipeline_graphics graphics;
    const struct ngpu_program *program;
    struct ngpu_pipeline *pipeline;
    struct ngpu_bindgroup_layout_desc bindgroup_layout_desc;
    struct ngpu_bindgroup_layout *bindgroup_layout;
    struct darray bindgroups;
    struct ngpu_bindgroup *cur_bindgroup;
    size_t cur_bindgroup_index;
    const struct ngpu_buffer **vertex_buffers;
    size_t nb_vertex_buffers;
    struct ngpu_texture_binding *textures;
    size_t nb_textures;
    struct ngpu_buffer_binding *buffers;
    size_t nb_buffers;
    uint32_t dynamic_offsets[NGPU_MAX_DYNAMIC_OFFSETS];
    size_t nb_dynamic_offsets;
    int updated;
    int need_pipeline_recreation;
    const struct pgcraft_compat_info *compat_info;
    struct ngpu_buffer *ubuffers[NGPU_PROGRAM_SHADER_NB];
    uint8_t *mapped_datas[NGPU_PROGRAM_SHADER_NB];
};

static int wait_buffer(struct pipeline_compat *s, int stage)
{
    if (s->mapped_datas[stage])
        return 0;

    struct ngpu_buffer *buffer = s->ubuffers[stage];
    return ngpu_buffer_wait(buffer);
}

static int map_buffer(struct pipeline_compat *s, int stage)
{
    if (s->mapped_datas[stage])
        return 0;

    struct ngpu_buffer *buffer = s->ubuffers[stage];
    return ngpu_buffer_map(buffer, 0, buffer->size, (void **) &s->mapped_datas[stage]);
}

static void unmap_buffers(struct pipeline_compat *s)
{
    for (size_t i = 0; i < NGPU_PROGRAM_SHADER_NB; i++) {
        if (s->mapped_datas[i]) {
            ngpu_buffer_unmap(s->ubuffers[i]);
            s->mapped_datas[i] = NULL;
        }
    }
}

struct pipeline_compat *ngli_pipeline_compat_create(struct ngpu_ctx *gpu_ctx)
{
    struct pipeline_compat *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->gpu_ctx = gpu_ctx;
    return s;
}

static int init_blocks_buffers(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    for (size_t i = 0; i < NGPU_PROGRAM_SHADER_NB; i++) {
        const size_t block_size = ngpu_block_desc_get_size(&s->compat_info->ublocks[i], 0);
        if (!block_size)
            continue;

        struct ngpu_buffer *buffer = ngpu_buffer_create(gpu_ctx);
        if (!buffer)
            return NGL_ERROR_MEMORY;
        s->ubuffers[i] = buffer;

        int ret = ngpu_buffer_init(buffer,
                                       block_size,
                                       NGPU_BUFFER_USAGE_DYNAMIC_BIT |
                                       NGPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                       NGPU_BUFFER_USAGE_MAP_WRITE);
        if (ret < 0)
            return ret;

        if (gpu_ctx->features & NGPU_FEATURE_BUFFER_MAP_PERSISTENT) {
            ret = ngpu_buffer_map(buffer, 0, buffer->size, (void **) &s->mapped_datas[i]);
            if (ret < 0)
                return ret;
        }

        ngli_pipeline_compat_update_buffer(s, s->compat_info->uindices[i], buffer, 0, buffer->size);
    }

    return 0;
}

static void free_bindgroup(void *user_arg, void *data)
{
    struct ngpu_bindgroup **bindgroup = data;
    ngpu_bindgroup_freep(bindgroup);
}

static int grow_bindgroup_array(struct pipeline_compat *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    size_t count = ngli_darray_count(&s->bindgroups);
    if (count == 0) {
        ngli_darray_init(&s->bindgroups, sizeof(struct ngpu_bindgroup *), 0);
        ngli_darray_set_free_func(&s->bindgroups, free_bindgroup, NULL);
        count = NB_BINDGROUPS;
    }

    for (size_t i = 0; i < count; i++) {
        struct ngpu_bindgroup *bindgroup = ngpu_bindgroup_create(gpu_ctx);
        if (!bindgroup)
            return NGL_ERROR_MEMORY;

        struct ngpu_bindgroup_params params = {
            .layout    = s->bindgroup_layout,
            .resources = {
                .textures    = s->textures,
                .nb_textures = s->nb_textures,
                .buffers     = s->buffers,
                .nb_buffers  = s->nb_buffers,
            },
        };

        int ret = ngpu_bindgroup_init(bindgroup, &params);
        if (ret < 0) {
            ngpu_bindgroup_freep(&bindgroup);
            return ret;
        }

        if (!ngli_darray_push(&s->bindgroups, &bindgroup)) {
            ngpu_bindgroup_freep(&bindgroup);
            return NGL_ERROR_MEMORY;
        }
    }

    return 0;
}

static int create_pipeline(struct pipeline_compat *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    s->bindgroup_layout = ngpu_bindgroup_layout_create(gpu_ctx);
    if (!s->bindgroup_layout)
        return NGL_ERROR_MEMORY;

    int ret = ngpu_bindgroup_layout_init(s->bindgroup_layout, &s->bindgroup_layout_desc);
    if (ret < 0)
        return ret;

    s->pipeline = ngpu_pipeline_create(gpu_ctx);
    if (!s->pipeline)
        return NGL_ERROR_MEMORY;

    const struct ngpu_pipeline_params pipeline_params = {
        .type     = s->type,
        .graphics = s->graphics,
        .program  = s->program,
        .layout   = {
            .bindgroup_layout = s->bindgroup_layout,
        }
    };

    ret = ngpu_pipeline_init(s->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    ret = grow_bindgroup_array(s);
    if (ret < 0)
        return ret;

    s->cur_bindgroup = *(struct ngpu_bindgroup **)ngli_darray_get(&s->bindgroups, 0);
    s->cur_bindgroup_index = 0;

    /* Initialize bindgroup before first pipeline execution */
    s->updated = 1;

    return 0;
}

static void reset_pipeline(struct pipeline_compat *s)
{
    ngpu_pipeline_freep(&s->pipeline);
    ngli_darray_clear(&s->bindgroups);
    s->cur_bindgroup = NULL;
    s->cur_bindgroup_index = 0;
    ngpu_bindgroup_layout_freep(&s->bindgroup_layout);
}

int ngli_pipeline_compat_init(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    s->type = params->type;

    int ret = ngpu_pipeline_graphics_copy(&s->graphics, &params->graphics);
    if (ret < 0)
        return ret;

    s->program = params->program;

    NGLI_ARRAY_MEMDUP(&s->bindgroup_layout_desc, &params->layout_desc, textures);
    NGLI_ARRAY_MEMDUP(&s->bindgroup_layout_desc, &params->layout_desc, buffers);

    const struct ngpu_bindgroup_resources *bindgroup_resources = &params->resources;
    NGLI_ARRAY_MEMDUP(s, bindgroup_resources, buffers);
    NGLI_ARRAY_MEMDUP(s, bindgroup_resources, textures);

    const struct ngpu_vertex_resources *vertex_resources = &params->vertex_resources;
    NGLI_ARRAY_MEMDUP(s, vertex_resources, vertex_buffers);

    s->compat_info = params->compat_info;
    ret = init_blocks_buffers(s, params);
    if (ret < 0)
        return ret;

    ret = create_pipeline(s);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_pipeline_compat_update_vertex_buffer(struct pipeline_compat *s, int32_t index, const struct ngpu_buffer *buffer)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_vertex_buffers);
    s->vertex_buffers[index] = buffer;
    return 0;
}

int ngli_pipeline_compat_update_uniform_count(struct pipeline_compat *s, int32_t index, const void *value, size_t count)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    const int32_t stage = index >> 16;
    const int32_t field_index = index & 0xffff;
    const struct ngpu_block_desc *block = &s->compat_info->ublocks[stage];
    const struct ngpu_block_field *fields = ngli_darray_data(&block->fields);
    const struct ngpu_block_field *field = &fields[field_index];
    if (value) {
        if (!(gpu_ctx->features & NGPU_FEATURE_BUFFER_MAP_PERSISTENT)) {
            int ret = map_buffer(s, stage);
            if (ret < 0)
                return ret;
        } else {
            int ret = wait_buffer(s, stage);
            if (ret < 0)
                return ret;
        }
        uint8_t *dst = s->mapped_datas[stage] + field->offset;
        ngpu_block_field_copy_count(field, dst, value, count);
    }

    return 0;
}

int ngli_pipeline_compat_update_uniform(struct pipeline_compat *s, int32_t index, const void *value)
{
    return ngli_pipeline_compat_update_uniform_count(s, index, value, 0);
}

static int update_texture(struct pipeline_compat *s, int32_t index, const struct ngpu_texture_binding *binding)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_textures);

    if (s->textures[index].immutable_sampler != binding->immutable_sampler) {
        struct ngpu_bindgroup_layout_entry *entry = &s->bindgroup_layout_desc.textures[index];
        entry->immutable_sampler = binding->immutable_sampler;
        s->need_pipeline_recreation = 1;
    }

    s->textures[index] = *binding;
    s->updated = 1;

    return 0;
}

int ngli_pipeline_compat_update_texture(struct pipeline_compat *s, int32_t index, const struct ngpu_texture *texture)
{
    const struct ngpu_texture_binding binding = {.texture = texture};
    return update_texture(s, index, &binding);
}

int ngli_pipeline_compat_update_dynamic_offsets(struct pipeline_compat *s, const uint32_t *offsets, size_t nb_offsets)
{
    ngli_assert(s->bindgroup_layout->nb_dynamic_offsets == nb_offsets);
    memcpy(s->dynamic_offsets, offsets, nb_offsets * sizeof(*s->dynamic_offsets));
    s->nb_dynamic_offsets = nb_offsets;
    return 0;
}

void ngli_pipeline_compat_apply_reframing_matrix(struct pipeline_compat *s, int32_t index, const struct image *image, const float *reframing)
{
    if (index == -1)
        return;

    ngli_assert(index >= 0 && index < s->compat_info->nb_texture_infos);
    const struct pgcraft_texture_info *info = &s->compat_info->texture_infos[index];
    const struct pgcraft_texture_info_field *fields = info->fields;

    if (fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index == -1)
        return;

    /* Scale up from normalized [0,1] UV to centered [-1,1], swapping y-axis */
    static const NGLI_ALIGNED_MAT(remap_uv_to_centered) = {
        2.f,  0.f, 0.f, 0.f,
        0.f, -2.f, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
       -1.f,  1.f, 0.f, 1.f,
    };

    /* Scale down from centered [-1,1] to normalized [0,1] UV, swapping y-axis */
    static const NGLI_ALIGNED_MAT(remap_centered_to_uv) = {
        .5f,  0.f, 0.f, 0.f,
        0.f, -.5f, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
        .5f,  .5f, 0.f, 1.f,
    };

    NGLI_ALIGNED_MAT(inverse_reframing);
    ngli_mat4_inverse(inverse_reframing, reframing);

    NGLI_ALIGNED_MAT(matrix);
    ngli_mat4_mul(matrix, remap_uv_to_centered, image->coordinates_matrix);
    ngli_mat4_mul(matrix, inverse_reframing, matrix);
    ngli_mat4_mul(matrix, remap_centered_to_uv, matrix);

    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, matrix);
}

void ngli_pipeline_compat_update_image(struct pipeline_compat *s, int32_t index, const struct image *image)
{
    if (index == -1)
        return;

    ngli_assert(index >= 0 && index < s->compat_info->nb_texture_infos);
    const struct pgcraft_texture_info *info = &s->compat_info->texture_infos[index];
    const struct pgcraft_texture_info_field *fields = info->fields;

    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_TIMESTAMP].index, &image->ts);

    if (image->params.layout) {
        const float dimensions[] = {(float)image->params.width, (float)image->params.height, (float)image->params.depth};
        ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_DIMENSIONS].index, dimensions);
    }

    struct ngpu_texture_binding bindings[NGLI_INFO_FIELD_NB] = {0};
    switch (image->params.layout) {
    case NGLI_IMAGE_LAYOUT_DEFAULT:
        bindings[NGLI_INFO_FIELD_SAMPLER_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_0].immutable_sampler = image->samplers[0];
        break;
    case NGLI_IMAGE_LAYOUT_NV12:
        bindings[NGLI_INFO_FIELD_SAMPLER_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_0].immutable_sampler = image->samplers[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].texture = image->planes[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].immutable_sampler = image->samplers[1];
        break;
    case NGLI_IMAGE_LAYOUT_NV12_RECTANGLE:
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].immutable_sampler = image->samplers[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_1].texture = image->planes[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_1].immutable_sampler = image->samplers[1];
        break;
    case NGLI_IMAGE_LAYOUT_MEDIACODEC:
        bindings[NGLI_INFO_FIELD_SAMPLER_OES].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_OES].immutable_sampler = image->samplers[0];
        break;
    case NGLI_IMAGE_LAYOUT_YUV:
        bindings[NGLI_INFO_FIELD_SAMPLER_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_0].immutable_sampler = image->samplers[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].texture = image->planes[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].immutable_sampler = image->samplers[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_2].texture = image->planes[2];
        bindings[NGLI_INFO_FIELD_SAMPLER_2].immutable_sampler = image->samplers[2];
        break;
    case NGLI_IMAGE_LAYOUT_RECTANGLE:
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].immutable_sampler = image->samplers[0];
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
        const int32_t binding_index = fields[sampler].index;
        const struct ngpu_texture_binding *binding = &bindings[sampler];
        ret &= update_texture(s, binding_index, binding);
    };

    const enum image_layout layout = ret < 0 ? NGLI_IMAGE_LAYOUT_NONE : image->params.layout;
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &layout);
}

int ngli_pipeline_compat_update_buffer(struct pipeline_compat *s, int32_t index, const struct ngpu_buffer *buffer, size_t offset, size_t size)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_buffers);
    s->buffers[index] = (struct ngpu_buffer_binding) {
        .buffer = buffer,
        .offset = offset,
        .size   = size ? size : buffer->size,
    };
    s->updated = 1;
    return 0;
}

static int select_next_available_bindgroup(struct pipeline_compat *s)
{
    /* If current bindgroup is not in use, select it */
    if (s->cur_bindgroup->rc.count == 1)
        return 0;

    /* Otherwhise, check if next bindgroup is available  */
    size_t bindgroup_index = (s->cur_bindgroup_index + 1) % ngli_darray_count(&s->bindgroups);
    struct ngpu_bindgroup *bindgroup = *(struct ngpu_bindgroup **)ngli_darray_get(&s->bindgroups, bindgroup_index);
    if (bindgroup->rc.count == 1) {
        s->cur_bindgroup = bindgroup;
        s->cur_bindgroup_index = bindgroup_index;
        return 0;
    }

    /*
     * If it is not, save next newly-allocated bind group index and increase
     * our bindgroup pool size
     */
    bindgroup_index = ngli_darray_count(&s->bindgroups);

    int ret = grow_bindgroup_array(s);
    if (ret < 0)
        return ret;

    /* Select bindgroup and assert that it is not in use */
    s->cur_bindgroup = *(struct ngpu_bindgroup **)ngli_darray_get(&s->bindgroups, bindgroup_index);
    s->cur_bindgroup_index = bindgroup_index;
    ngli_assert(s->cur_bindgroup->rc.count == 1);

    return 0;
}

static int prepare_bindgroup(struct pipeline_compat *s)
{
    if (!s->updated)
        return 0;

    s->updated = 0;

    if (s->need_pipeline_recreation) {
        s->need_pipeline_recreation = 0;
        reset_pipeline(s);
        int ret = create_pipeline(s);
        if (ret < 0)
            return ret;
    }

    int ret = select_next_available_bindgroup(s);
    if (ret < 0)
        return ret;

    for (size_t i = 0; i < s->nb_textures; i++) {
        ret = ngpu_bindgroup_update_texture(s->cur_bindgroup, (int32_t) i, &s->textures[i]);
        if (ret < 0)
            return ret;
    }

    for (size_t i = 0; i < s->nb_buffers; i++) {
        ret = ngpu_bindgroup_update_buffer(s->cur_bindgroup, (int32_t) i, &s->buffers[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int prepare_pipeline(struct pipeline_compat *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    if (!(gpu_ctx->features & NGPU_FEATURE_BUFFER_MAP_PERSISTENT))
       unmap_buffers(s);

    int ret = prepare_bindgroup(s);
    if (ret < 0)
        return ret;

    return 0;
}

void ngli_pipeline_compat_draw(struct pipeline_compat *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    int ret = prepare_pipeline(s);
    if (ret < 0)
        return;

    ngpu_ctx_set_pipeline(gpu_ctx, s->pipeline);
    for (size_t i = 0; i < s->nb_vertex_buffers; i++)
        ngpu_ctx_set_vertex_buffer(gpu_ctx, (uint32_t)i, s->vertex_buffers[i]);
    ngpu_ctx_set_bindgroup(gpu_ctx, s->cur_bindgroup, s->dynamic_offsets, s->nb_dynamic_offsets);
    ngpu_ctx_draw(gpu_ctx, nb_vertices, nb_instances, first_vertex);
}

void ngli_pipeline_compat_draw_indexed(struct pipeline_compat *s, const struct ngpu_buffer *indices, enum ngpu_format indices_format, uint32_t nb_indices, uint32_t nb_instances)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    int ret = prepare_pipeline(s);
    if (ret < 0)
        return;

    ngpu_ctx_set_pipeline(gpu_ctx, s->pipeline);
    for (size_t i = 0; i < s->nb_vertex_buffers; i++)
        ngpu_ctx_set_vertex_buffer(gpu_ctx, (uint32_t)i, s->vertex_buffers[i]);
    ngpu_ctx_set_index_buffer(gpu_ctx, indices, indices_format);
    ngpu_ctx_set_bindgroup(gpu_ctx, s->cur_bindgroup, s->dynamic_offsets, s->nb_dynamic_offsets);
    ngpu_ctx_draw_indexed(gpu_ctx, nb_indices, nb_instances);
}

void ngli_pipeline_compat_dispatch(struct pipeline_compat *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    int ret = prepare_pipeline(s);
    if (ret < 0)
        return;

    ngpu_ctx_set_pipeline(gpu_ctx, s->pipeline);
    ngpu_ctx_set_bindgroup(gpu_ctx, s->cur_bindgroup, s->dynamic_offsets, s->nb_dynamic_offsets);
    ngpu_ctx_dispatch(gpu_ctx, nb_group_x, nb_group_y, nb_group_z);
}

void ngli_pipeline_compat_freep(struct pipeline_compat **sp)
{
    struct pipeline_compat *s = *sp;
    if (!s)
        return;

    reset_pipeline(s);
    ngli_darray_reset(&s->bindgroups);

    ngpu_pipeline_graphics_reset(&s->graphics);

    ngli_freep(&s->bindgroup_layout_desc.textures);
    ngli_freep(&s->bindgroup_layout_desc.buffers);

    ngli_freep(&s->vertex_buffers);
    ngli_freep(&s->textures);
    ngli_freep(&s->buffers);

    if (s->compat_info) {
        for (size_t i = 0; i < NGPU_PROGRAM_SHADER_NB; i++) {
            if (s->ubuffers[i]) {
                if (s->mapped_datas[i])
                    ngpu_buffer_unmap(s->ubuffers[i]);
                ngpu_buffer_freep(&s->ubuffers[i]);
            }
        }
    }
    ngli_freep(sp);
}
