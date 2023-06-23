/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2021-2022 GoPro Inc.
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

#include <string.h>

#include "gpu_ctx_gl.h"
#include "pipeline.h"
#include "program_gl.h"
#include "program_gl_utils.h"
#include "type.h"

int ngli_program_gl_set_locations_and_bindings(struct program *s,
                                               const struct pgcraft *crafter)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct program_gl *s_priv = (struct program_gl *)s;

    const char *name = NULL;
    int need_relink = 0;
    const struct darray *info_array = ngli_pgcraft_get_attribute_infos(crafter);
    const struct pgcraft_attribute_info *infos = ngli_darray_data(info_array);
    for (size_t i = 0; i < ngli_darray_count(info_array); i++) {
        const struct pgcraft_attribute_info *attribute_info = &infos[i];
        if (name && !strcmp(name, attribute_info->name))
            continue;
        name = attribute_info->name;
        ngli_glBindAttribLocation(gl, s_priv->id, attribute_info->location, attribute_info->name);
        struct program_variable_info *info = ngli_hmap_get(s->attributes, attribute_info->name);
        if (info && info->location != attribute_info->location) {
            info->location = attribute_info->location;
            need_relink = 1;
        }
    }
    if (need_relink)
        ngli_glLinkProgram(gl, s_priv->id);

    const struct pipeline_layout layout = ngli_pgcraft_get_pipeline_layout(crafter);
    for (size_t i = 0; i < layout.nb_buffer_descs; i++) {
        const struct pipeline_buffer_desc *buffer_desc = &layout.buffer_descs[i];
        if (buffer_desc->type != NGLI_TYPE_UNIFORM_BUFFER)
            continue;
        const GLuint block_index = ngli_glGetUniformBlockIndex(gl, s_priv->id, buffer_desc->name);
        ngli_glUniformBlockBinding(gl, s_priv->id, block_index, buffer_desc->binding);
        struct program_variable_info *info = ngli_hmap_get(s->buffer_blocks, buffer_desc->name);
        if (info)
            info->binding = buffer_desc->binding;
    }

    struct glstate *glstate = &gpu_ctx_gl->glstate;
    ngli_glstate_use_program(gl, glstate, s_priv->id);
    for (size_t i = 0; i < layout.nb_texture_descs; i++) {
        const struct pipeline_texture_desc *texture_desc = &layout.texture_descs[i];
        const GLint location = ngli_glGetUniformLocation(gl, s_priv->id, texture_desc->name);
        ngli_glUniform1i(gl, location, texture_desc->binding);
        struct program_variable_info *info = ngli_hmap_get(s->uniforms, texture_desc->name);
        if (info)
            info->binding = texture_desc->binding;
    }

    return 0;
}
