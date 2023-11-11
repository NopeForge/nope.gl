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

#include "darray.h"
#include "utils.h"

struct my_item {
    int id;
    void *ptr;
};

static void free_elem(void *user_arg, void *data)
{
    struct my_item *item = data;
    ngli_assert(!strcmp(user_arg, "test"));
    free(item->ptr);
}

static void test_free(void)
{
    struct darray darray;
    ngli_darray_init(&darray, sizeof(struct my_item), 0);

    void *user_arg = "test";
    ngli_darray_set_free_func(&darray, free_elem, user_arg);

    struct my_item p0 = {.id=0x12, .ptr=malloc(10)};
    struct my_item p1 = {.id=0x34, .ptr=malloc(20)};
    struct my_item p2 = {.id=0x56, .ptr=malloc(30)};
    struct my_item p3 = {.id=0x78, .ptr=malloc(40)};
    struct my_item p4 = {.id=0x9a, .ptr=malloc(50)};
    struct my_item p5 = {.id=0xbc, .ptr=malloc(60)};

    ngli_assert(p0.ptr && p1.ptr && p2.ptr && p3.ptr && p4.ptr && p5.ptr);
    ngli_assert(ngli_darray_push(&darray, &p0));
    ngli_assert(ngli_darray_push(&darray, &p1));
    ngli_assert(ngli_darray_push(&darray, &p2));
    ngli_assert(ngli_darray_push(&darray, &p3));
    ngli_assert(ngli_darray_push(&darray, &p4));
    ngli_assert(ngli_darray_push(&darray, &p5));

    ngli_darray_remove_range(&darray, 1, 3);
    ngli_assert(ngli_darray_count(&darray) == 3);
    ngli_darray_clear(&darray);
    ngli_assert(ngli_darray_count(&darray) == 0);

    ngli_darray_reset(&darray);
}

int main(void)
{
    struct darray darray = {0};
    ngli_darray_init(&darray, sizeof(int), 0);

    size_t count = ngli_darray_count(&darray);
    ngli_assert(count == 0);

    int *element = ngli_darray_push(&darray, NULL);
    ngli_assert(element);
    *element = 0xFF;
    count = ngli_darray_count(&darray);
    ngli_assert(count == 1);

    element = ngli_darray_push(&darray, NULL);
    ngli_assert(element);
    *element = 0xFFFF;
    count = ngli_darray_count(&darray);
    ngli_assert(count == 2);

    element = ngli_darray_tail(&darray);
    ngli_assert(*element == 0xFFFF);

    element = ngli_darray_pop(&darray);
    ngli_assert(*element == 0xFFFF);

    element = ngli_darray_pop(&darray);
    ngli_assert(*element == 0xFF);

    element = ngli_darray_pop(&darray);
    ngli_assert(element == NULL);

    count = ngli_darray_count(&darray);
    ngli_assert(count == 0);

    for (size_t i = 0; i < 1000; i++) {
        element = ngli_darray_push(&darray, NULL);
        ngli_assert(element);
    }

    ngli_darray_reset(&darray);

    test_free();

    return 0;
}
