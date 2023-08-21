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

#ifndef BINDGROUP_GL_H
#define BINDGROUP_GL_H

#include <stdlib.h>

#include "bindgroup.h"
#include "glincludes.h"

struct gpu_ctx;

struct bindgroup_layout_gl {
    struct bindgroup_layout parent;
};

struct bindgroup_gl {
    struct bindgroup parent;
    struct darray texture_bindings;   // texture_binding_gl
    struct darray buffer_bindings;    // buffer_binding_gl
    int use_barriers;
};

struct bindgroup_layout *ngli_bindgroup_layout_gl_create(struct gpu_ctx *gpu_ctx);
int ngli_bindgroup_layout_gl_init(struct bindgroup_layout *s);
void ngli_bindgroup_layout_gl_freep(struct bindgroup_layout **sp);

struct bindgroup *ngli_bindgroup_gl_create(struct gpu_ctx *gpu_ctx);
int ngli_bindgroup_gl_init(struct bindgroup *s, const struct bindgroup_params *params);
int ngli_bindgroup_gl_update_texture(struct bindgroup *s, int32_t index, const struct texture_binding *binding);
int ngli_bindgroup_gl_update_buffer(struct bindgroup *s, int32_t index, const struct buffer_binding *binding);
GLbitfield ngli_bindgroup_gl_get_memory_barriers(struct bindgroup *s);
void ngli_bindgroup_gl_bind(struct bindgroup *s);
void ngli_bindgroup_gl_freep(struct bindgroup **sp);

#endif
