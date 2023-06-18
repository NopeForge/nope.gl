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

#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdlib.h>

#include "buffer.h"
#include "darray.h"
#include "graphicstate.h"
#include "program.h"
#include "rendertarget.h"
#include "texture.h"

struct gpu_ctx;

enum {
    NGLI_ACCESS_UNDEFINED,
    NGLI_ACCESS_READ_BIT,
    NGLI_ACCESS_WRITE_BIT,
    NGLI_ACCESS_READ_WRITE,
    NGLI_ACCESS_NB
};

NGLI_STATIC_ASSERT(texture_access, (NGLI_ACCESS_READ_BIT | NGLI_ACCESS_WRITE_BIT) == NGLI_ACCESS_READ_WRITE);

struct pipeline_texture_desc {
    char name[MAX_ID_LEN];
    int type;
    int binding;
    int access;
    int stage;
};

struct pipeline_buffer_desc {
    char name[MAX_ID_LEN];
    int type;
    int binding;
    int access;
    int stage;
};

struct pipeline_attribute_desc {
    char name[MAX_ID_LEN];
    int location;
    int format;
    int rate;
    size_t stride;
    size_t offset;
};

struct vertex_attribute {
    int location;
    int format;
    size_t offset;
};

#define NGLI_MAX_ATTRIBUTES_PER_BUFFER 16

struct vertex_buffer_layout {
    struct vertex_attribute attributes[NGLI_MAX_ATTRIBUTES_PER_BUFFER];
    size_t nb_attributes;
    int rate;
    size_t stride;
};

struct vertex_state {
    struct vertex_buffer_layout *buffers;
    size_t nb_buffers;
};

struct pipeline_graphics {
    int topology;
    struct graphicstate state;
    struct rendertarget_desc rt_desc;
    struct vertex_state vertex_state;
};

enum {
    NGLI_PIPELINE_TYPE_GRAPHICS,
    NGLI_PIPELINE_TYPE_COMPUTE,
};

struct pipeline_layout {
    const struct pipeline_texture_desc *texture_descs;
    size_t nb_texture_descs;
    const struct pipeline_buffer_desc *buffer_descs;
    size_t nb_buffer_descs;
};

struct pipeline_resources {
    struct texture **textures;
    size_t nb_textures;
    struct buffer **buffers;
    size_t nb_buffers;
    struct buffer **attributes;
    size_t nb_attributes;
};

struct pipeline_params {
    int type;
    const struct pipeline_graphics graphics;
    const struct program *program;
    struct pipeline_layout layout;
};

struct pipeline {
    struct gpu_ctx *gpu_ctx;

    int type;
    struct pipeline_graphics graphics;
    const struct program *program;
    struct pipeline_layout layout;
};

int ngli_pipeline_graphics_copy(struct pipeline_graphics *dst, const struct pipeline_graphics *src);
void ngli_pipeline_graphics_reset(struct pipeline_graphics *graphics);

int ngli_pipeline_layout_copy(struct pipeline_layout *dst, const struct pipeline_layout *src);
void ngli_pipeline_layout_reset(struct pipeline_layout *layout);

struct pipeline *ngli_pipeline_create(struct gpu_ctx *gpu_ctx);
int ngli_pipeline_init(struct pipeline *s, const struct pipeline_params *params);
int ngli_pipeline_set_resources(struct pipeline *s, const struct pipeline_resources *resources);
int ngli_pipeline_update_texture(struct pipeline *s, int32_t index, const struct texture *texture);
int ngli_pipeline_update_buffer(struct pipeline *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size);

void ngli_pipeline_freep(struct pipeline **sp);

#endif
