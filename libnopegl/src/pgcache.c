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

#include <string.h>

#include "memory.h"
#include "internal.h"
#include "pgcache.h"
#include "utils.h"

static void reset_cached_program(void *user_arg, void *data)
{
    struct gpu_program *p = data;
    ngli_gpu_program_freep(&p);
}

static void reset_cached_frag_map(void *user_arg, void *data)
{
    struct hmap *p = data;
    ngli_hmap_freep(&p);
}

int ngli_pgcache_init(struct pgcache *s, struct gpu_ctx *gpu_ctx)
{
    s->gpu_ctx = gpu_ctx;
    s->graphics_cache = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    s->compute_cache = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!s->graphics_cache || !s->compute_cache)
        return NGL_ERROR_MEMORY;
    ngli_hmap_set_free_func(s->graphics_cache, reset_cached_frag_map, s);
    ngli_hmap_set_free_func(s->compute_cache, reset_cached_program, s);
    return 0;
}

static int query_cache(struct pgcache *s, struct gpu_program **dstp,
                       struct hmap *cache, const char *cache_key,
                       const struct gpu_program_params *params)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    struct gpu_program *cached_program = ngli_hmap_get_str(cache, cache_key);
    if (cached_program) {
        /* make sure the cached program has not been reset by the user */
        ngli_assert(cached_program->gpu_ctx);

        *dstp = cached_program;
        return 0;
    }

    /* this is free'd by the reset_cached_program() when destroying the cache */
    struct gpu_program *new_program = ngli_gpu_program_create(gpu_ctx);
    if (!new_program)
        return NGL_ERROR_MEMORY;

    int ret = ngli_gpu_program_init(new_program, params);
    if (ret < 0) {
        ngli_gpu_program_freep(&new_program);
        return ret;
    }

    ret = ngli_hmap_set_str(cache, cache_key, new_program);
    if (ret < 0) {
        ngli_gpu_program_freep(&new_program);
        return ret;
    }

    *dstp = new_program;
    return 0;
}

int ngli_pgcache_get_graphics_program(struct pgcache *s, struct gpu_program **dstp, const struct gpu_program_params *params)
{
    /*
     * The first dimension of the graphics_cache hmap is another hmap: what we
     * do is basically graphics_cache[vert][frag] to obtain the program. If the
     * 2nd hmap is not yet allocated, we do create a new one here.
     */
    struct hmap *frag_map = ngli_hmap_get_str(s->graphics_cache, params->vertex);
    if (!frag_map) {
        frag_map = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
        if (!frag_map)
            return NGL_ERROR_MEMORY;
        ngli_hmap_set_free_func(frag_map, reset_cached_program, s);

        int ret = ngli_hmap_set_str(s->graphics_cache, params->vertex, frag_map);
        if (ret < 0) {
            ngli_hmap_freep(&frag_map);
            return NGL_ERROR_MEMORY;
        }
    }

    return query_cache(s, dstp, frag_map, params->fragment, params);
}

int ngli_pgcache_get_compute_program(struct pgcache *s, struct gpu_program **dstp, const struct gpu_program_params *params)
{
    return query_cache(s, dstp, s->compute_cache, params->compute, params);
}

void ngli_pgcache_reset(struct pgcache *s)
{
    if (!s->gpu_ctx)
        return;
    ngli_hmap_freep(&s->compute_cache);
    ngli_hmap_freep(&s->graphics_cache);
    memset(s, 0, sizeof(*s));
}
