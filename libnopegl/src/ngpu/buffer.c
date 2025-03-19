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

#include "buffer.h"
#include "ctx.h"
#include "utils/utils.h"

static void buffer_freep(void **bufferp)
{
    struct ngpu_buffer **sp = (struct ngpu_buffer **)bufferp;
    if (!*sp)
        return;

    ngpu_buffer_wait(*sp);

    (*sp)->gpu_ctx->cls->buffer_freep(sp);
}

struct ngpu_buffer *ngpu_buffer_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_buffer *s = gpu_ctx->cls->buffer_create(gpu_ctx);
    if (!s)
        return NULL;
    s->rc = NGLI_RC_CREATE(buffer_freep);
    return s;
}

int ngpu_buffer_init(struct ngpu_buffer *s, size_t size, uint32_t usage)
{
    s->size = size;
    s->usage = usage;

    return s->gpu_ctx->cls->buffer_init(s);
}

int ngpu_buffer_wait(struct ngpu_buffer *s)
{
    return s->gpu_ctx->cls->buffer_wait(s);
}

int ngpu_buffer_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size)
{
    int ret = ngpu_buffer_wait(s);
    if (ret < 0)
        return ret;
    return s->gpu_ctx->cls->buffer_upload(s, data, offset, size);
}

int ngpu_buffer_map(struct ngpu_buffer *s, size_t offset, size_t size, void **datap)
{
    int ret = ngpu_buffer_wait(s);
    if (ret < 0)
        return ret;
    return s->gpu_ctx->cls->buffer_map(s, offset, size, datap);
}

void ngpu_buffer_unmap(struct ngpu_buffer *s)
{
    s->gpu_ctx->cls->buffer_unmap(s);
}

void ngpu_buffer_freep(struct ngpu_buffer **sp)
{
    NGLI_RC_UNREFP(sp);
}
