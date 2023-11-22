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

#ifndef DARRAY_H
#define DARRAY_H

#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

#define NGLI_DARRAY_FLAG_ALIGNED (1U << 0)

struct darray {
    uint8_t *data;
    size_t count;
    size_t capacity;
    size_t element_size;
    ngli_user_free_func_type user_free_func;
    void *user_arg;
    int (*reserve)(struct darray *darray, size_t capacity);
    void (*release)(void *ptr);
};

void ngli_darray_init(struct darray *darray, size_t element_size, uint32_t flags);
void ngli_darray_set_free_func(struct darray *darray, ngli_user_free_func_type user_free_func, void *user_arg);
void *ngli_darray_push(struct darray *darray, const void *element);
void *ngli_darray_pop(struct darray *darray);
void *ngli_darray_pop_unsafe(struct darray *darray);
void *ngli_darray_tail(const struct darray *darray);
void *ngli_darray_get(const struct darray *darray, size_t index);
void ngli_darray_remove(struct darray *darray, size_t index);
void ngli_darray_remove_range(struct darray *darray, size_t index, size_t count);
void ngli_darray_clear(struct darray *darray);
void ngli_darray_reset(struct darray *darray);

static inline size_t ngli_darray_count(const struct darray *darray)
{
    return darray->count;
}

static inline void *ngli_darray_data(const struct darray *darray)
{
    return (void *)darray->data;
}

#endif
