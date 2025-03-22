/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <string.h>

#include "bindgroup_gl.h"
#include "buffer_gl.h"
#include "ctx_gl.h"
#include "glcontext.h"
#include "log.h"
#include "ngpu/format.h"
#include "pipeline_gl.h"
#include "program_gl.h"
#include "utils/memory.h"

struct attribute_binding_gl {
    size_t binding;
    int location;
    enum ngpu_format format;
    size_t stride;
    size_t offset;
};

static int build_attribute_bindings(struct ngpu_pipeline *s)
{
    struct ngpu_pipeline_gl *s_priv = (struct ngpu_pipeline_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    gl->funcs.GenVertexArrays(1, &s_priv->vao_id);
    gl->funcs.BindVertexArray(s_priv->vao_id);

    const struct ngpu_pipeline_graphics *graphics = &s->graphics;
    const struct ngpu_vertex_state *state = &graphics->vertex_state;
    for (size_t i = 0; i < state->nb_buffers; i++) {
        const struct ngpu_vertex_buffer_layout *buffer = &state->buffers[i];
        for (size_t j = 0; j < buffer->nb_attributes; j++) {
            const struct ngpu_vertex_attribute *attribute = &buffer->attributes[j];

            struct attribute_binding_gl binding = {
                .binding  = i,
                .location = attribute->location,
                .format   = attribute->format,
                .stride   = buffer->stride,
                .offset   = attribute->offset,
            };
            if (!ngli_darray_push(&s_priv->attribute_bindings, &binding))
                return NGL_ERROR_MEMORY;

            const GLuint location = attribute->location;
            const GLuint rate = buffer->rate;
            gl->funcs.EnableVertexAttribArray(location);
            if (rate > 0)
                gl->funcs.VertexAttribDivisor(location, rate);
        }
    }

    return 0;
}

static const GLenum gl_primitive_topology_map[NGPU_PRIMITIVE_TOPOLOGY_NB] = {
    [NGPU_PRIMITIVE_TOPOLOGY_POINT_LIST]     = GL_POINTS,
    [NGPU_PRIMITIVE_TOPOLOGY_LINE_LIST]      = GL_LINES,
    [NGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP]     = GL_LINE_STRIP,
    [NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST]  = GL_TRIANGLES,
    [NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = GL_TRIANGLE_STRIP,
};

static GLenum get_gl_topology(enum ngpu_primitive_topology topology)
{
    return gl_primitive_topology_map[topology];
}

static const GLenum gl_indices_type_map[NGPU_FORMAT_NB] = {
    [NGPU_FORMAT_R16_UNORM] = GL_UNSIGNED_SHORT,
    [NGPU_FORMAT_R32_UINT]  = GL_UNSIGNED_INT,
};

static GLenum get_gl_indices_type(enum ngpu_format indices_format)
{
    return gl_indices_type_map[indices_format];
}

static void bind_vertex_attribs(const struct ngpu_pipeline *s)
{
    const struct ngpu_pipeline_gl *s_priv = (const struct ngpu_pipeline_gl *)s;
    struct ngpu_ctx *gpu_ctx = (struct ngpu_ctx *)s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    gl->funcs.BindVertexArray(s_priv->vao_id);

    const struct ngpu_buffer **vertex_buffers = gpu_ctx->vertex_buffers;
    const struct attribute_binding_gl *bindings = ngli_darray_data(&s_priv->attribute_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->attribute_bindings); i++) {
        const struct attribute_binding_gl *attribute_binding = &bindings[i];
        const size_t binding = attribute_binding->binding;
        const GLuint location = attribute_binding->location;
        const GLuint size = ngpu_format_get_nb_comp(attribute_binding->format);
        const GLsizei stride = (GLsizei)attribute_binding->stride;
        const void *offset = (void *)(uintptr_t)attribute_binding->offset;
        const struct ngpu_buffer_gl *buffer_gl = (const struct ngpu_buffer_gl *)vertex_buffers[binding];
        gl->funcs.BindBuffer(GL_ARRAY_BUFFER, buffer_gl->id);
        gl->funcs.VertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride, offset);
    }
}

static int pipeline_graphics_init(struct ngpu_pipeline *s)
{
    int ret = build_attribute_bindings(s);
    if (ret < 0)
        return ret;

    return 0;
}

static int pipeline_compute_init(struct ngpu_pipeline *s)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    ngli_assert(NGLI_HAS_ALL_FLAGS(gl->features, NGLI_FEATURE_GL_COMPUTE_SHADER_ALL));

    return 0;
}

struct ngpu_pipeline *ngpu_pipeline_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_pipeline_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_pipeline *)s;
}

int ngpu_pipeline_gl_init(struct ngpu_pipeline *s)
{
    struct ngpu_pipeline_gl *s_priv = (struct ngpu_pipeline_gl *)s;

    ngli_darray_init(&s_priv->attribute_bindings, sizeof(struct attribute_binding_gl), 0);

    if (s->type == NGPU_PIPELINE_TYPE_GRAPHICS) {
        int ret = pipeline_graphics_init(s);
        if (ret < 0)
            return ret;
    } else if (s->type == NGPU_PIPELINE_TYPE_COMPUTE) {
        int ret = pipeline_compute_init(s);
        if (ret < 0)
            return ret;
    } else {
        ngli_assert(0);
    }

    return 0;
}

static void set_graphics_state(struct ngpu_pipeline *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_glstate *glstate = &gpu_ctx_gl->glstate;
    struct ngpu_pipeline_graphics *graphics = &s->graphics;

    ngpu_glstate_update(gl, glstate, &graphics->state);
}

void ngpu_pipeline_gl_draw(struct ngpu_pipeline *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_glstate *glstate = &gpu_ctx_gl->glstate;
    struct ngpu_pipeline_graphics *graphics = &s->graphics;
    struct ngpu_program_gl *program_gl = (struct ngpu_program_gl *)s->program;

    set_graphics_state(s);
    ngpu_glstate_use_program(gl, glstate, program_gl->id);

    bind_vertex_attribs(s);

    const GLbitfield barriers = ngpu_bindgroup_gl_get_memory_barriers(gpu_ctx->bindgroup);
    if (barriers)
        gl->funcs.MemoryBarrier(barriers);

    const GLenum gl_topology = get_gl_topology(graphics->topology);
    gl->funcs.DrawArraysInstanced(gl_topology, (GLint)first_vertex, (GLint)nb_vertices, (GLint)nb_instances);

    if (barriers)
        gl->funcs.MemoryBarrier(barriers);
}

void ngpu_pipeline_gl_draw_indexed(struct ngpu_pipeline *s, uint32_t nb_indices, uint32_t nb_instances)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_glstate *glstate = &gpu_ctx_gl->glstate;
    struct ngpu_pipeline_graphics *graphics = &s->graphics;
    struct ngpu_program_gl *program_gl = (struct ngpu_program_gl *)s->program;

    set_graphics_state(s);
    ngpu_glstate_use_program(gl, glstate, program_gl->id);

    bind_vertex_attribs(s);

    const struct ngpu_buffer_gl *indices_gl = (const struct ngpu_buffer_gl *)gpu_ctx->index_buffer;
    const GLenum gl_indices_type = get_gl_indices_type(gpu_ctx->index_format);
    gl->funcs.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_gl->id);

    const GLbitfield barriers = ngpu_bindgroup_gl_get_memory_barriers(gpu_ctx->bindgroup);
    if (barriers)
        gl->funcs.MemoryBarrier(barriers);

    const GLenum gl_topology = get_gl_topology(graphics->topology);
    gl->funcs.DrawElementsInstanced(gl_topology, (GLint)nb_indices, gl_indices_type, 0, (GLint)nb_instances);

    if (barriers)
        gl->funcs.MemoryBarrier(barriers);
}

void ngpu_pipeline_gl_dispatch(struct ngpu_pipeline *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_glstate *glstate = &gpu_ctx_gl->glstate;
    struct ngpu_program_gl *program_gl = (struct ngpu_program_gl *)s->program;

    ngpu_glstate_use_program(gl, glstate, program_gl->id);

    const GLbitfield barriers = ngpu_bindgroup_gl_get_memory_barriers(gpu_ctx->bindgroup);
    if (barriers)
        gl->funcs.MemoryBarrier(barriers);

    ngli_assert(gl->features & NGLI_FEATURE_GL_COMPUTE_SHADER);
    gl->funcs.DispatchCompute(nb_group_x, nb_group_y, nb_group_z);

    if (barriers)
        gl->funcs.MemoryBarrier(barriers);
}

void ngpu_pipeline_gl_freep(struct ngpu_pipeline **sp)
{
    if (!*sp)
        return;

    struct ngpu_pipeline *s = *sp;
    struct ngpu_pipeline_gl *s_priv = (struct ngpu_pipeline_gl *)s;

    ngli_darray_reset(&s_priv->attribute_bindings);

    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    gl->funcs.DeleteVertexArrays(1, &s_priv->vao_id);

    ngli_freep(sp);
}
