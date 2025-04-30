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

#ifndef NGPU_BINDGROUP_H
#define NGPU_BINDGROUP_H

#include <stdlib.h>

#include "buffer.h"
#include "graphics_state.h"
#include "ngpu/type.h"
#include "program.h"
#include "rendertarget.h"
#include "utils/darray.h"
#include "texture.h"
#include "utils/utils.h"

struct ngpu_ctx;

enum ngpu_access {
    NGPU_ACCESS_UNDEFINED  = 0,
    NGPU_ACCESS_READ_BIT   = 1 << 0,
    NGPU_ACCESS_WRITE_BIT  = 1 << 1,
    NGPU_ACCESS_READ_WRITE = NGPU_ACCESS_READ_BIT | NGPU_ACCESS_WRITE_BIT,
    NGPU_ACCESS_NB,
    NGPU_ACCESS_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_bindgroup_layout_entry {
    size_t id;
    enum ngpu_type type;
    int binding;
    enum ngpu_access access;
    uint32_t stage_flags;
    void *immutable_sampler;
};

struct ngpu_bindgroup_layout_desc {
    struct ngpu_bindgroup_layout_entry *textures;
    size_t nb_textures;
    struct ngpu_bindgroup_layout_entry *buffers;
    size_t nb_buffers;
};

struct ngpu_bindgroup_layout {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    struct ngpu_bindgroup_layout_entry *textures;
    size_t nb_textures;
    struct ngpu_bindgroup_layout_entry *buffers;
    size_t nb_buffers;
    size_t nb_dynamic_offsets;
};

NGLI_RC_CHECK_STRUCT(ngpu_bindgroup_layout);

struct ngpu_texture_binding {
    const struct ngpu_texture *texture;
    void *immutable_sampler;
};

struct ngpu_buffer_binding {
    const struct ngpu_buffer *buffer;
    size_t offset;
    size_t size;
};

struct ngpu_bindgroup_resources {
    struct ngpu_texture_binding *textures;
    size_t nb_textures;
    struct ngpu_buffer_binding *buffers;
    size_t nb_buffers;
};

struct ngpu_bindgroup_params {
    struct ngpu_bindgroup_layout *layout;
    struct ngpu_bindgroup_resources resources;
};

struct ngpu_bindgroup {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    struct ngpu_bindgroup_layout *layout;
};

NGLI_RC_CHECK_STRUCT(ngpu_bindgroup);

struct ngpu_bindgroup_layout *ngpu_bindgroup_layout_create(struct ngpu_ctx *gpu_ctx);
int ngpu_bindgroup_layout_init(struct ngpu_bindgroup_layout *s, struct ngpu_bindgroup_layout_desc *desc);
int ngpu_bindgroup_layout_is_compatible(const struct ngpu_bindgroup_layout *a, const struct ngpu_bindgroup_layout *b);
void ngpu_bindgroup_layout_freep(struct ngpu_bindgroup_layout **sp);

struct ngpu_bindgroup *ngpu_bindgroup_create(struct ngpu_ctx *gpu_ctx);
int ngpu_bindgroup_init(struct ngpu_bindgroup *s, const struct ngpu_bindgroup_params *params);
int ngpu_bindgroup_update_texture(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_texture_binding *binding);
int ngpu_bindgroup_update_buffer(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_buffer_binding *binding);
void ngpu_bindgroup_freep(struct ngpu_bindgroup **sp);

#endif
