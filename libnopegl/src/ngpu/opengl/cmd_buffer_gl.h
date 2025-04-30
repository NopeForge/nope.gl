/*
 * Copyright 2024-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_CMD_BUFFER_GL_H
#define NGPU_CMD_BUFFER_GL_H

#include "ngpu/ctx.h"
#include "ngpu/limits.h"

struct ngpu_rendertarget;
struct ngpu_bindgroup;
struct ngpu_pipeline;
struct ngpu_buffer;

enum ngpu_cmd_type_gl {
    NGPU_CMD_TYPE_GL_BEGIN_RENDER_PASS,
    NGPU_CMD_TYPE_GL_END_RENDER_PASS,
    NGPU_CMD_TYPE_GL_SET_BINDGROUP,
    NGPU_CMD_TYPE_GL_SET_PIPELINE,
    NGPU_CMD_TYPE_GL_SET_VIEWPORT,
    NGPU_CMD_TYPE_GL_SET_SCISSOR,
    NGPU_CMD_TYPE_GL_DRAW,
    NGPU_CMD_TYPE_GL_DRAW_INDEXED,
    NGPU_CMD_TYPE_GL_DISPATCH,
    NGPU_CMD_TYPE_GL_SET_VERTEX_BUFFER,
    NGPU_CMD_TYPE_GL_SET_INDEX_BUFFER,
    NGPU_CMD_TYPE_GL_GENERATE_TEXTURE_MIPMAP,
    NGPU_CMD_TYPE_GL_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_cmd_gl {
    enum ngpu_cmd_type_gl type;
    union {
        struct {
            struct ngpu_rendertarget *rendertarget;
        } begin_render_pass;

        struct {
            struct ngpu_pipeline *pipeline;
        } set_pipeline;

        struct {
            struct ngpu_bindgroup *bindgroup;
            uint32_t offsets[NGPU_MAX_DYNAMIC_OFFSETS];
            size_t nb_offsets;
        } set_bindgroup;

        struct {
            struct ngpu_viewport viewport;
        } set_viewport;

        struct {
            struct ngpu_scissor scissor;
        } set_scissor;

        struct {
            uint32_t nb_vertices;
            uint32_t nb_instances;
            uint32_t first_vertex;
        } draw;

        struct {
            uint32_t nb_indices;
            uint32_t nb_instances;
        } draw_indexed;

        struct {
            uint32_t nb_group_x;
            uint32_t nb_group_y;
            uint32_t nb_group_z;
        } dispatch;

        struct {
            uint32_t index;
            const struct ngpu_buffer *buffer;
        } set_vertex_buffer;

        struct {
            const struct ngpu_buffer *buffer;
            enum ngpu_format format;
        } set_index_buffer;

        struct {
            struct ngpu_texture *texture;
        } generate_texture_mipmap;
    };
};

struct ngpu_cmd_buffer_gl;

struct ngpu_cmd_buffer_gl *ngpu_cmd_buffer_gl_create(struct ngpu_ctx *gpu_ctx);
void ngpu_cmd_buffer_gl_freep(struct ngpu_cmd_buffer_gl **sp);
int ngpu_cmd_buffer_gl_init(struct ngpu_cmd_buffer_gl *s);

#define NGLI_CMD_BUFFER_GL_CMD_REF(cmd, rc) ngpu_cmd_buffer_gl_ref((cmd), (struct ngli_rc *)(rc))
int ngpu_cmd_buffer_gl_ref(struct ngpu_cmd_buffer_gl *s, struct ngli_rc *rc);
int ngpu_cmd_buffer_gl_ref_buffer(struct ngpu_cmd_buffer_gl *s, struct ngpu_buffer *buffer);

int ngpu_cmd_buffer_gl_begin(struct ngpu_cmd_buffer_gl *s);
int ngpu_cmd_buffer_gl_push(struct ngpu_cmd_buffer_gl *s, const struct ngpu_cmd_gl *cmd);
int ngpu_cmd_buffer_gl_submit(struct ngpu_cmd_buffer_gl *s);
int ngpu_cmd_buffer_gl_wait(struct ngpu_cmd_buffer_gl *s);

#endif
