/*
 * Copyright 2019 GoPro Inc.
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
#include "glcontext.h"
#include "glincludes.h"
#include "program.h"

typedef void (*nodeprograminfopair_handle_func)(struct glcontext *gl, GLint loc, void *priv);

struct nodeprograminfopair {
    char name[MAX_ID_LEN];
    struct ngl_node *node;
    void *program_info;
    nodeprograminfopair_handle_func handle;
};

struct pass_params {
    const char *label;
    struct ngl_node *program;
    struct hmap *textures;
    struct hmap *uniforms;
    struct hmap *blocks;

    /* graphics */
    struct ngl_node *geometry;
    int nb_instances;
    struct hmap *attributes;
    struct hmap *instance_attributes;

    /* compute */
    int nb_group_x;
    int nb_group_y;
    int nb_group_z;
};

enum {
    NGLI_PASS_TYPE_GRAPHIC,
    NGLI_PASS_TYPE_COMPUTE,
};

struct textureprograminfo {
    int sampling_mode_location;
    int sampler_value;
    int sampler_type;
    int sampler_location;
    int external_sampler_location;
    int y_sampler_location;
    int uv_sampler_location;
    int coord_matrix_location;
    int dimensions_location;
    int dimensions_type;
    int ts_location;
};

struct pass {
    struct ngl_ctx *ctx;
    struct glcontext *gl;
    struct pass_params params;
    int type;

    struct textureprograminfo *textureprograminfos;
    int nb_textureprograminfos;
    struct darray texture_pairs; // nodeprograminfopair (texture, textureprograminfo)

    uint64_t used_texture_units;
    int disabled_texture_unit[2]; /* 2D, OES */

    struct darray uniform_pairs; // nodeprograminfopair (uniform, uniformprograminfo)
    struct darray block_pairs; // nodeprograminfopair (block, uniformprograminfo)

    void (*exec)(struct glcontext *gl, const struct pass *s);

    /* graphics */

    struct darray attribute_pairs; // nodeprograminfopair (attribute, attributeprograminfo)
    struct darray instance_attribute_pairs; // nodeprograminfopair (instance attribute, attributeprograminfo)

    GLint modelview_matrix_location;
    GLint projection_matrix_location;
    GLint normal_matrix_location;

    GLuint vao_id;

    struct hmap *pass_attributes;
    int has_indices_buffer_ref;
    GLenum indices_type;
};

int ngli_pass_init(struct pass *s, struct ngl_ctx *ctx, const struct pass_params *params);
void ngli_pass_uninit(struct pass *s);
int ngli_pass_update(struct pass *s, double t);
int ngli_pass_exec(struct pass *s);

#endif
