/*
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

#ifndef ATLAS_H
#define ATLAS_H

#include <stdint.h>
#include <stdlib.h>

struct ngl_ctx;
struct atlas;

struct bitmap {
    uint8_t *buffer;
    size_t stride;
    uint32_t width, height;
};

struct atlas *ngli_atlas_create(struct ngl_ctx *ctx);

int ngli_atlas_init(struct atlas *s);
int ngli_atlas_add_bitmap(struct atlas *s, const struct bitmap *bitmap, uint32_t *bitmap_id);
int ngli_atlas_finalize(struct atlas *s);

struct ngpu_texture *ngli_atlas_get_texture(const struct atlas *s);
void ngli_atlas_get_bitmap_coords(const struct atlas *s, uint32_t bitmap_id, uint32_t *dst);

void ngli_atlas_freep(struct atlas **sp);

#endif
