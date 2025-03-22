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

#ifndef PASS_H
#define PASS_H

#include <stdint.h>

#include "ngpu/pipeline.h"
#include "ngpu/pgcraft.h"
#include "utils/darray.h"

struct ngl_ctx;

struct pass_params {
    const char *label;
    const char *program_label;

    /* graphics */
    const char *vert_base;
    const char *frag_base;
    struct hmap *vert_resources;
    struct hmap *frag_resources;
    const struct hmap *properties;
    const struct geometry *geometry;
    uint32_t nb_instances;
    struct hmap *attributes;
    struct hmap *instance_attributes;
    struct ngpu_pgcraft_iovar *vert_out_vars;
    size_t nb_vert_out_vars;
    size_t nb_frag_output;
    int blending;

    /* compute */
    const char *comp_base;
    struct hmap *compute_resources;
    uint32_t workgroup_count[3];
    uint32_t workgroup_size[3];
};

struct pass {
    struct ngl_ctx *ctx;
    struct pass_params params;

    struct ngpu_buffer *indices;
    const struct buffer_layout *indices_layout;
    uint32_t nb_vertices;
    uint32_t nb_instances;
    enum ngpu_primitive_topology topology;
    enum ngpu_pipeline_type pipeline_type;
    struct darray crafter_attributes;
    struct darray crafter_uniforms;
    struct darray crafter_textures;
    struct darray crafter_blocks;
    struct ngpu_pgcraft *crafter;
    int32_t modelview_matrix_index;
    int32_t projection_matrix_index;
    int32_t normal_matrix_index;
    int32_t resolution_index;
    struct darray uniforms_map;
    struct darray pipeline_descs;
};

int ngli_pass_init(struct pass *s, struct ngl_ctx *ctx, const struct pass_params *params);
int ngli_pass_prepare(struct pass *s);
void ngli_pass_uninit(struct pass *s);
int ngli_pass_exec(struct pass *s);

#endif
