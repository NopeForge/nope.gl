/*
 * Copyright 2017 GoPro Inc.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hmap.h"
#include "utils.h"

struct bucket {
    struct hmap_entry *entries;
    int nb_entries;
};

struct hmap {
    struct bucket *buckets;
    int size;
    int count; // total number of entries
    user_free_func_type user_free_func;
    void *user_arg;
};

static uint32_t get_hash(const char *key)
{
    uint32_t hash = 0;
    while (*key)
        hash = (hash<<5) ^ (hash>>27) ^ *key++;
    return hash;
}

void ngli_hmap_set_free(struct hmap *hm, user_free_func_type user_free_func, void *user_arg)
{
    hm->user_free_func = user_free_func;
    hm->user_arg = user_arg;
}

struct hmap *ngli_hmap_create(void)
{
    struct hmap *hm = calloc(1, sizeof(*hm));
    if (!hm)
        return NULL;
    hm->size = HMAP_SIZE;
    hm->buckets = calloc(hm->size, sizeof(*hm->buckets));
    if (!hm->buckets) {
        free(hm);
        return NULL;
    }
    return hm;
}

int ngli_hmap_count(struct hmap *hm)
{
    return hm->count;
}

int ngli_hmap_set(struct hmap *hm, const char *key, void *data)
{
    if (!key)
        return -1;

    const uint32_t hash = get_hash(key);
    const int id = hash % hm->size;
    struct bucket *b = &hm->buckets[id];

    /* Delete */
    if (!data) {
        for (int i = 0; i < b->nb_entries; i++) {
            struct hmap_entry *e = &b->entries[i];
            if (!strcmp(e->key, key)) {
                free(e->key);
                if (hm->user_free_func)
                    hm->user_free_func(hm->user_arg, e->data);
                hm->count--;
                b->nb_entries--;
                if (!b->nb_entries) {
                    free(b->entries);
                } else {
                    memmove(e, e + 1, (b->nb_entries - i) * sizeof(*b->entries));
                    struct hmap_entry *entries =
                        realloc(b->entries, b->nb_entries * sizeof(*b->entries));
                    if (!entries)
                        return 0; // unable to realloc but entry got dropped, so this is OK
                    b->entries = entries;
                }
                return 1;
            }
        }
        return 0;
    }

    /* Replace */
    for (int i = 0; i < b->nb_entries; i++) {
        struct hmap_entry *e = &b->entries[i];
        if (!strcmp(e->key, key)) {
            if (hm->user_free_func)
                hm->user_free_func(hm->user_arg, e->data);
            e->data = data;
            return 0;
        }
    }

    /* Resize check before addition  */
    if (hm->count * 3 / 4 >= hm->size) {
        struct hmap old_hm = *hm;

        struct bucket *new_buckets = calloc(hm->size << 1, sizeof(*new_buckets));
        if (new_buckets) {
            hm->buckets = new_buckets;
            hm->count = 0;
            hm->size <<= 1;

            if (old_hm.count) {
                /* Transfer all entries to the new map */
                const struct hmap_entry *e = NULL;
                while ((e = ngli_hmap_next(&old_hm, e))) {
                    const int new_id = get_hash(e->key) % hm->size;
                    struct bucket *b = &hm->buckets[new_id];
                    struct hmap_entry *entries =
                        realloc(b->entries, (b->nb_entries + 1) * sizeof(*b->entries));
                    if (!entries) {
                        /* Unable to allocate more, free the incomplete buckets
                         * and restore the previous hashmap state */
                        for (int j = 0; j < hm->size; j++)
                            free(hm->buckets[j].entries);
                        free(hm->buckets);
                        *hm = old_hm;
                        return -1;
                    }
                    b->entries = entries;
                    struct hmap_entry *new_e = &entries[b->nb_entries++];
                    memcpy(new_e, e, sizeof(*e));
                    hm->count++;
                }

                /* Destroy previous indexes in the old buckets */
                for (int j = 0; j < old_hm.size; j++) {
                    struct bucket *b = &old_hm.buckets[j];
                    free(b->entries);
                    old_hm.count -= b->nb_entries;
                    if (old_hm.count == 0)
                        break;
                }
            }

            free(old_hm.buckets);

            /* Fix the bucket position for the entry to add */
            b = &hm->buckets[hash % hm->size];
        }
    }

    /* Add */
    char *new_key = ngli_strdup(key);
    if (!new_key)
        return -1;
    struct hmap_entry *entries =
        realloc(b->entries, (b->nb_entries + 1) * sizeof(*b->entries));
    if (!entries) {
        free(new_key);
        return -1;
    }
    b->entries = entries;
    struct hmap_entry *e = &entries[b->nb_entries++];
    e->key = new_key;
    e->data = data;
    hm->count++;

    return 0;
}

static const struct hmap_entry *get_first_entry(const struct hmap *hm,
                                                int bucket_start)
{
    for (int i = bucket_start; i < hm->size; i++) {
        const struct bucket *b = &hm->buckets[i];
        if (b->nb_entries)
            return &b->entries[0];
    }
    return NULL;
}

const struct hmap_entry *ngli_hmap_next(const struct hmap *hm,
                                        const struct hmap_entry *prev)
{
    if (!hm->count)
        return NULL;

    if (!prev)
        return get_first_entry(hm, 0);

    const int id = get_hash(prev->key) % hm->size;
    const struct bucket *b = &hm->buckets[id];
    const int entry_id = prev - b->entries;

    if (entry_id < b->nb_entries - 1)
        return &b->entries[entry_id + 1];

    if (id < hm->size - 1)
        return get_first_entry(hm, id + 1);

    return NULL;
}

void *ngli_hmap_get(const struct hmap *hm, const char *key)
{
    const int id = get_hash(key) % hm->size;
    const struct bucket *b = &hm->buckets[id];

    for (int i = 0; i < b->nb_entries; i++) {
        struct hmap_entry *e = &b->entries[i];
        if (!strcmp(e->key, key))
            return e->data;
    }
    return NULL;
}

void ngli_hmap_freep(struct hmap **hmp)
{
    struct hmap *hm = *hmp;

    if (!hm)
        return;

    if (hm->count) {
        for (int j = 0; j < hm->size; j++) {
            struct bucket *b = &hm->buckets[j];
            for (int i = 0; i < b->nb_entries; i++) {
                struct hmap_entry *e = &b->entries[i];
                free(e->key);
                if (hm->user_free_func)
                    hm->user_free_func(hm->user_arg, e->data);
            }
            free(b->entries);
            hm->count -= b->nb_entries;
            if (hm->count == 0)
                break;
        }
    }

    free(hm->buckets);
    free(hm);
    *hmp = NULL;
}
