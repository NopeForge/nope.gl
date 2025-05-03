/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Forge
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

#ifndef DISTMAP_H
#define DISTMAP_H

#include <stdint.h>

#include "path.h"

#define NGLI_DISTMAP_FLAG_PATH_AUTO_CLOSE (1 << 0) // Consider all sub-paths to be closed

struct ngl_ctx;
struct distmap;

struct distmap *ngli_distmap_create(struct ngl_ctx *ctx);

int ngli_distmap_init(struct distmap *s);
int ngli_distmap_add_shape(struct distmap *s, uint32_t shape_w, uint32_t shape_h,
                           const struct path *path, uint32_t flags, uint32_t *shape_id);
int ngli_distmap_finalize(struct distmap *s);

struct ngpu_texture *ngli_distmap_get_texture(const struct distmap *s);
void ngli_distmap_get_shape_coords(const struct distmap *s, uint32_t shape_id, int32_t *dst);
void ngli_distmap_get_shape_scale(const struct distmap *s, uint32_t shape_id, float *dst);

void ngli_distmap_freep(struct distmap **sp);

#endif
