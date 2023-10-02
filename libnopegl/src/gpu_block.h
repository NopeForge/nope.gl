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

#ifndef GPU_BLOCK_H
#define GPU_BLOCK_H

#include <stdint.h>
#include <stddef.h>

#include "block.h"
#include "buffer.h"

struct gpu_ctx;

#define NGLI_GPU_BLOCK_FIELD(_st, _name, _type, _count) \
    {.field={.name= #_name, .type=_type, .count=_count}, .offset=offsetof(_st, _name)}

struct gpu_block_field {
    struct block_field field;
    size_t offset;
};

struct gpu_block_params {
    int layout;
    int usage;
    size_t count;
    const struct gpu_block_field *fields;
    size_t nb_fields;
};

struct gpu_block {
    struct gpu_ctx *gpu_ctx;
    struct block block;
    size_t block_size;
    struct darray offsets; // array of size_t
    struct buffer *buffer;
};

int ngli_gpu_block_init(struct gpu_ctx *gpu_ctx, struct gpu_block *s, const struct gpu_block_params *params);
int ngli_gpu_block_update(struct gpu_block *s, size_t index, const void *data);
void ngli_gpu_block_reset(struct gpu_block *s);

#endif
