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

#ifndef NGPU_BLOCK_H
#define NGPU_BLOCK_H

#include <stdint.h>
#include <stddef.h>

#include "block_desc.h"
#include "buffer.h"

struct ngpu_ctx;

#define NGPU_BLOCK_FIELD(_st, _name, _type, _count) \
    {.field={.name= #_name, .type=_type, .count=_count}, .offset=offsetof(_st, _name)}

struct ngpu_block_entry {
    struct ngpu_block_field field;
    size_t offset;
};

struct ngpu_block_params {
    int layout;
    uint32_t usage;
    size_t count;
    const struct ngpu_block_entry *entries;
    size_t nb_entries;
};

struct ngpu_block {
    struct ngpu_ctx *gpu_ctx;
    struct ngpu_block_desc block_desc;
    size_t block_size;
    struct darray offsets; // array of size_t
    struct ngpu_buffer *buffer;
};

int ngpu_block_init(struct ngpu_ctx *gpu_ctx, struct ngpu_block *s, const struct ngpu_block_params *params);
int ngpu_block_update(struct ngpu_block *s, size_t index, const void *data);
void ngpu_block_reset(struct ngpu_block *s);

#endif
