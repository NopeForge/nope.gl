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

#include "ngpu/buffer.h"
#include "ngpu/program.h" // MAX_ID_LEN
#include "ngpu/type.h"
#include "utils/darray.h"

struct ngpu_ctx;

enum ngpu_block_layout {
    NGPU_BLOCK_LAYOUT_UNKNOWN,
    NGPU_BLOCK_LAYOUT_STD140,
    NGPU_BLOCK_LAYOUT_STD430,
    NGPU_BLOCK_NB_LAYOUTS,
    NGPU_BLOCK_LAYOUT_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_block_field {
    char name[MAX_ID_LEN];
    enum ngpu_type type;
    size_t count;
    size_t offset;
    size_t size;
    size_t stride;
    enum ngpu_precision precision;
};

struct ngpu_block_field_data {
    const void *data;
    size_t count;
};

void ngpu_block_field_copy(const struct ngpu_block_field *fi, uint8_t *dst, const uint8_t *src);
void ngpu_block_field_copy_count(const struct ngpu_block_field *fi, uint8_t *dst, const uint8_t *src, size_t count);

struct ngpu_block_desc {
    struct ngpu_ctx *gpu_ctx;
    enum ngpu_block_layout layout;
    struct darray fields; // block_field
    size_t size;
};

void ngpu_block_desc_fields_copy(const struct ngpu_block_desc *s, const struct ngpu_block_field_data *src_array, uint8_t *dst);

#define NGPU_BLOCK_DESC_VARIADIC_COUNT SIZE_MAX

void ngpu_block_desc_init(struct ngpu_ctx *gpu_ctx, struct ngpu_block_desc *s, enum ngpu_block_layout layout);
size_t ngpu_block_desc_get_size(const struct ngpu_block_desc *s, size_t variadic_count);

/*
 * Get the block size aligned to the minimum offset alignment required by the
 * GPU. This function is useful when one wants to pack multiple blocks into the
 * same buffer as it ensures that the next block offset into the buffer will
 * honor the GPU offset alignment constraints.
 */
size_t ngpu_block_desc_get_aligned_size(const struct ngpu_block_desc *s, size_t variadic_count);

int ngpu_block_desc_add_field(struct ngpu_block_desc *s, const char *name, enum ngpu_type type, size_t count);
int ngpu_block_desc_add_fields(struct ngpu_block_desc *s, const struct ngpu_block_field *fields, size_t count);

void ngpu_block_desc_reset(struct ngpu_block_desc *s);

#endif
