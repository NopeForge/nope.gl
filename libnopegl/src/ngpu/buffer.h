/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_BUFFER_H
#define NGPU_BUFFER_H

#include <stdlib.h>

#include "utils/utils.h"
#include "utils/refcount.h"

struct ngpu_ctx;

enum ngpu_buffer_usage {
    NGPU_BUFFER_USAGE_DYNAMIC_BIT        = 1 << 0,
    NGPU_BUFFER_USAGE_TRANSFER_SRC_BIT   = 1 << 1,
    NGPU_BUFFER_USAGE_TRANSFER_DST_BIT   = 1 << 2,
    NGPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 1 << 3,
    NGPU_BUFFER_USAGE_STORAGE_BUFFER_BIT = 1 << 4,
    NGPU_BUFFER_USAGE_INDEX_BUFFER_BIT   = 1 << 5,
    NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT  = 1 << 6,
    NGPU_BUFFER_USAGE_MAP_READ           = 1 << 7,
    NGPU_BUFFER_USAGE_MAP_WRITE          = 1 << 8,
    NGPU_BUFFER_USAGE_MAP_PERSISTENT     = 1 << 9,
    NGPU_BUFFER_USAGE_MAX_ENUM           = 0x7FFFFFFF
};

struct ngpu_buffer {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    size_t size;
    uint32_t usage;
};

NGLI_RC_CHECK_STRUCT(ngpu_buffer);

struct ngpu_buffer *ngpu_buffer_create(struct ngpu_ctx *gpu_ctx);
int ngpu_buffer_init(struct ngpu_buffer *s, size_t size, uint32_t usage);
int ngpu_buffer_wait(struct ngpu_buffer *s);
int ngpu_buffer_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size);
int ngpu_buffer_map(struct ngpu_buffer *s, size_t offset, size_t size, void **datap);
void ngpu_buffer_unmap(struct ngpu_buffer *s);
void ngpu_buffer_freep(struct ngpu_buffer **sp);

#endif
