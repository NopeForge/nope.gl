/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2020-2022 GoPro Inc.
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

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "block_desc.h"
#include "ngpu/ctx.h"
#include "ngpu/type.h"
#include "utils.h"

void ngpu_block_desc_init(struct ngpu_ctx *gpu_ctx, struct ngpu_block_desc *s, enum ngpu_block_layout layout)
{
    s->gpu_ctx = gpu_ctx;
    s->layout = layout;
    ngli_darray_init(&s->fields, sizeof(struct ngpu_block_field), 0);
}

static const size_t strides_map[NGPU_BLOCK_NB_LAYOUTS][NGPU_TYPE_NB] = {
    [NGPU_BLOCK_LAYOUT_STD140] = {
        [NGPU_TYPE_BOOL]   = sizeof(int)   * 4,
        [NGPU_TYPE_I32]    = sizeof(int32_t)  * 4,
        [NGPU_TYPE_IVEC2]  = sizeof(int32_t)  * 4,
        [NGPU_TYPE_IVEC3]  = sizeof(int32_t)  * 4,
        [NGPU_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
        [NGPU_TYPE_U32]    = sizeof(uint32_t) * 4,
        [NGPU_TYPE_UVEC2]  = sizeof(uint32_t) * 4,
        [NGPU_TYPE_UVEC3]  = sizeof(uint32_t) * 4,
        [NGPU_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
        [NGPU_TYPE_F32]    = sizeof(float) * 4,
        [NGPU_TYPE_VEC2]   = sizeof(float) * 4,
        [NGPU_TYPE_VEC3]   = sizeof(float) * 4,
        [NGPU_TYPE_VEC4]   = sizeof(float) * 4,
        [NGPU_TYPE_MAT3]   = sizeof(float) * 4 * 3,
        [NGPU_TYPE_MAT4]   = sizeof(float) * 4 * 4,
    },
    [NGPU_BLOCK_LAYOUT_STD430] = {
        [NGPU_TYPE_BOOL]   = sizeof(int)   * 1,
        [NGPU_TYPE_I32]    = sizeof(int32_t)  * 1,
        [NGPU_TYPE_IVEC2]  = sizeof(int32_t)  * 2,
        [NGPU_TYPE_IVEC3]  = sizeof(int32_t)  * 4,
        [NGPU_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
        [NGPU_TYPE_U32]    = sizeof(uint32_t) * 1,
        [NGPU_TYPE_UVEC2]  = sizeof(uint32_t) * 2,
        [NGPU_TYPE_UVEC3]  = sizeof(uint32_t) * 4,
        [NGPU_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
        [NGPU_TYPE_F32]    = sizeof(float) * 1,
        [NGPU_TYPE_VEC2]   = sizeof(float) * 2,
        [NGPU_TYPE_VEC3]   = sizeof(float) * 4,
        [NGPU_TYPE_VEC4]   = sizeof(float) * 4,
        [NGPU_TYPE_MAT3]   = sizeof(float) * 4 * 3,
        [NGPU_TYPE_MAT4]   = sizeof(float) * 4 * 4,
    },
};

static const size_t sizes_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_BOOL]   = sizeof(int)   * 1,
    [NGPU_TYPE_I32]    = sizeof(int32_t)  * 1,
    [NGPU_TYPE_IVEC2]  = sizeof(int32_t)  * 2,
    [NGPU_TYPE_IVEC3]  = sizeof(int32_t)  * 3,
    [NGPU_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
    [NGPU_TYPE_U32]    = sizeof(uint32_t) * 1,
    [NGPU_TYPE_UVEC2]  = sizeof(uint32_t) * 2,
    [NGPU_TYPE_UVEC3]  = sizeof(uint32_t) * 3,
    [NGPU_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
    [NGPU_TYPE_F32]    = sizeof(float) * 1,
    [NGPU_TYPE_VEC2]   = sizeof(float) * 2,
    [NGPU_TYPE_VEC3]   = sizeof(float) * 3,
    [NGPU_TYPE_VEC4]   = sizeof(float) * 4,
    [NGPU_TYPE_MAT3]   = sizeof(float) * 4 * 3,
    [NGPU_TYPE_MAT4]   = sizeof(float) * 4 * 4,
};

static const size_t aligns_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_BOOL]   = sizeof(int)   * 1,
    [NGPU_TYPE_I32]    = sizeof(int32_t)  * 1,
    [NGPU_TYPE_IVEC2]  = sizeof(int32_t)  * 2,
    [NGPU_TYPE_IVEC3]  = sizeof(int32_t)  * 4,
    [NGPU_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
    [NGPU_TYPE_U32]    = sizeof(uint32_t) * 1,
    [NGPU_TYPE_UVEC2]  = sizeof(uint32_t) * 2,
    [NGPU_TYPE_UVEC3]  = sizeof(uint32_t) * 4,
    [NGPU_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
    [NGPU_TYPE_F32]    = sizeof(float) * 1,
    [NGPU_TYPE_VEC2]   = sizeof(float) * 2,
    [NGPU_TYPE_VEC3]   = sizeof(float) * 4,
    [NGPU_TYPE_VEC4]   = sizeof(float) * 4,
    [NGPU_TYPE_MAT3]   = sizeof(float) * 4,
    [NGPU_TYPE_MAT4]   = sizeof(float) * 4,
};

static size_t get_buffer_stride(const struct ngpu_block_field *field, enum ngpu_block_layout layout)
{
    return strides_map[layout][field->type];
}

static size_t get_buffer_size(const struct ngpu_block_field *field, enum ngpu_block_layout layout)
{
    return field->count * get_buffer_stride(field, layout);
}

static size_t get_field_size(const struct ngpu_block_field *field, enum ngpu_block_layout layout)
{
    if (field->count)
        return get_buffer_size(field, layout);
    return sizes_map[field->type];
}

static size_t get_field_align(const struct ngpu_block_field *field, enum ngpu_block_layout layout)
{
    if (field->count && field->type != NGPU_TYPE_MAT3 && field->type != NGPU_TYPE_MAT4)
        return get_buffer_stride(field, layout);
    return aligns_map[field->type];
}

static size_t fill_tail_field_info(const struct ngpu_block_desc *s, struct ngpu_block_field *field)
{
    /* Ignore the last field until the count is known */
    if (field->count == NGPU_BLOCK_DESC_VARIADIC_COUNT) {
        field->size = 0;
        field->stride = 0;
        field->offset = 0;
        return s->size;
    }

    const size_t size  = get_field_size(field, s->layout);
    const size_t align = get_field_align(field, s->layout);

    ngli_assert(field->type != NGPU_TYPE_NONE);
    ngli_assert(size);
    ngli_assert(align);

    const size_t remain = s->size % align;
    const size_t offset = s->size + (remain ? align - remain : 0);

    field->size   = size;
    field->stride = get_buffer_stride(field, s->layout);
    field->offset = offset;
    return offset + size;
}

size_t ngpu_block_desc_get_size(const struct ngpu_block_desc *s, size_t variadic_count)
{
    if (variadic_count == 0)
        return NGLI_ALIGN(s->size, aligns_map[NGPU_TYPE_VEC4]);

    /*
     * If the last field is variadic, we create an identical artificial field
     * where we set the count to the one specified in this function, and
     * recalculate the new size as if it was a normal field.
     */
    const struct ngpu_block_field *last = ngli_darray_tail(&s->fields);
    ngli_assert(last->count == NGPU_BLOCK_DESC_VARIADIC_COUNT);

    struct ngpu_block_field tmp = *last;
    tmp.count = variadic_count;
    size_t size = fill_tail_field_info(s, &tmp);
    return NGLI_ALIGN(size, aligns_map[NGPU_TYPE_VEC4]);
}

size_t ngpu_block_desc_get_aligned_size(const struct ngpu_block_desc *s, size_t variadic_count)
{
    const struct ngpu_limits *limits = &s->gpu_ctx->limits;
    const size_t alignment = NGLI_MAX(limits->min_uniform_block_offset_alignment,
                                      limits->min_storage_block_offset_alignment);
    return NGLI_ALIGN(ngpu_block_desc_get_size(s, variadic_count), alignment);
}

int ngpu_block_desc_add_field(struct ngpu_block_desc *s, const char *name, enum ngpu_type type, size_t count)
{
    ngli_assert(s->layout != NGPU_BLOCK_LAYOUT_UNKNOWN);

    /* check that we are not adding a field after a variadic one */
    const struct ngpu_block_field *last = ngli_darray_tail(&s->fields);
    if (last)
        ngli_assert(last->count != NGPU_BLOCK_DESC_VARIADIC_COUNT);

    struct ngpu_block_field field = {
        .type = type,
        .count = count,
    };
    snprintf(field.name, sizeof(field.name), "%s", name);
    s->size = fill_tail_field_info(s, &field);
    if (!ngli_darray_push(&s->fields, &field))
        return NGL_ERROR_MEMORY;

    const size_t nb_fields = ngli_darray_count(&s->fields);
    ngli_assert(nb_fields > 0 && nb_fields < INT_MAX);

    return (int)nb_fields - 1;
}

int ngpu_block_desc_add_fields(struct ngpu_block_desc *s, const struct ngpu_block_field *fields, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const struct ngpu_block_field *field = &fields[i];

        /* These fields are filled internally */
        ngli_assert(field->offset == 0);
        ngli_assert(field->size == 0);
        ngli_assert(field->stride == 0);

        int ret = ngpu_block_desc_add_field(s, field->name, field->type, field->count);
        if (ret < 0)
            return ret;
    }
    return 0;
}

void ngpu_block_field_copy_count(const struct ngpu_block_field *fi, uint8_t * restrict dst, const uint8_t * restrict src, size_t count)
{
    uint8_t *dstp = dst;
    const uint8_t *srcp = src;

    if (fi->type == NGPU_TYPE_MAT3) {
        const size_t dst_vec_stride = fi->stride / 3;
        const size_t src_vec_stride = sizes_map[NGPU_TYPE_VEC3];
        const size_t n = 3 * NGLI_MAX(count ? count : fi->count, 1);
        for (size_t i = 0; i < n; i++) {
            memcpy(dstp, srcp, src_vec_stride);
            dstp += dst_vec_stride;
            srcp += src_vec_stride;
        }
        return;
    }

    const size_t src_stride = sizes_map[fi->type];
    const size_t n = NGLI_MAX(count ? count : fi->count, 1);
    for (size_t i = 0; i < n; i++) {
        memcpy(dstp, srcp, src_stride);
        dstp += fi->stride;
        srcp += src_stride;
    }
}

void ngpu_block_field_copy(const struct ngpu_block_field *fi, uint8_t *dst, const uint8_t *src)
{
    ngpu_block_field_copy_count(fi, dst, src, 0);
}

void ngpu_block_desc_fields_copy(const struct ngpu_block_desc *s, const struct ngpu_block_field_data *src_array, uint8_t *dst)
{
    const struct ngpu_block_field *fields = ngli_darray_data(&s->fields);
    for (size_t i = 0; i < ngli_darray_count(&s->fields); i++) {
        const struct ngpu_block_field_data *src = &src_array[i];
        const struct ngpu_block_field *fi = &fields[i];
        ngpu_block_field_copy_count(fi, dst + fi->offset, src->data, src->count);
    }
}

void ngpu_block_desc_reset(struct ngpu_block_desc *s)
{
    ngli_darray_reset(&s->fields);
    memset(s, 0, sizeof(*s));
}
