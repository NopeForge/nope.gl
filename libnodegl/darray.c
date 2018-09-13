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


#define _POSIX_C_SOURCE 200809L // posix_memalign()

#include <stdlib.h>
#include <string.h>

#include "darray.h"
#include "utils.h"

static int reserve_non_aligned(struct darray *darray, int capacity)
{
    if (capacity < darray->capacity)
        return 0;

    void *ptr = realloc(darray->data, capacity * darray->element_size);
    if (!ptr)
        return -1;
    darray->data = ptr;
    darray->capacity = capacity;
    return 0;
}

static int reserve_aligned(struct darray *darray, int capacity)
{
    if (capacity < darray->capacity)
        return 0;

    void *ptr;
    if (posix_memalign(&ptr, NGLI_ALIGN_VAL, capacity * darray->element_size))
        return -1;
    if (darray->data) {
        memcpy(ptr, darray->data, darray->capacity * darray->element_size);
        free(darray->data);
    }
    darray->data = ptr;
    darray->capacity = capacity;
    return 0;
}

void ngli_darray_init(struct darray *darray, int element_size, int aligned)
{
    darray->data = NULL;
    darray->size = 0;
    darray->capacity = 0;
    darray->element_size = element_size;
    darray->reserve = aligned ? reserve_aligned : reserve_non_aligned;
}

void *ngli_darray_push(struct darray *darray, const void *element)
{
    if (darray->size >= darray->capacity - 1) {
        if (darray->capacity >= 1 << (sizeof(darray->capacity)*8 - 2))
            return NULL;
        int ret = darray->reserve(darray, darray->capacity ? darray->capacity << 1: 8);
        if (ret < 0)
            return NULL;
    }
    void *new_element = darray->data + darray->size * darray->element_size;
    darray->size++;
    if (element)
        memcpy(new_element, element, darray->element_size);
    return new_element;
}

void *ngli_darray_tail(struct darray *darray)
{
    if (darray->size <= 0)
        return NULL;
    return darray->data + (darray->size - 1) * darray->element_size;
}

void *ngli_darray_pop(struct darray *darray)
{
    void *element = ngli_darray_tail(darray);
    darray->size = darray->size > 0 ? darray->size - 1 : 0;
    return element;
}

int ngli_darray_size(struct darray *darray)
{
    return darray->size;
}

void *ngli_darray_get(struct darray *darray, int index)
{
    if (index < 0 || index >= darray->size)
        return NULL;
    return darray->data + index * darray->element_size;
}

void ngli_darray_reset(struct darray *darray)
{
    free(darray->data);
    memset(darray, 0, sizeof(*darray));
}
