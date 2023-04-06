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
#include "darray.h"
#include "pgcraft.h"
#include "pipeline.h"

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
    int nb_instances;
    struct hmap *attributes;
    struct hmap *instance_attributes;
    struct pgcraft_iovar *vert_out_vars;
    int nb_vert_out_vars;
    int nb_frag_output;
    int blending;

    /* compute */
    const char *comp_base;
    struct hmap *compute_resources;
    int workgroup_count[3];
    int workgroup_size[3];
};

enum {
    NGLI_PASS_TYPE_GRAPHIC,
    NGLI_PASS_TYPE_COMPUTE,
};

struct pass {
    struct ngl_ctx *ctx;
    struct pass_params params;

    struct buffer *indices;
    const struct buffer_layout *indices_layout;
    int nb_vertices;
    int nb_instances;

    int pipeline_type;
    struct pipeline_graphics pipeline_graphics;
    struct darray crafter_attributes;
    struct darray crafter_uniforms;
    struct darray crafter_textures;
    struct darray crafter_blocks;
    struct darray pipeline_descs;
};

int ngli_pass_init(struct pass *s, struct ngl_ctx *ctx, const struct pass_params *params);
int ngli_pass_prepare(struct pass *s);
void ngli_pass_uninit(struct pass *s);
void ngli_pass_update_texture_uniforms(struct pipeline *pipeline, const struct pgcraft_texture_info *info);
int ngli_pass_exec(struct pass *s);

#endif
