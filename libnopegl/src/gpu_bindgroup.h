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

#ifndef GPU_BINDGROUP_H
#define GPU_BINDGROUP_H

#include <stdlib.h>

#include "gpu_buffer.h"
#include "darray.h"
#include "gpu_graphics_state.h"
#include "gpu_program.h"
#include "gpu_rendertarget.h"
#include "gpu_texture.h"
#include "type.h"
#include "utils.h"

struct gpu_ctx;

enum {
    NGLI_GPU_ACCESS_UNDEFINED,
    NGLI_GPU_ACCESS_READ_BIT,
    NGLI_GPU_ACCESS_WRITE_BIT,
    NGLI_GPU_ACCESS_READ_WRITE,
    NGLI_GPU_ACCESS_NB
};

NGLI_STATIC_ASSERT(access, (NGLI_GPU_ACCESS_READ_BIT | NGLI_GPU_ACCESS_WRITE_BIT) == NGLI_GPU_ACCESS_READ_WRITE);

struct gpu_bindgroup_layout_entry {
    size_t id;
    int type;
    int binding;
    int access;
    int stage;
    void *immutable_sampler;
};

struct gpu_bindgroup_layout_params {
    struct gpu_bindgroup_layout_entry *textures;
    size_t nb_textures;
    struct gpu_bindgroup_layout_entry *buffers;
    size_t nb_buffers;
};

struct gpu_bindgroup_layout {
    struct ngli_rc rc;
    struct gpu_ctx *gpu_ctx;
    struct gpu_bindgroup_layout_entry *textures;
    size_t nb_textures;
    struct gpu_bindgroup_layout_entry *buffers;
    size_t nb_buffers;
    size_t nb_dynamic_offsets;
};

NGLI_RC_CHECK_STRUCT(gpu_bindgroup_layout);

struct gpu_texture_binding {
    const struct gpu_texture *texture;
    void *immutable_sampler;
};

struct gpu_buffer_binding {
    const struct gpu_buffer *buffer;
    size_t offset;
    size_t size;
};

struct gpu_bindgroup_params {
    const struct gpu_bindgroup_layout *layout;
    struct gpu_texture_binding *textures;
    size_t nb_textures;
    struct gpu_buffer_binding *buffers;
    size_t nb_buffers;
};

struct gpu_bindgroup {
    struct ngli_rc rc;
    struct gpu_ctx *gpu_ctx;
    const struct gpu_bindgroup_layout *layout;
};

NGLI_RC_CHECK_STRUCT(gpu_bindgroup);

struct gpu_bindgroup_layout *ngli_gpu_bindgroup_layout_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_bindgroup_layout_init(struct gpu_bindgroup_layout *s, const struct gpu_bindgroup_layout_params *params);
int ngli_gpu_bindgroup_layout_is_compatible(const struct gpu_bindgroup_layout *a, const struct gpu_bindgroup_layout *b);
void ngli_gpu_bindgroup_layout_freep(struct gpu_bindgroup_layout **sp);

struct gpu_bindgroup *ngli_gpu_bindgroup_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_bindgroup_init(struct gpu_bindgroup *s, const struct gpu_bindgroup_params *params);
int ngli_gpu_bindgroup_update_texture(struct gpu_bindgroup *s, int32_t index, const struct gpu_texture_binding *binding);
int ngli_gpu_bindgroup_update_buffer(struct gpu_bindgroup *s, int32_t index, const struct gpu_buffer_binding *binding);
void ngli_gpu_bindgroup_freep(struct gpu_bindgroup **sp);

#endif
