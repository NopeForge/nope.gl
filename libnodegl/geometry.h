/*
 * Copyright 2021 GoPro Inc.
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

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "internal.h"

struct geometry {
    struct buffer *vertices_buffer;
    struct buffer *uvcoords_buffer;
    struct buffer *normals_buffer;
    struct buffer *indices_buffer;

    struct buffer_layout vertices_layout;
    struct buffer_layout uvcoords_layout;
    struct buffer_layout normals_layout;
    struct buffer_layout indices_layout;

    int topology;

    int64_t max_indices;
};

struct gpu_ctx;

int ngli_geometry_gen_vec3(struct buffer **bufferp, struct buffer_layout *layout,
                           struct gpu_ctx *gpu_ctx, int count, const void *data);
int ngli_geometry_gen_vec2(struct buffer **bufferp, struct buffer_layout *layout,
                           struct gpu_ctx *gpu_ctx, int count, const void *data);
int ngli_geometry_gen_indices(struct buffer **bufferp, struct buffer_layout *layout,
                              struct gpu_ctx *gpu_ctx, int count, const void *data);
#endif
