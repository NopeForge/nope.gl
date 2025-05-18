/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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
#include "gpu_block.h"

int ngpu_block_init(struct ngpu_ctx *gpu_ctx, struct ngpu_block *s, const struct ngpu_block_params *params)
{
    s->gpu_ctx = gpu_ctx;

    const int layout = params->layout ? params->layout : NGPU_BLOCK_LAYOUT_STD140;
    ngpu_block_desc_init(gpu_ctx, &s->block, layout);
    ngli_darray_init(&s->offsets, sizeof(size_t), 0);

    size_t last_offset = SIZE_MAX;
    for (size_t i = 0; i < params->nb_entries; i++) {
        const struct ngpu_block_field *field = &params->entries[i].field;
        int ret = ngpu_block_desc_add_field(&s->block, field->name, field->type, field->count);
        if (ret < 0)
            return ret;

        size_t offset = params->entries[i].offset;
        if (!ngli_darray_push(&s->offsets, &offset))
            return NGL_ERROR_MEMORY;

        ngli_assert(last_offset == SIZE_MAX || offset > last_offset);
        last_offset = offset;
    }

    s->block_size = ngpu_block_desc_get_aligned_size(&s->block, 0);
    s->buffer = ngpu_buffer_create(gpu_ctx);
    if (!s->buffer)
        return NGL_ERROR_MEMORY;

    const size_t buffer_size = s->block_size * NGLI_MAX(params->count, 1);
    const uint32_t usage = params->usage | NGPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT | NGPU_BUFFER_USAGE_MAP_WRITE;
    int ret = ngpu_buffer_init(s->buffer, buffer_size, usage);
    if (ret < 0)
        return ret;

    return 0;
}

int ngpu_block_update(struct ngpu_block *s, size_t index, const void *data)
{
    const uint8_t *src = data;
    uint8_t *dst = NULL;
    int ret = ngpu_buffer_map(s->buffer, s->block_size * index, s->block_size, (void **) &dst);
    if (ret < 0)
        return ret;
    const struct ngpu_block_field *fields = ngli_darray_data(&s->block.fields);
    const size_t *offsets = ngli_darray_data(&s->offsets);
    for (size_t i = 0; i < ngli_darray_count(&s->block.fields); i++) {
        const struct ngpu_block_field *field = &fields[i];
        ngpu_block_field_copy_count(field, dst + field->offset, src + offsets[i], field->count);
    }
    ngpu_buffer_unmap(s->buffer);

    return 0;
}

void ngpu_block_reset(struct ngpu_block *s)
{
    ngpu_block_desc_reset(&s->block);
    ngli_darray_reset(&s->offsets);
    ngpu_buffer_freep(&s->buffer);
    memset(s, 0, sizeof(*s));
}
