/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2020-2022 GoPro Inc.
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

#ifndef NGPU_PGCRAFT_H
#define NGPU_PGCRAFT_H

#include "image.h"
#include "ngpu/buffer.h"
#include "ngpu/pipeline.h"
#include "ngpu/texture.h"
#include "precision.h"
#include "ngpu/block_desc.h"
#include "utils/bstr.h"

struct ngpu_ctx;

struct ngpu_pgcraft_uniform { // also buffers (for arrays)
    char name[MAX_ID_LEN];
    enum ngpu_type type;
    int stage;
    int precision;
    const void *data;
    size_t count;
};

enum ngpu_pgcraft_shader_tex_type {
    NGPU_PGCRAFT_SHADER_TEX_TYPE_NONE,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_VIDEO,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_2D,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_2D_ARRAY,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D_ARRAY,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_3D,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_IMAGE_3D,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_CUBE,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_IMAGE_CUBE,
    NGPU_PGCRAFT_SHADER_TEX_TYPE_NB
};

struct ngpu_pgcraft_texture {
    char name[MAX_ID_LEN];
    enum ngpu_pgcraft_shader_tex_type type;
    int stage;
    enum ngpu_precision precision;
    int writable;
    enum ngpu_format format;
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
    struct ngpu_texture *texture;
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

struct ngpu_pgcraft_block {
    char name[MAX_ID_LEN];
    const char *instance_name;
    enum ngpu_type type;
    int stage;
    int writable;
    const struct ngpu_block_desc *block;
    struct ngpu_buffer_binding buffer;
};

struct ngpu_pgcraft_attribute {
    char name[MAX_ID_LEN];
    enum ngpu_type type;
    enum ngpu_precision precision;
    enum ngpu_format format;
    size_t stride;
    size_t offset;
    int rate;
    struct ngpu_buffer *buffer;
};

struct ngpu_pgcraft_iovar {
    char name[MAX_ID_LEN];
    enum ngpu_precision precision_out;
    enum ngpu_precision precision_in;
    enum ngpu_type type;
};

enum {
    NGPU_INFO_FIELD_SAMPLING_MODE,
    NGPU_INFO_FIELD_COORDINATE_MATRIX,
    NGPU_INFO_FIELD_COLOR_MATRIX,
    NGPU_INFO_FIELD_DIMENSIONS,
    NGPU_INFO_FIELD_TIMESTAMP,
    NGPU_INFO_FIELD_SAMPLER_0,
    NGPU_INFO_FIELD_SAMPLER_1,
    NGPU_INFO_FIELD_SAMPLER_2,
    NGPU_INFO_FIELD_SAMPLER_OES,
    NGPU_INFO_FIELD_SAMPLER_RECT_0,
    NGPU_INFO_FIELD_SAMPLER_RECT_1,
    NGPU_INFO_FIELD_NB
};

struct ngpu_pgcraft_texture_info_field {
    enum ngpu_type type;
    int32_t index;
    int stage;
};

struct ngpu_pgcraft_texture_info {
    size_t id;
    struct ngpu_pgcraft_texture_info_field fields[NGPU_INFO_FIELD_NB];
};

/*
 * Oldest OpenGL flavours exclusively support single uniforms (no concept of
 * blocks). And while OpenGL added support for blocks in addition to uniforms,
 * modern backends such as Vulkan do not support the concept of single uniforms
 * at all. This means there is no cross-API common ground.
 *
 * Since NopeGL still exposes some form of cross-backend uniform abstraction to
 * the user (through the Uniform* nodes), we have to make a compatibility
 * layer, which we name "ublock" (for uniform-block). This compatibility layer
 * maps single uniforms to dedicated uniform blocks.
 */
struct ngpu_pgcraft_compat_info {
    struct ngpu_block_desc ublocks[NGPU_PROGRAM_SHADER_NB];
    int32_t ubindings[NGPU_PROGRAM_SHADER_NB];
    int32_t uindices[NGPU_PROGRAM_SHADER_NB];

    const struct ngpu_pgcraft_texture_info *texture_infos;
    const struct image **images;
    size_t nb_texture_infos;
};

struct ngpu_pgcraft_params {
    const char *program_label;
    const char *vert_base;
    const char *frag_base;
    const char *comp_base;

    const struct ngpu_pgcraft_uniform *uniforms;
    size_t nb_uniforms;
    const struct ngpu_pgcraft_texture *textures;
    size_t nb_textures;
    const struct ngpu_pgcraft_block *blocks;
    size_t nb_blocks;
    const struct ngpu_pgcraft_attribute *attributes;
    size_t nb_attributes;

    const struct ngpu_pgcraft_iovar *vert_out_vars;
    size_t nb_vert_out_vars;

    size_t nb_frag_output;

    uint32_t workgroup_size[3];
};

struct ngpu_pgcraft;

struct ngpu_pgcraft *ngpu_pgcraft_create(struct ngpu_ctx *gpu_ctx);
int ngpu_pgcraft_craft(struct ngpu_pgcraft *s, const struct ngpu_pgcraft_params *params);
int32_t ngpu_pgcraft_get_uniform_index(const struct ngpu_pgcraft *s, const char *name, int stage);
int32_t ngpu_pgcraft_get_block_index(const struct ngpu_pgcraft *s, const char *name, int stage);
int32_t ngpu_pgcraft_get_image_index(const struct ngpu_pgcraft *s, const char *name);
const struct ngpu_pgcraft_compat_info *ngpu_pgcraft_get_compat_info(const struct ngpu_pgcraft *s);
const char *ngpu_pgcraft_get_symbol_name(const struct ngpu_pgcraft *s, size_t id);
struct ngpu_vertex_state ngpu_pgcraft_get_vertex_state(const struct ngpu_pgcraft *s);
struct ngpu_vertex_resources ngpu_pgcraft_get_vertex_resources(const struct ngpu_pgcraft *s);
int32_t ngpu_pgcraft_get_vertex_buffer_index(const struct ngpu_pgcraft *s, const char *name);
struct ngpu_program *ngpu_pgcraft_get_program(const struct ngpu_pgcraft *s);
struct ngpu_bindgroup_layout_desc ngpu_pgcraft_get_bindgroup_layout_desc(const struct ngpu_pgcraft *s);
struct ngpu_bindgroup_resources ngpu_pgcraft_get_bindgroup_resources(const struct ngpu_pgcraft *s);
void ngpu_pgcraft_freep(struct ngpu_pgcraft **sp);

#endif
