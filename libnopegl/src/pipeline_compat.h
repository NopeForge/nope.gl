/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2022 GoPro Inc.
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

#ifndef PIPELINE_COMPAT_H
#define PIPELINE_COMPAT_H

#include "gpu_bindgroup.h"
#include "pgcraft.h"
#include "gpu_pipeline.h"

struct pipeline_compat_resources {
    struct gpu_texture_binding *textures;
    size_t nb_textures;
    struct gpu_buffer_binding *buffers;
    size_t nb_buffers;
    struct gpu_buffer **vertex_buffers;
    size_t nb_vertex_buffers;
};

struct pipeline_compat_params {
    int type; // NGLI_PIPELINE_TYPE_*
    struct gpu_pipeline_graphics graphics;
    const struct gpu_program *program;
    struct gpu_bindgroup_layout_desc layout_desc;
    struct pipeline_compat_resources resources;
    const struct pgcraft_compat_info *compat_info;
};

struct pipeline_compat;

struct pipeline_compat *ngli_pipeline_compat_create(struct gpu_ctx *gpu_ctx);
int ngli_pipeline_compat_init(struct pipeline_compat *s, const struct pipeline_compat_params *params);
int ngli_pipeline_compat_update_vertex_buffer(struct pipeline_compat *s, int32_t index, const struct gpu_buffer *buffer);
int ngli_pipeline_compat_update_uniform(struct pipeline_compat *s, int32_t index, const void *value);
int ngli_pipeline_compat_update_uniform_count(struct pipeline_compat *s, int32_t index, const void *value, size_t count);
int ngli_pipeline_compat_update_texture(struct pipeline_compat *s, int32_t index, const struct gpu_texture *texture);
void ngli_pipeline_compat_apply_reframing_matrix(struct pipeline_compat *s, int32_t index, const struct image *image, const float *reframing);
void ngli_pipeline_compat_update_image(struct pipeline_compat *s, int32_t index, const struct image *image);
int ngli_pipeline_compat_update_buffer(struct pipeline_compat *s, int32_t index, const struct gpu_buffer *buffer, size_t offset, size_t size);
int ngli_pipeline_compat_update_dynamic_offsets(struct pipeline_compat *s, const uint32_t *offsets, size_t nb_offsets);
void ngli_pipeline_compat_draw(struct pipeline_compat *s, int nb_vertices, int nb_instances, int first_vertex);
void ngli_pipeline_compat_draw_indexed(struct pipeline_compat *s, const struct gpu_buffer *indices, int indices_format, int nb_indices, int nb_instances);
void ngli_pipeline_compat_dispatch(struct pipeline_compat *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);
void ngli_pipeline_compat_freep(struct pipeline_compat **sp);

#endif
