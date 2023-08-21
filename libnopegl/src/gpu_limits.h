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

#ifndef GPU_LIMITS_H
#define GPU_LIMITS_H

#include <stdint.h>
#include <stdlib.h>

#define NGLI_MAX_UNIFORM_BUFFERS_DYNAMIC 8
#define NGLI_MAX_STORAGE_BUFFERS_DYNAMIC 4
#define NGLI_MAX_DYNAMIC_OFFSETS (NGLI_MAX_UNIFORM_BUFFERS_DYNAMIC + NGLI_MAX_STORAGE_BUFFERS_DYNAMIC)

#define NGLI_MAX_COLOR_ATTACHMENTS 8

struct gpu_limits {
    uint32_t max_vertex_attributes;
    uint32_t max_texture_image_units;
    uint32_t max_image_units;
    uint32_t max_compute_work_group_count[3];
    uint32_t max_compute_work_group_invocations;
    uint32_t max_compute_work_group_size[3];
    uint32_t max_compute_shared_memory_size;
    uint32_t max_uniform_block_size;
    size_t min_uniform_block_offset_alignment;
    uint32_t max_storage_block_size;
    size_t min_storage_block_offset_alignment;
    uint32_t max_samples;
    uint32_t max_texture_dimension_1d;
    uint32_t max_texture_dimension_2d;
    uint32_t max_texture_dimension_3d;
    uint32_t max_texture_dimension_cube;
    uint32_t max_texture_array_layers;
    uint32_t max_color_attachments;
    uint32_t max_draw_buffers;
};

#endif
