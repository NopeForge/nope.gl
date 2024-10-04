/*
 * Copyright 2018-2022 GoPro Inc.
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

#include "gpu_buffer.h"
#include "gpu_ctx.h"
#include "utils.h"

static void buffer_freep(struct gpu_buffer **sp)
{
    if (!*sp)
        return;

    (*sp)->gpu_ctx->cls->buffer_freep(sp);
}

struct gpu_buffer *ngli_gpu_buffer_create(struct gpu_ctx *gpu_ctx)
{
    struct gpu_buffer *s = gpu_ctx->cls->buffer_create(gpu_ctx);
    s->rc = NGLI_RC_CREATE(buffer_freep);
    return s;
}

int ngli_gpu_buffer_init(struct gpu_buffer *s, size_t size, uint32_t usage)
{
    s->size = size;
    s->usage = usage;

    return s->gpu_ctx->cls->buffer_init(s);
}

int ngli_gpu_buffer_upload(struct gpu_buffer *s, const void *data, size_t offset, size_t size)
{
    return s->gpu_ctx->cls->buffer_upload(s, data, offset, size);
}

int ngli_gpu_buffer_map(struct gpu_buffer *s, size_t offset, size_t size, void **datap)
{
    return s->gpu_ctx->cls->buffer_map(s, offset, size, datap);
}

void ngli_gpu_buffer_unmap(struct gpu_buffer *s)
{
    s->gpu_ctx->cls->buffer_unmap(s);
}

void ngli_gpu_buffer_freep(struct gpu_buffer **sp)
{
    NGLI_RC_UNREFP(sp);
}
