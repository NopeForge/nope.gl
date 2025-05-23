/*
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

#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#include "gpu_buffer.h"
#include "darray.h"
#include "gpu_program.h" // MAX_ID_LEN

struct gpu_ctx;

enum block_layout {
    NGLI_BLOCK_LAYOUT_UNKNOWN,
    NGLI_BLOCK_LAYOUT_STD140,
    NGLI_BLOCK_LAYOUT_STD430,
    NGLI_BLOCK_NB_LAYOUTS
};

struct block_field {
    char name[MAX_ID_LEN];
    int type;
    size_t count;
    size_t offset;
    size_t size;
    size_t stride;
    int precision;
};

void ngli_block_field_copy(const struct block_field *fi, uint8_t *dst, const uint8_t *src);
void ngli_block_field_copy_count(const struct block_field *fi, uint8_t *dst, const uint8_t *src, size_t count);

struct block {
    struct gpu_ctx *gpu_ctx;
    enum block_layout layout;
    struct darray fields; // block_field
    size_t size;
};

struct block_field_data {
    const void *data;
    size_t count;
};

void ngli_block_fields_copy(const struct block *s, const struct block_field_data *src_array, uint8_t *dst);

#define NGLI_BLOCK_VARIADIC_COUNT -1

void ngli_block_init(struct gpu_ctx *gpu_ctx, struct block *s, enum block_layout layout);
size_t ngli_block_get_size(const struct block *s, size_t variadic_count);

/*
 * Get the block size aligned to the minimum offset alignment required by the
 * GPU. This function is useful when one wants to pack multiple blocks into the
 * same buffer as it ensures that the next block offset into the buffer will
 * honor the GPU offset alignment constraints.
 */
size_t ngli_block_get_aligned_size(const struct block *s, size_t variadic_count);

int ngli_block_add_field(struct block *s, const char *name, int type, size_t count);
int ngli_block_add_fields(struct block *s, const struct block_field *fields, size_t count);

void ngli_block_reset(struct block *s);

#endif
