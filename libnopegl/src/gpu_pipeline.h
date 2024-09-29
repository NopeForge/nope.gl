/*
 * Copyright 2019-2022 GoPro Inc.
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

#ifndef GPU_PIPELINE_H
#define GPU_PIPELINE_H

#include <stdlib.h>

#include "gpu_bindgroup.h"
#include "gpu_buffer.h"
#include "gpu_limits.h"
#include "gpu_graphics_state.h"
#include "gpu_program.h"
#include "gpu_rendertarget.h"
#include "gpu_texture.h"
#include "utils.h"

struct gpu_ctx;

struct gpu_vertex_attribute {
    size_t id;
    int location;
    int format;
    size_t offset;
};

struct gpu_vertex_buffer_layout {
    struct gpu_vertex_attribute attributes[NGLI_GPU_MAX_ATTRIBUTES_PER_BUFFER];
    size_t nb_attributes;
    int rate;
    size_t stride;
};

struct gpu_vertex_state {
    struct gpu_vertex_buffer_layout *buffers;
    size_t nb_buffers;
};

struct gpu_pipeline_graphics {
    int topology;
    struct gpu_graphics_state state;
    struct gpu_rendertarget_layout rt_layout;
    struct gpu_vertex_state vertex_state;
};

enum {
    NGLI_GPU_PIPELINE_TYPE_GRAPHICS,
    NGLI_GPU_PIPELINE_TYPE_COMPUTE,
};

struct gpu_pipeline_layout {
    const struct gpu_bindgroup_layout *bindgroup_layout;
};

struct gpu_pipeline_params {
    int type;
    const struct gpu_pipeline_graphics graphics;
    const struct gpu_program *program;
    const struct gpu_pipeline_layout layout;
};

struct gpu_pipeline {
    struct ngli_rc rc;
    struct gpu_ctx *gpu_ctx;

    int type;
    struct gpu_pipeline_graphics graphics;
    const struct gpu_program *program;
    struct gpu_pipeline_layout layout;
};

NGLI_RC_CHECK_STRUCT(gpu_pipeline);

int ngli_gpu_pipeline_graphics_copy(struct gpu_pipeline_graphics *dst, const struct gpu_pipeline_graphics *src);
void ngli_gpu_pipeline_graphics_reset(struct gpu_pipeline_graphics *graphics);

struct gpu_pipeline *ngli_gpu_pipeline_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_pipeline_init(struct gpu_pipeline *s, const struct gpu_pipeline_params *params);
void ngli_gpu_pipeline_freep(struct gpu_pipeline **sp);

#endif
