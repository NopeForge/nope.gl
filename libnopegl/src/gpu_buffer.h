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

#ifndef GPU_BUFFER_H
#define GPU_BUFFER_H

#include <stdlib.h>

#include "utils.h"

struct gpu_ctx;

enum {
    NGLI_GPU_BUFFER_USAGE_DYNAMIC_BIT        = 1 << 0,
    NGLI_GPU_BUFFER_USAGE_TRANSFER_SRC_BIT   = 1 << 1,
    NGLI_GPU_BUFFER_USAGE_TRANSFER_DST_BIT   = 1 << 2,
    NGLI_GPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 1 << 3,
    NGLI_GPU_BUFFER_USAGE_STORAGE_BUFFER_BIT = 1 << 4,
    NGLI_GPU_BUFFER_USAGE_INDEX_BUFFER_BIT   = 1 << 5,
    NGLI_GPU_BUFFER_USAGE_VERTEX_BUFFER_BIT  = 1 << 6,
    NGLI_GPU_BUFFER_USAGE_MAP_READ           = 1 << 7,
    NGLI_GPU_BUFFER_USAGE_MAP_WRITE          = 1 << 8,
    NGLI_GPU_BUFFER_USAGE_NB
};

struct gpu_buffer {
    struct ngli_rc rc;
    struct gpu_ctx *gpu_ctx;
    size_t size;
    int usage;
};

NGLI_RC_CHECK_STRUCT(gpu_buffer);

struct gpu_buffer *ngli_gpu_buffer_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_buffer_init(struct gpu_buffer *s, size_t size, int usage);
int ngli_gpu_buffer_upload(struct gpu_buffer *s, const void *data, size_t offset, size_t size);
int ngli_gpu_buffer_map(struct gpu_buffer *s, size_t offset, size_t size, void **datap);
void ngli_gpu_buffer_unmap(struct gpu_buffer *s);
void ngli_gpu_buffer_freep(struct gpu_buffer **sp);

#endif
