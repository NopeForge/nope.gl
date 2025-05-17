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

#include "bindgroup.h"
#include "buffer.h"
#include "format.h"
#include "graphics_state.h"
#include "limits.h"
#include "program.h"
#include "rendertarget.h"
#include "utils/utils.h"
#include "texture.h"
#include "utils/refcount.h"

struct ngpu_ctx;

struct ngpu_vertex_attribute {
    size_t id;
    uint32_t location;
    enum ngpu_format format;
    size_t offset;
};

struct ngpu_vertex_buffer_layout {
    struct ngpu_vertex_attribute attributes[NGPU_MAX_ATTRIBUTES_PER_BUFFER];
    size_t nb_attributes;
    uint32_t rate;
    size_t stride;
};

struct ngpu_vertex_state {
    struct ngpu_vertex_buffer_layout *buffers;
    size_t nb_buffers;
};

struct ngpu_vertex_resources {
    struct ngpu_buffer **vertex_buffers;
    size_t nb_vertex_buffers;
};

enum ngpu_primitive_topology {
    NGPU_PRIMITIVE_TOPOLOGY_POINT_LIST,
    NGPU_PRIMITIVE_TOPOLOGY_LINE_LIST,
    NGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    NGPU_PRIMITIVE_TOPOLOGY_NB,
    NGPU_PRIMITIVE_TOPOLOGY_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_pipeline_graphics {
    enum ngpu_primitive_topology topology;
    struct ngpu_graphics_state state;
    struct ngpu_rendertarget_layout rt_layout;
    struct ngpu_vertex_state vertex_state;
};

enum ngpu_pipeline_type {
    NGPU_PIPELINE_TYPE_GRAPHICS,
    NGPU_PIPELINE_TYPE_COMPUTE,
    NGPU_PIPELINE_TYPE_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_pipeline_layout {
    const struct ngpu_bindgroup_layout *bindgroup_layout;
};

struct ngpu_pipeline_params {
    enum ngpu_pipeline_type type;
    const struct ngpu_pipeline_graphics graphics;
    const struct ngpu_program *program;
    const struct ngpu_pipeline_layout layout;
};

struct ngpu_pipeline {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;

    enum ngpu_pipeline_type type;
    struct ngpu_pipeline_graphics graphics;
    const struct ngpu_program *program;
    struct ngpu_pipeline_layout layout;
};

NGLI_RC_CHECK_STRUCT(ngpu_pipeline);

int ngpu_pipeline_graphics_copy(struct ngpu_pipeline_graphics *dst, const struct ngpu_pipeline_graphics *src);
void ngpu_pipeline_graphics_reset(struct ngpu_pipeline_graphics *graphics);

struct ngpu_pipeline *ngpu_pipeline_create(struct ngpu_ctx *gpu_ctx);
int ngpu_pipeline_init(struct ngpu_pipeline *s, const struct ngpu_pipeline_params *params);
void ngpu_pipeline_freep(struct ngpu_pipeline **sp);

#endif
