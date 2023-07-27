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

#include "block.h"
#include "nopegl.h"
#include "type.h"
#include "utils.h"

void ngli_block_init(struct block *s, enum block_layout layout)
{
    ngli_darray_init(&s->fields, sizeof(struct block_field), 0);
    s->layout = layout;
}

static const size_t strides_map[NGLI_BLOCK_NB_LAYOUTS][NGLI_TYPE_NB] = {
    [NGLI_BLOCK_LAYOUT_STD140] = {
        [NGLI_TYPE_BOOL]   = sizeof(int)   * 4,
        [NGLI_TYPE_I32]    = sizeof(int32_t)  * 4,
        [NGLI_TYPE_IVEC2]  = sizeof(int32_t)  * 4,
        [NGLI_TYPE_IVEC3]  = sizeof(int32_t)  * 4,
        [NGLI_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
        [NGLI_TYPE_U32]    = sizeof(uint32_t) * 4,
        [NGLI_TYPE_UVEC2]  = sizeof(uint32_t) * 4,
        [NGLI_TYPE_UVEC3]  = sizeof(uint32_t) * 4,
        [NGLI_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
        [NGLI_TYPE_F32]    = sizeof(float) * 4,
        [NGLI_TYPE_VEC2]   = sizeof(float) * 4,
        [NGLI_TYPE_VEC3]   = sizeof(float) * 4,
        [NGLI_TYPE_VEC4]   = sizeof(float) * 4,
        [NGLI_TYPE_MAT3]   = sizeof(float) * 4 * 3,
        [NGLI_TYPE_MAT4]   = sizeof(float) * 4 * 4,
    },
    [NGLI_BLOCK_LAYOUT_STD430] = {
        [NGLI_TYPE_BOOL]   = sizeof(int)   * 1,
        [NGLI_TYPE_I32]    = sizeof(int32_t)  * 1,
        [NGLI_TYPE_IVEC2]  = sizeof(int32_t)  * 2,
        [NGLI_TYPE_IVEC3]  = sizeof(int32_t)  * 4,
        [NGLI_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
        [NGLI_TYPE_U32]    = sizeof(uint32_t) * 1,
        [NGLI_TYPE_UVEC2]  = sizeof(uint32_t) * 2,
        [NGLI_TYPE_UVEC3]  = sizeof(uint32_t) * 4,
        [NGLI_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
        [NGLI_TYPE_F32]    = sizeof(float) * 1,
        [NGLI_TYPE_VEC2]   = sizeof(float) * 2,
        [NGLI_TYPE_VEC3]   = sizeof(float) * 4,
        [NGLI_TYPE_VEC4]   = sizeof(float) * 4,
        [NGLI_TYPE_MAT3]   = sizeof(float) * 4 * 3,
        [NGLI_TYPE_MAT4]   = sizeof(float) * 4 * 4,
    },
};

static const size_t sizes_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_BOOL]   = sizeof(int)   * 1,
    [NGLI_TYPE_I32]    = sizeof(int32_t)  * 1,
    [NGLI_TYPE_IVEC2]  = sizeof(int32_t)  * 2,
    [NGLI_TYPE_IVEC3]  = sizeof(int32_t)  * 3,
    [NGLI_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
    [NGLI_TYPE_U32]    = sizeof(uint32_t) * 1,
    [NGLI_TYPE_UVEC2]  = sizeof(uint32_t) * 2,
    [NGLI_TYPE_UVEC3]  = sizeof(uint32_t) * 3,
    [NGLI_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
    [NGLI_TYPE_F32]    = sizeof(float) * 1,
    [NGLI_TYPE_VEC2]   = sizeof(float) * 2,
    [NGLI_TYPE_VEC3]   = sizeof(float) * 3,
    [NGLI_TYPE_VEC4]   = sizeof(float) * 4,
    [NGLI_TYPE_MAT3]   = sizeof(float) * 4 * 3,
    [NGLI_TYPE_MAT4]   = sizeof(float) * 4 * 4,
};

static const size_t aligns_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_BOOL]   = sizeof(int)   * 1,
    [NGLI_TYPE_I32]    = sizeof(int32_t)  * 1,
    [NGLI_TYPE_IVEC2]  = sizeof(int32_t)  * 2,
    [NGLI_TYPE_IVEC3]  = sizeof(int32_t)  * 4,
    [NGLI_TYPE_IVEC4]  = sizeof(int32_t)  * 4,
    [NGLI_TYPE_U32]    = sizeof(uint32_t) * 1,
    [NGLI_TYPE_UVEC2]  = sizeof(uint32_t) * 2,
    [NGLI_TYPE_UVEC3]  = sizeof(uint32_t) * 4,
    [NGLI_TYPE_UVEC4]  = sizeof(uint32_t) * 4,
    [NGLI_TYPE_F32]    = sizeof(float) * 1,
    [NGLI_TYPE_VEC2]   = sizeof(float) * 2,
    [NGLI_TYPE_VEC3]   = sizeof(float) * 4,
    [NGLI_TYPE_VEC4]   = sizeof(float) * 4,
    [NGLI_TYPE_MAT3]   = sizeof(float) * 4,
    [NGLI_TYPE_MAT4]   = sizeof(float) * 4,
};

static size_t get_buffer_stride(const struct block_field *field, int layout)
{
    return strides_map[layout][field->type];
}

static size_t get_buffer_size(const struct block_field *field, int layout)
{
    return field->count * get_buffer_stride(field, layout);
}

static size_t get_field_size(const struct block_field *field, int layout)
{
    if (field->count)
        return get_buffer_size(field, layout);
    return sizes_map[field->type];
}

static size_t get_field_align(const struct block_field *field, int layout)
{
    if (field->count && field->type != NGLI_TYPE_MAT3 && field->type != NGLI_TYPE_MAT4)
        return get_buffer_stride(field, layout);
    return aligns_map[field->type];
}

static size_t fill_tail_field_info(const struct block *s, struct block_field *field)
{
    /* Ignore the last field until the count is known */
    if (field->count == NGLI_BLOCK_VARIADIC_COUNT) {
        field->size = 0;
        field->stride = 0;
        field->offset = 0;
        return s->size;
    }

    const size_t size  = get_field_size(field, s->layout);
    const size_t align = get_field_align(field, s->layout);

    ngli_assert(field->type != NGLI_TYPE_NONE);
    ngli_assert(size);
    ngli_assert(align);

    const size_t remain = s->size % align;
    const size_t offset = s->size + (remain ? align - remain : 0);

    field->size   = size;
    field->stride = get_buffer_stride(field, s->layout);
    field->offset = offset;
    return offset + size;
}

size_t ngli_block_get_size(const struct block *s, size_t variadic_field_count)
{
    if (variadic_field_count == 0)
        return NGLI_ALIGN(s->size, aligns_map[NGLI_TYPE_VEC4]);

    /*
     * If the last field is variadic, we create an identical artificial field
     * where we set the count to the one specified in this function, and
     * recalculate the new size as if it was a normal field.
     */
    const struct block_field *last = ngli_darray_tail(&s->fields);
    ngli_assert(last->count == NGLI_BLOCK_VARIADIC_COUNT);

    struct block_field tmp = *last;
    tmp.count = variadic_field_count;
    size_t size = fill_tail_field_info(s, &tmp);
    return NGLI_ALIGN(size, aligns_map[NGLI_TYPE_VEC4]);
}

int ngli_block_add_field(struct block *s, const char *name, int type, size_t count)
{
    ngli_assert(s->layout != NGLI_BLOCK_LAYOUT_UNKNOWN);

    /* check that we are not adding a field after a variadic one */
    const struct block_field *last = ngli_darray_tail(&s->fields);
    if (last)
        ngli_assert(last->count != NGLI_BLOCK_VARIADIC_COUNT);

    struct block_field field = {
        .type = type,
        .count = count,
    };
    snprintf(field.name, sizeof(field.name), "%s", name);
    s->size = fill_tail_field_info(s, &field);
    if (!ngli_darray_push(&s->fields, &field))
        return NGL_ERROR_MEMORY;

    return 0;
}

void ngli_block_field_copy_count(const struct block_field *fi, uint8_t * restrict dst, const uint8_t * restrict src, size_t count)
{
    uint8_t *dstp = dst;
    const uint8_t *srcp = src;

    if (fi->type == NGLI_TYPE_MAT3) {
        const size_t dst_vec_stride = fi->stride / 3;
        const size_t src_vec_stride = sizes_map[NGLI_TYPE_VEC3];
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

void ngli_block_field_copy(const struct block_field *fi, uint8_t *dst, const uint8_t *src)
{
    ngli_block_field_copy_count(fi, dst, src, 0);
}

void ngli_block_fields_copy(const struct block *s, const struct block_field_data *src_array, uint8_t *dst)
{
    const struct block_field *fields = ngli_darray_data(&s->fields);
    for (size_t i = 0; i < ngli_darray_count(&s->fields); i++) {
        const struct block_field_data *src = &src_array[i];
        const struct block_field *fi = &fields[i];
        ngli_block_field_copy_count(fi, dst + fi->offset, src->data, src->count);
    }
}

void ngli_block_reset(struct block *s)
{
    ngli_darray_reset(&s->fields);
    memset(s, 0, sizeof(*s));
}
