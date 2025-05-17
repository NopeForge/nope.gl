/*
 * Copyright 2024-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2017-2022 GoPro Inc.
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
#include "memory.h"
#include "utils.h"
#include "crc32.h"
#include "string.h"

struct bucket {
    struct hmap_entry *entries;
    size_t nb_entries;
};

struct key_funcs {
    uint32_t (*hash)(union hmap_key x);             // mixing/hashing of a key
    int (*cmp)(union hmap_key a, union hmap_key b); // compare 2 keys (0 if identical)
    union hmap_key (*dup)(union hmap_key x);        // create a copy of the key
    int (*check)(union hmap_key x);                 // check whether the key is valid or not
    void (*free)(union hmap_key x);                 // free a key
};

struct hmap {
    struct bucket *buckets;
    size_t size;
    size_t mask;
    size_t count; // total number of entries
    ngli_user_free_func_type user_free_func;
    void *user_arg;
    struct hmap_ref first;
    struct hmap_ref last;
    enum hmap_type type;
    struct key_funcs key_funcs;
};

void ngli_hmap_set_free_func(struct hmap *hm, ngli_user_free_func_type user_free_func, void *user_arg)
{
    ngli_assert(!hm->count);
    hm->user_free_func = user_free_func;
    hm->user_arg = user_arg;
}

#define NO_REF (struct hmap_ref){.bucket_id = SIZE_MAX}
#define HAS_REF(ref) ((ref).bucket_id != SIZE_MAX)

static uint32_t key_hash_str(union hmap_key x) { return ngli_crc32(x.str); }
static uint32_t key_hash_u64(union hmap_key x) { return ngli_crc32_mem(x.u8_8, sizeof(x.u8_8)); }

static int key_cmp_str(union hmap_key a, union hmap_key b) { return strcmp(a.str, b.str); }
static int key_cmp_u64(union hmap_key a, union hmap_key b) { return a.u64 != b.u64; }

static union hmap_key key_dup_str(union hmap_key x) { return (union hmap_key){.str=ngli_strdup(x.str)}; }
static union hmap_key key_dup_u64(union hmap_key x) { return x; }

static int key_check_str(union hmap_key x) { return !!x.str; }
static int key_check_u64(union hmap_key x) { return 1; }

static void key_free_str(union hmap_key x) { ngli_free(x.str); }
static void key_free_u64(union hmap_key x) { }

static const struct key_funcs key_funcs_map[] = {
    [NGLI_HMAP_TYPE_STR] = {key_hash_str, key_cmp_str, key_dup_str, key_check_str, key_free_str},
    [NGLI_HMAP_TYPE_U64] = {key_hash_u64, key_cmp_u64, key_dup_u64, key_check_u64, key_free_u64},
};

struct hmap *ngli_hmap_create(enum hmap_type type)
{
    struct hmap *hm = ngli_calloc(1, sizeof(*hm));
    if (!hm)
        return NULL;
    hm->size = 1 << HMAP_SIZE_NBIT;
    hm->mask = hm->size - 1;
    hm->buckets = ngli_calloc(hm->size, sizeof(*hm->buckets));
    if (!hm->buckets) {
        ngli_free(hm);
        return NULL;
    }
    hm->first = hm->last = NO_REF;
    hm->type = type;
    hm->key_funcs = key_funcs_map[type];
    return hm;
}

size_t ngli_hmap_count(const struct hmap *hm)
{
    return hm->count;
}

static struct hmap_entry *entry_from_ref(const struct hmap *hm, struct hmap_ref ref)
{
    return HAS_REF(ref) ? &hm->buckets[ref.bucket_id].entries[ref.entry_id] : NULL;
}

static struct hmap_ref ref_from_entry(const struct hmap *hm, const struct hmap_entry *entry)
{
    if (!entry)
        return NO_REF;
    struct hmap_ref ref = {
        .bucket_id = entry->bucket_id,
        .entry_id  = (size_t)(entry - hm->buckets[entry->bucket_id].entries),
    };
    return ref;
}

static struct hmap_entry *fixed_entry_from_ref(const struct hmap *hm, struct hmap_ref ref,
                                               struct hmap_ref removed, size_t current_id)
{
    if (!HAS_REF(ref))
        return NULL;
    const int need_fix =
        /* Only the current bucket has entry references shifted */
        ref.bucket_id == removed.bucket_id &&
        /* Only the entries after the removed one are shifted */
        ref.entry_id > removed.entry_id
        /*
         * The following last condition prevents from fixing the reference
         * twice (and thus decrementing too much): if the passed reference
         * `ref` (be it a next or prev, it doesn't matter) is located earlier
         * in the current bucket, it means the complementary reference (prev if
         * next, next if prev) in that particular entry was for sure already
         * fixed. Indeed, since the current entry points to it, it means that
         * the referred entry was also pointing to the current entry with its
         * complementary reference.
         */
        && current_id < ref.entry_id - 1;
    const size_t entry_id = ref.entry_id - (size_t)need_fix;
    return &hm->buckets[ref.bucket_id].entries[entry_id];
}

/*
 * Every entry starting at removed.entry_id is offset by 1 (since the buffer
 * was memory moved after the target entry got removed). We need to fix the
 * references to all these entries, which can be simply done with a -1 on
 * entry indexes (or global pointers indexes) referencing them.
 *
 * Note that we can not use entry_from_ref() directly, because the
 * e->{prev,next} could be referencing an entry in the same bucket, and so we
 * need to offset them in this particular situation; see fixed_entry_from_ref().
 */
static void fix_refs(struct hmap *hm, struct bucket *b, struct hmap_ref removed)
{
    for (size_t i = removed.entry_id; i < b->nb_entries; i++) {
        struct hmap_entry *e = &b->entries[i];
        struct hmap_entry *prev = fixed_entry_from_ref(hm, e->prev, removed, i);
        struct hmap_entry *next = fixed_entry_from_ref(hm, e->next, removed, i);
        struct hmap_ref *prev_ref = prev ? &prev->next : &hm->first;
        struct hmap_ref *next_ref = next ? &next->prev : &hm->last;
        prev_ref->entry_id--;
        next_ref->entry_id--;
    }
}

static int add_entry(struct hmap *hm, struct bucket *b, union hmap_key key, void *data, size_t id)
{
    struct hmap_entry *entries = ngli_realloc(b->entries, b->nb_entries + 1, sizeof(*b->entries));
    if (!entries)
        return NGL_ERROR_MEMORY;
    b->entries = entries;
    struct hmap_entry *e = &entries[b->nb_entries++];
    e->key = key;
    e->data = data;
    e->bucket_id = id;
    e->prev = hm->last;
    e->next = NO_REF;

    /* update global first reference */
    const struct hmap_ref ref = ref_from_entry(hm, e);
    if (!HAS_REF(hm->first))
        hm->first = ref;

    /* update global last reference */
    struct hmap_entry *last = entry_from_ref(hm, hm->last);
    if (last)
        last->next = ref;
    hm->last = ref;

    hm->count++;
    return 0;
}

static int hmap_set(struct hmap *hm, union hmap_key key, void *data)
{
    if (!hm->key_funcs.check(key))
        return NGL_ERROR_INVALID_ARG;

    const uint32_t hash = hm->key_funcs.hash(key);
    size_t id = (size_t)hash & hm->mask;
    struct bucket *b = &hm->buckets[id];

    /* Delete */
    if (!data) {
        for (size_t i = 0; i < b->nb_entries; i++) {
            struct hmap_entry *e = &b->entries[i];
            if (!hm->key_funcs.cmp(e->key, key)) {

                /* Link previous and next entries together */
                struct hmap_entry *prev = entry_from_ref(hm, e->prev);
                struct hmap_entry *next = entry_from_ref(hm, e->next);
                struct hmap_ref *link_from_back  = prev ? &prev->next : &hm->first;
                struct hmap_ref *link_from_front = next ? &next->prev : &hm->last;
                *link_from_back = e->next;
                *link_from_front = e->prev;

                /* Keep a reference pre-remove for fixing the refs later */
                const struct hmap_ref removed = ref_from_entry(hm, e);

                hm->key_funcs.free(e->key);
                if (hm->user_free_func)
                    hm->user_free_func(hm->user_arg, e->data);
                hm->count--;
                b->nb_entries--;
                if (!b->nb_entries) {
                    ngli_freep(&b->entries);
                } else {
                    memmove(e, e + 1, (b->nb_entries - i) * sizeof(*b->entries));
                    struct hmap_entry *entries =
                        ngli_realloc(b->entries, b->nb_entries, sizeof(*b->entries));
                    if (!entries)
                        return 0; // unable to realloc but entry got dropped, so this is OK
                    b->entries = entries;
                    fix_refs(hm, b, removed);
                }
                return 1;
            }
        }
        return 0;
    }

    /* Replace */
    for (size_t i = 0; i < b->nb_entries; i++) {
        struct hmap_entry *e = &b->entries[i];
        if (!hm->key_funcs.cmp(e->key, key)) {
            if (hm->user_free_func)
                hm->user_free_func(hm->user_arg, e->data);
            e->data = data;
            return 0;
        }
    }

    /* Resize check before addition  */
    if (hm->count * 3 / 4 >= hm->size) {
        struct hmap old_hm = *hm;

#if HAVE_BUILTIN_OVERFLOW
        size_t new_size;
        if (__builtin_mul_overflow(hm->size, 2, &new_size))
            return NGL_ERROR_LIMIT_EXCEEDED;
#else
        /* Also includes the realloc overflow check */
        if (hm->size >= 1ULL << (sizeof(hm->size)*8 - 2))
            return NGL_ERROR_LIMIT_EXCEEDED;
        size_t new_size = hm->size * 2;
#endif

        struct bucket *new_buckets = ngli_calloc(new_size, sizeof(*new_buckets));
        if (new_buckets) {
            hm->buckets = new_buckets;
            hm->count = 0;
            hm->size = new_size;
            hm->mask = hm->size - 1;
            hm->first = hm->last = NO_REF;

            if (old_hm.count) {
                /* Transfer all entries to the new map */
                const struct hmap_entry *e = NULL;
                while ((e = ngli_hmap_next(&old_hm, e))) {
                    const size_t new_id = (size_t)hm->key_funcs.hash(e->key) & hm->mask;
                    struct bucket *new_bucket = &hm->buckets[new_id];
                    int ret = add_entry(hm, new_bucket, e->key, e->data, new_id);
                    if (ret < 0) {
                        /* Unable to allocate more, ngli_free the incomplete buckets
                         * and restore the previous hashmap state */
                        for (size_t j = 0; j < hm->size; j++)
                            ngli_free(hm->buckets[j].entries);
                        ngli_free(hm->buckets);
                        *hm = old_hm;
                        return NGL_ERROR_MEMORY;
                    }
                }

                /* Destroy previous indexes in the old buckets */
                for (size_t j = 0; j < old_hm.size; j++) {
                    struct bucket *old_bucket = &old_hm.buckets[j];
                    ngli_free(old_bucket->entries);
                    old_hm.count -= old_bucket->nb_entries;
                    if (old_hm.count == 0)
                        break;
                }
            }

            ngli_free(old_hm.buckets);

            /* Fix the bucket position for the entry to add */
            id = hash & hm->mask;
            b = &hm->buckets[id];
        }
    }

    /* Add */
    union hmap_key new_key = hm->key_funcs.dup(key);
    if (!hm->key_funcs.check(new_key))
        return NGL_ERROR_MEMORY;
    int ret = add_entry(hm, b, new_key, data, id);
    if (ret < 0) {
        hm->key_funcs.free(new_key);
        return ret;
    }

    return 0;
}

int ngli_hmap_set_str(struct hmap *hm, const char *str, void *data)
{
    ngli_assert(hm->type == NGLI_HMAP_TYPE_STR);
    const union hmap_key key = {.str=(char *)str};
    return hmap_set(hm, key, data);
}

int ngli_hmap_set_u64(struct hmap *hm, uint64_t u64, void *data)
{
    ngli_assert(hm->type == NGLI_HMAP_TYPE_U64);
    const union hmap_key key = {.u64=u64};
    return hmap_set(hm, key, data);
}

struct hmap_entry *ngli_hmap_next(const struct hmap *hm,
                                  const struct hmap_entry *prev)
{
    return entry_from_ref(hm, prev ? prev->next : hm->first);
}

static void *hmap_get(const struct hmap *hm, union hmap_key key)
{
    const size_t id = (size_t)hm->key_funcs.hash(key) & hm->mask;
    const struct bucket *b = &hm->buckets[id];

    for (size_t i = 0; i < b->nb_entries; i++) {
        struct hmap_entry *e = &b->entries[i];
        if (!hm->key_funcs.cmp(e->key, key))
            return e->data;
    }
    return NULL;
}

void *ngli_hmap_get_str(const struct hmap *hm, const char *str)
{
    ngli_assert(hm->type == NGLI_HMAP_TYPE_STR);
    const union hmap_key key = {.str=(char *)str};
    return hmap_get(hm, key);
}

void *ngli_hmap_get_u64(const struct hmap *hm, uint64_t u64)
{
    ngli_assert(hm->type == NGLI_HMAP_TYPE_U64);
    const union hmap_key key = {.u64=u64};
    return hmap_get(hm, key);
}

void ngli_hmap_freep(struct hmap **hmp)
{
    struct hmap *hm = *hmp;

    if (!hm)
        return;

    if (hm->count) {
        for (size_t j = 0; j < hm->size; j++) {
            struct bucket *b = &hm->buckets[j];
            for (size_t i = 0; i < b->nb_entries; i++) {
                struct hmap_entry *e = &b->entries[i];
                hm->key_funcs.free(e->key);
                if (hm->user_free_func)
                    hm->user_free_func(hm->user_arg, e->data);
            }
            ngli_free(b->entries);
            hm->count -= b->nb_entries;
            if (hm->count == 0)
                break;
        }
    }

    ngli_free(hm->buckets);
    ngli_freep(hmp);
}
