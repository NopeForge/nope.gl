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

#include "ctx_gl.h"
#include "log.h"
#include "ngpu/type.h"
#include "ngpu/pgcraft.h"
#include "pipeline_compat.h"
#include "program_gl.h"
#include "program_gl_utils.h"

int ngpu_program_gl_set_locations_and_bindings(struct ngpu_program *s,
                                               const struct ngpu_pgcraft *crafter)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_program_gl *s_priv = (struct ngpu_program_gl *)s;

    const char *name = NULL;
    int need_relink = 0;
    const struct ngpu_vertex_state vertex_state = ngpu_pgcraft_get_vertex_state(crafter);
    for (size_t i = 0; i < vertex_state.nb_buffers; i++) {
        const struct ngpu_vertex_buffer_layout *layout = &vertex_state.buffers[i];
        for (size_t j = 0; j < layout->nb_attributes; j++) {
            const struct ngpu_vertex_attribute *attribute = &layout->attributes[j];
            const char *attribute_name = ngpu_pgcraft_get_symbol_name(crafter, attribute->id);
            if (name && !strcmp(name, attribute_name))
                continue;
            name = attribute_name;
            gl->funcs.BindAttribLocation(s_priv->id, attribute->location, attribute_name);
            struct ngpu_program_variable_info *info = ngli_hmap_get_str(s->attributes, attribute_name);
            if (info && info->location != attribute->location) {
                info->location = attribute->location;
                need_relink = 1;
            }
        }
    }
    if (need_relink)
        gl->funcs.LinkProgram(s_priv->id);

    const struct ngpu_bindgroup_layout_desc layout_desc = ngpu_pgcraft_get_bindgroup_layout_desc(crafter);
    for (size_t i = 0; i < layout_desc.nb_buffers; i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &layout_desc.buffers[i];
        if (entry->type != NGPU_TYPE_UNIFORM_BUFFER &&
            entry->type != NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC)
            continue;
        const char *buffer_name = ngpu_pgcraft_get_symbol_name(crafter, entry->id);
        char block_name[MAX_ID_LEN];
        int len = snprintf(block_name, sizeof(block_name), "%s_block", buffer_name);
        if (len >= sizeof(block_name)) {
            LOG(ERROR, "block name \"%s\" is too long", buffer_name);
            return NGL_ERROR_MEMORY;
        }
        const GLuint block_index = gl->funcs.GetUniformBlockIndex(s_priv->id, block_name);
        gl->funcs.UniformBlockBinding(s_priv->id, block_index, entry->binding);
        struct ngpu_program_variable_info *info = ngli_hmap_get_str(s->buffer_blocks, block_name);
        if (info)
            info->binding = entry->binding;
    }

    struct glstate *glstate = &gpu_ctx_gl->glstate;
    ngli_glstate_use_program(gl, glstate, s_priv->id);
    for (size_t i = 0; i < layout_desc.nb_textures; i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &layout_desc.textures[i];
        const char *texture_name = ngpu_pgcraft_get_symbol_name(crafter, entry->id);
        const GLint location = gl->funcs.GetUniformLocation(s_priv->id, texture_name);
        gl->funcs.Uniform1i(location, entry->binding);
        struct ngpu_program_variable_info *info = ngli_hmap_get_str(s->uniforms, texture_name);
        if (info)
            info->binding = entry->binding;
    }

    return 0;
}
