/*
 * Copyright 2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_PGCACHE_H
#define NGPU_PGCACHE_H

#include "program.h"
#include "utils/hmap.h"

struct ngpu_pgcache {
    struct ngpu_ctx *gpu_ctx;
    struct hmap *graphics_cache;
    struct hmap *compute_cache;
};

int ngpu_pgcache_init(struct ngpu_pgcache *s, struct ngpu_ctx *ctx);
int ngpu_pgcache_get_graphics_program(struct ngpu_pgcache *s, struct ngpu_program **dstp, const struct ngpu_program_params *params);
int ngpu_pgcache_get_compute_program(struct ngpu_pgcache *s, struct ngpu_program **dstp, const struct ngpu_program_params *params);
void ngpu_pgcache_reset(struct ngpu_pgcache *s);

#endif
