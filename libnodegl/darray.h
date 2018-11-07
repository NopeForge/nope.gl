/*
 * Copyright 2018 GoPro Inc.
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

struct darray {
    uint8_t *data;
    int size;
    int capacity;
    int element_size;
    int (*reserve)(struct darray *darray, int capacity);
    void (*release)(void *ptr);
};

void ngli_darray_init(struct darray *darray, int element_size, int aligned);
void *ngli_darray_push(struct darray *darray, const void *element);
void *ngli_darray_pop(struct darray *darray);
void *ngli_darray_tail(struct darray *darray);
void *ngli_darray_get(struct darray *darray, int index);
void ngli_darray_reset(struct darray *darray);

static inline int ngli_darray_count(struct darray *darray)
{
    return darray->size;
}

#endif
