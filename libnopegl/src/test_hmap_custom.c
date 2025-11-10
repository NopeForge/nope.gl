/*
 * Copyright 2025 Nope Forge
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

#define HMAP_SIZE_NBIT 1
#include "utils/crc32.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "utils/utils.h"

#define PRINT_HMAP(...) do {                                                 \
    printf(__VA_ARGS__);                                                     \
    const struct hmap_entry *e = NULL;                                       \
    while ((e = ngli_hmap_next(hm, e))) {                                    \
        const struct key *key = e->key.ptr;                                  \
        printf("  %08X %s, %s: %s\n",                                        \
            ngli_crc32_mem(e->key.ptr, sizeof(struct key), NGLI_CRC32_INIT), \
            blend_factor_to_str[key->blend_dst_factor],                      \
            blend_factor_to_str[key->blend_src_factor],                      \
            (const char *)e->data);                                          \
    }                                                                        \
    printf("\n");                                                            \
} while (0)

#define RSTR "replaced"

static void free_func(void *arg, void *data)
{
    ngli_free(data);
}

enum blend_factor {
    BLEND_FACTOR_ZERO,
    BLEND_FACTOR_ONE,
    BLEND_FACTOR_SRC_COLOR,
    BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    BLEND_FACTOR_DST_COLOR,
    BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    BLEND_FACTOR_SRC_ALPHA,
    BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    BLEND_FACTOR_DST_ALPHA,
    BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    BLEND_FACTOR_NB,
    BLEND_FACTOR_MAX_ENUM = 0x7FFFFFFF
};

const char *blend_factor_to_str[] = {
    [BLEND_FACTOR_ZERO]                     = "zero",
    [BLEND_FACTOR_ONE]                      = "one",
    [BLEND_FACTOR_SRC_COLOR]                = "src_color",
    [BLEND_FACTOR_ONE_MINUS_SRC_COLOR]      = "one_minus_src_color",
    [BLEND_FACTOR_DST_COLOR]                = "dst_color",
    [BLEND_FACTOR_ONE_MINUS_DST_COLOR]      = "one_minus_dst_color",
    [BLEND_FACTOR_SRC_ALPHA]                = "src_alpha",
    [BLEND_FACTOR_ONE_MINUS_SRC_ALPHA]      = "one_minus_src_alpha",
    [BLEND_FACTOR_DST_ALPHA]                = "dst_alpha",
    [BLEND_FACTOR_ONE_MINUS_DST_ALPHA]      = "one_minus_dst_alpha",
};

struct key {
    enum blend_factor blend_dst_factor;
    enum blend_factor blend_src_factor;
};

static const struct {
    struct key key;
    const char *val;
} kvs[] = {
    {{BLEND_FACTOR_ONE,                 BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}, "src_over"},
    {{BLEND_FACTOR_ONE_MINUS_DST_ALPHA, BLEND_FACTOR_ONE},                 "dst_over"},
    {{BLEND_FACTOR_ONE_MINUS_DST_ALPHA, BLEND_FACTOR_ZERO},                "src_out"},
    {{BLEND_FACTOR_ZERO,                BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}, "dst_out"},
    {{BLEND_FACTOR_DST_ALPHA,           BLEND_FACTOR_ZERO},                "src_in"},
    {{BLEND_FACTOR_ZERO,                BLEND_FACTOR_SRC_ALPHA},           "dst_in"},
    {{BLEND_FACTOR_DST_ALPHA,           BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}, "src_atop"},
    {{BLEND_FACTOR_ONE_MINUS_DST_ALPHA, BLEND_FACTOR_SRC_ALPHA},           "dst_atop"},
    {{BLEND_FACTOR_ONE_MINUS_DST_ALPHA, BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}, "xor"},
};

static size_t get_key_index(const struct key *key)
{
    for (size_t i = 0; i < NGLI_ARRAY_NB(kvs); i++)
        if (!memcmp(&kvs[i].key, key, sizeof(struct key)))
            return i;
    return SIZE_MAX;
}

static void check_order(const struct hmap *hm)
{
    size_t last_index = SIZE_MAX;
    const struct hmap_entry *e = NULL;
    while ((e = ngli_hmap_next(hm, e))) {
        const size_t index = get_key_index(e->key.ptr);
        ngli_assert(last_index == SIZE_MAX || index > last_index);
        last_index = index;
    }
}

static uint32_t key_hash(union hmap_key x)
{
    return ngli_crc32_mem(x.ptr, sizeof(struct key), NGLI_CRC32_INIT);
}

static int key_cmp(union hmap_key a, union hmap_key b)
{
    return memcmp(a.ptr, b.ptr, sizeof(struct key));
}

static union hmap_key key_dup(union hmap_key x)
{
    return (union hmap_key){.ptr=ngli_memdup(x.ptr, sizeof(struct key))};
}

static int key_check(union hmap_key x)
{
    return !!x.ptr;
}

static void key_free(union hmap_key x)
{
    ngli_free(x.ptr);
}

static const struct hmap_key_funcs key_funcs = {
    key_hash,
    key_cmp,
    key_dup,
    key_check,
    key_free,
};

int main(void)
{
    struct hmap *hm = ngli_hmap_create_ptr(&key_funcs);
    ngli_hmap_set_free_func(hm, free_func, NULL);

    /* Test addition */
    for (size_t i = 0; i < NGLI_ARRAY_NB(kvs); i++) {
        void *data = ngli_strdup(kvs[i].val);
        ngli_assert(ngli_hmap_set_ptr(hm, &kvs[i].key, data) >= 0);
        const char *val = ngli_hmap_get_ptr(hm, &kvs[i].key);
        ngli_assert(val);
        ngli_assert(!strcmp(val, kvs[i].val));
        check_order(hm);
    }

    PRINT_HMAP("init [%zu entries]:\n", ngli_hmap_count(hm));

    for (size_t i = 0; i < NGLI_ARRAY_NB(kvs) - 1; i++) {
        /* Test replace */
        if (i & 1) {
            void *data = ngli_strdup(RSTR);
            ngli_assert(ngli_hmap_set_ptr(hm, &kvs[i].key, data) == 0);
            const char *val = ngli_hmap_get_ptr(hm, &kvs[i].key);
            ngli_assert(val);
            ngli_assert(strcmp(val, RSTR) == 0);
            PRINT_HMAP("replace %s:\n", kvs[i].val);
            check_order(hm);
        }

        /* Test delete */
        ngli_assert(ngli_hmap_set_ptr(hm, &kvs[i].key, NULL) == 1);
        ngli_assert(ngli_hmap_set_ptr(hm, &kvs[i].key, NULL) == 0);
        PRINT_HMAP("drop %s (%zu remaining):\n", kvs[i].val, ngli_hmap_count(hm));
        check_order(hm);
    }

    ngli_hmap_freep(&hm);

    return 0;
}
