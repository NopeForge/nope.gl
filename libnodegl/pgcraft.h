/*
 * Copyright 2020 GoPro Inc.
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

#ifndef PGCRAFT_H
#define PGCRAFT_H

#include "block.h"
#include "bstr.h"
#include "buffer.h"
#include "image.h"
#include "pipeline.h"
#include "precision.h"
#include "texture.h"

struct ngl_ctx;

struct pgcraft_uniform { // also buffers (for arrays)
    char name[MAX_ID_LEN];
    int type;
    int stage;
    int count;
    int precision;
    const void *data;
};

enum pgcraft_shader_tex_type {
    NGLI_PGCRAFT_SHADER_TEX_TYPE_NONE,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_3D,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_NB
};

struct pgcraft_texture {
    char name[MAX_ID_LEN];
    enum pgcraft_shader_tex_type type;
    int stage;
    int precision;
    int writable;
    int format;
    int clamp_video;
    /*
     * Just like the other types (uniforms, blocks, attributes), this field
     * exists in order to be transmitted to the pipeline (through the
     * pipeline_resources destination). That way, these resources can be
     * associated with the pipeline straight after the pipeline initialization
     * (using ngli_pipeline_set_resources()). In the case of the texture
     * though, there is one exception: if the specified type is
     * NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO, then the texture field must be NULL.
     * Indeed, this type implies the potential use of multiple samplers (which
     * can be hardware accelerated and platform/backend specific) depending on
     * the image layout. This means that a single texture cannot be used as a
     * default resource for all these samplers. Moreover, the image layout
     * (which determines which samplers are used) is generally unknown at
     * pipeline initialization and it is only known once a frame has been
     * decoded/mapped. The image structure describes which layout to use and
     * which textures to bind and the pgcraft_texture_info.fields describes
     * where to bind the textures and their associated data.
     */
    struct texture *texture;
    /*
     * The image field is a bit special, it is not transmitted directly to the
     * pipeline but instead to the corresponding pgcraft_texture_info entry
     * accessible through pgcraft.texture_infos. The user may optionally set it
     * if they plan to have access to the image information directly through
     * the pgcraft_texture_info structure. The field is pretty much mandatory
     * if the user plans to use ngli_pipeline_compat_update_texture_info() in
     * conjunction with pgcraft.texture_infos to instruct a pipeline on which
     * texture resources to use.
     */
    struct image *image;
};

struct pgcraft_block {
    char name[MAX_ID_LEN];
    const char *instance_name;
    int type;
    int stage;
    int variadic;
    int writable;
    const struct block *block;
    struct buffer *buffer;
};

struct pgcraft_attribute {
    char name[MAX_ID_LEN];
    int type;
    int precision;
    int format;
    int stride;
    int offset;
    int rate;
    struct buffer *buffer;
};

struct pgcraft_iovar {
    char name[MAX_ID_LEN];
    int precision_out;
    int precision_in;
    int type;
};

enum {
    NGLI_INFO_FIELD_SAMPLING_MODE,
    NGLI_INFO_FIELD_COORDINATE_MATRIX,
    NGLI_INFO_FIELD_COLOR_MATRIX,
    NGLI_INFO_FIELD_DIMENSIONS,
    NGLI_INFO_FIELD_TIMESTAMP,
    NGLI_INFO_FIELD_SAMPLER_0,
    NGLI_INFO_FIELD_SAMPLER_1,
    NGLI_INFO_FIELD_SAMPLER_2,
    NGLI_INFO_FIELD_SAMPLER_OES,
    NGLI_INFO_FIELD_SAMPLER_RECT_0,
    NGLI_INFO_FIELD_SAMPLER_RECT_1,
    NGLI_INFO_FIELD_NB
};

struct pgcraft_texture_info_field {
    char name[MAX_ID_LEN];
    int type;
    int index;
    int stage;
};

struct pgcraft_texture_info {
    int stage;
    int precision;
    int writable;
    int format;
    struct texture *texture;
    struct image *image;
    struct pgcraft_texture_info_field fields[NGLI_INFO_FIELD_NB];
};

struct pgcraft_params {
    const char *vert_base;
    const char *frag_base;
    const char *comp_base;

    const struct pgcraft_uniform *uniforms;
    int nb_uniforms;
    const struct pgcraft_texture *textures;
    int nb_textures;
    const struct pgcraft_block *blocks;
    int nb_blocks;
    const struct pgcraft_attribute *attributes;
    int nb_attributes;

    const struct pgcraft_iovar *vert_out_vars;
    int nb_vert_out_vars;

    int nb_frag_output;

    int workgroup_size[3];
};

struct pgcraft;

struct pgcraft *ngli_pgcraft_create(struct ngl_ctx *ctx);
int ngli_pgcraft_craft(struct pgcraft *s, const struct pgcraft_params *params);
int ngli_pgcraft_get_uniform_index(const struct pgcraft *s, const char *name, int stage);
const struct darray *ngli_pgcraft_get_texture_infos(const struct pgcraft *s);
struct program *ngli_pgcraft_get_program(const struct pgcraft *s);
struct pipeline_layout ngli_pgcraft_get_pipeline_layout(const struct pgcraft *s);
struct pipeline_resources ngli_pgcraft_get_pipeline_resources(const struct pgcraft *s);
void ngli_pgcraft_freep(struct pgcraft **sp);

#endif
