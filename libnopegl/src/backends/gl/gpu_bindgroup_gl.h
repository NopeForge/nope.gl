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

#ifndef GPU_BINDGROUP_GL_H
#define GPU_BINDGROUP_GL_H

#include <stdlib.h>

#include "gpu_bindgroup.h"
#include "glincludes.h"

struct gpu_ctx;

struct gpu_bindgroup_layout_gl {
    struct gpu_bindgroup_layout parent;
};

struct gpu_bindgroup_gl {
    struct gpu_bindgroup parent;
    struct darray texture_bindings;   // texture_binding_gl
    struct darray buffer_bindings;    // buffer_binding_gl
    int use_barriers;
};

struct gpu_bindgroup_layout *ngli_gpu_bindgroup_layout_gl_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_bindgroup_layout_gl_init(struct gpu_bindgroup_layout *s);
void ngli_gpu_bindgroup_layout_gl_freep(struct gpu_bindgroup_layout **sp);

struct gpu_bindgroup *ngli_gpu_bindgroup_gl_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_bindgroup_gl_init(struct gpu_bindgroup *s, const struct gpu_bindgroup_params *params);
int ngli_gpu_bindgroup_gl_update_texture(struct gpu_bindgroup *s, int32_t index, const struct gpu_texture_binding *binding);
int ngli_gpu_bindgroup_gl_update_buffer(struct gpu_bindgroup *s, int32_t index, const struct gpu_buffer_binding *binding);
GLbitfield ngli_gpu_bindgroup_gl_get_memory_barriers(struct gpu_bindgroup *s);
void ngli_gpu_bindgroup_gl_bind(struct gpu_bindgroup *s);
void ngli_gpu_bindgroup_gl_freep(struct gpu_bindgroup **sp);

#endif
