/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef BINDGROUP_H
#define BINDGROUP_H

#include <stdlib.h>

#include "buffer.h"
#include "darray.h"
#include "graphics_state.h"
#include "program.h"
#include "rendertarget.h"
#include "texture.h"
#include "type.h"

struct gpu_ctx;

enum {
    NGLI_ACCESS_UNDEFINED,
    NGLI_ACCESS_READ_BIT,
    NGLI_ACCESS_WRITE_BIT,
    NGLI_ACCESS_READ_WRITE,
    NGLI_ACCESS_NB
};

NGLI_STATIC_ASSERT(access, (NGLI_ACCESS_READ_BIT | NGLI_ACCESS_WRITE_BIT) == NGLI_ACCESS_READ_WRITE);

struct bindgroup_layout_entry {
    size_t id;
    int type;
    int binding;
    int access;
    int stage;
    void *immutable_sampler;
};

struct bindgroup_layout_params {
    struct bindgroup_layout_entry *textures;
    size_t nb_textures;
    struct bindgroup_layout_entry *buffers;
    size_t nb_buffers;
};

struct bindgroup_layout {
    struct gpu_ctx *gpu_ctx;
    struct bindgroup_layout_entry *textures;
    size_t nb_textures;
    struct bindgroup_layout_entry *buffers;
    size_t nb_buffers;
    size_t nb_dynamic_offsets;
};

struct texture_binding {
    const struct texture *texture;
    void *immutable_sampler;
};

struct buffer_binding {
    const struct buffer *buffer;
    size_t offset;
    size_t size;
};

struct bindgroup_params {
    const struct bindgroup_layout *layout;
    struct texture_binding *textures;
    size_t nb_textures;
    struct buffer_binding *buffers;
    size_t nb_buffers;
};

struct bindgroup {
    struct gpu_ctx *gpu_ctx;
    const struct bindgroup_layout *layout;
};

struct bindgroup_layout *ngli_bindgroup_layout_create(struct gpu_ctx *gpu_ctx);
int ngli_bindgroup_layout_init(struct bindgroup_layout *s, const struct bindgroup_layout_params *params);
int ngli_bindgroup_layout_is_compatible(const struct bindgroup_layout *a, const struct bindgroup_layout *b);
void ngli_bindgroup_layout_freep(struct bindgroup_layout **sp);

struct bindgroup *ngli_bindgroup_create(struct gpu_ctx *gpu_ctx);
int ngli_bindgroup_init(struct bindgroup *s, const struct bindgroup_params *params);
int ngli_bindgroup_update_texture(struct bindgroup *s, int32_t index, const struct texture_binding *binding);
int ngli_bindgroup_update_buffer(struct bindgroup *s, int32_t index, const struct buffer_binding *binding);
void ngli_bindgroup_freep(struct bindgroup **sp);

#endif
