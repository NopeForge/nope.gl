/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_BINDGROUP_GL_H
#define NGPU_BINDGROUP_GL_H

#include <stdlib.h>

#include "glincludes.h"
#include "ngpu/bindgroup.h"

struct ngpu_ctx;

struct texture_binding_gl {
    struct ngpu_bindgroup_layout_entry layout_entry;
    const struct ngpu_texture *texture;
};

struct buffer_binding_gl {
    struct ngpu_bindgroup_layout_entry layout_entry;
    const struct ngpu_buffer *buffer;
    size_t offset;
    size_t size;
};

struct ngpu_bindgroup_layout_gl {
    struct ngpu_bindgroup_layout parent;
};

struct ngpu_bindgroup_gl {
    struct ngpu_bindgroup parent;
    struct darray texture_bindings;   // texture_binding_gl
    struct darray buffer_bindings;    // buffer_binding_gl
    int use_barriers;
};

struct ngpu_bindgroup_layout *ngpu_bindgroup_layout_gl_create(struct ngpu_ctx *gpu_ctx);
int ngpu_bindgroup_layout_gl_init(struct ngpu_bindgroup_layout *s);
void ngpu_bindgroup_layout_gl_freep(struct ngpu_bindgroup_layout **sp);

struct ngpu_bindgroup *ngpu_bindgroup_gl_create(struct ngpu_ctx *gpu_ctx);
int ngpu_bindgroup_gl_init(struct ngpu_bindgroup *s, const struct ngpu_bindgroup_params *params);
int ngpu_bindgroup_gl_update_texture(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_texture_binding *binding);
int ngpu_bindgroup_gl_update_buffer(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_buffer_binding *binding);
GLbitfield ngpu_bindgroup_gl_get_memory_barriers(struct ngpu_bindgroup *s);
void ngpu_bindgroup_gl_bind(struct ngpu_bindgroup *s, const uint32_t *dynamic_offsets, size_t nb_dynamic_offsets);
void ngpu_bindgroup_gl_freep(struct ngpu_bindgroup **sp);

#endif
