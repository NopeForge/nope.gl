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
#include "format.h"
#include "glcontext.h"
#include "gpu_ctx_gl.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "pipeline_gl.h"
#include "program_gl.h"
#include "topology.h"

struct attribute_binding_gl {
    size_t binding;
    int location;
    int format;
    size_t stride;
    size_t offset;
};

static int build_attribute_bindings(struct pipeline *s)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    ngli_glGenVertexArrays(gl, 1, &s_priv->vao_id);
    ngli_glBindVertexArray(gl, s_priv->vao_id);

    const struct pipeline_graphics *graphics = &s->graphics;
    const struct vertex_state *state = &graphics->vertex_state;
    for (size_t i = 0; i < state->nb_buffers; i++) {
        const struct vertex_buffer_layout *buffer = &state->buffers[i];
        for (size_t j = 0; j < buffer->nb_attributes; j++) {
            const struct vertex_attribute *attribute = &buffer->attributes[j];

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
            ngli_glEnableVertexAttribArray(gl, location);
            if (rate > 0)
                ngli_glVertexAttribDivisor(gl, location, rate);
        }
    }

    return 0;
}

static const GLenum gl_primitive_topology_map[NGLI_PRIMITIVE_TOPOLOGY_NB] = {
    [NGLI_PRIMITIVE_TOPOLOGY_POINT_LIST]     = GL_POINTS,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_LIST]      = GL_LINES,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_STRIP]     = GL_LINE_STRIP,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST]  = GL_TRIANGLES,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = GL_TRIANGLE_STRIP,
};

static GLenum get_gl_topology(int topology)
{
    return gl_primitive_topology_map[topology];
}

static const GLenum gl_indices_type_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R16_UNORM] = GL_UNSIGNED_SHORT,
    [NGLI_FORMAT_R32_UINT]  = GL_UNSIGNED_INT,
};

static GLenum get_gl_indices_type(int indices_format)
{
    return gl_indices_type_map[indices_format];
}

static void bind_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    const struct pipeline_gl *s_priv = (const struct pipeline_gl *)s;
    struct gpu_ctx *gpu_ctx = (struct gpu_ctx *)s->gpu_ctx;

    ngli_glBindVertexArray(gl, s_priv->vao_id);

    const struct buffer **vertex_buffers = gpu_ctx->vertex_buffers;
    const struct attribute_binding_gl *bindings = ngli_darray_data(&s_priv->attribute_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->attribute_bindings); i++) {
        const struct attribute_binding_gl *attribute_binding = &bindings[i];
        const size_t binding = attribute_binding->binding;
        const GLuint location = attribute_binding->location;
        const GLuint size = ngli_format_get_nb_comp(attribute_binding->format);
        const GLsizei stride = (GLsizei)attribute_binding->stride;
        const void *offset = (void *)(uintptr_t)attribute_binding->offset;
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)vertex_buffers[binding];
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer_gl->id);
        ngli_glVertexAttribPointer(gl, location, size, GL_FLOAT, GL_FALSE, stride, offset);
    }
}

static int pipeline_graphics_init(struct pipeline *s)
{
    int ret = build_attribute_bindings(s);
    if (ret < 0)
        return ret;

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    ngli_assert(NGLI_HAS_ALL_FLAGS(gl->features, NGLI_FEATURE_GL_COMPUTE_SHADER_ALL));

    return 0;
}

struct pipeline *ngli_pipeline_gl_create(struct gpu_ctx *gpu_ctx)
{
    struct pipeline_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct pipeline *)s;
}

int ngli_pipeline_gl_init(struct pipeline *s)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    ngli_darray_init(&s_priv->attribute_bindings, sizeof(struct attribute_binding_gl), 0);

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        int ret = pipeline_graphics_init(s);
        if (ret < 0)
            return ret;
    } else if (s->type == NGLI_PIPELINE_TYPE_COMPUTE) {
        int ret = pipeline_compute_init(s);
        if (ret < 0)
            return ret;
    } else {
        ngli_assert(0);
    }

    return 0;
}

static void set_graphics_state(struct pipeline *s)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    const struct ngl_config *config = &gpu_ctx->config;
    struct rendertarget *rendertarget = gpu_ctx->rendertarget;
    struct pipeline_graphics *graphics = &s->graphics;

    ngli_glstate_update(gl, glstate, &graphics->state);
    ngli_glstate_update_viewport(gl, glstate, &gpu_ctx->viewport);
    struct scissor scissor = gpu_ctx->scissor;
    if (config->offscreen)
        scissor.y = NGLI_MAX(rendertarget->height - scissor.y - scissor.height, 0);
    ngli_glstate_update_scissor(gl, glstate, &scissor);
}

void ngli_pipeline_gl_draw(struct pipeline *s, int nb_vertices, int nb_instances)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    struct pipeline_graphics *graphics = &s->graphics;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    set_graphics_state(s);
    ngli_glstate_use_program(gl, glstate, program_gl->id);

    bind_vertex_attribs(s, gl);

    const GLbitfield barriers = ngli_bindgroup_gl_get_memory_barriers(gpu_ctx->bindgroup);
    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);

    const GLenum gl_topology = get_gl_topology(graphics->topology);
    if (nb_instances > 1)
        ngli_glDrawArraysInstanced(gl, gl_topology, 0, nb_vertices, nb_instances);
    else
        ngli_glDrawArrays(gl, gl_topology, 0, nb_vertices);

    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);
}

void ngli_pipeline_gl_draw_indexed(struct pipeline *s, int nb_indices, int nb_instances)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    struct pipeline_graphics *graphics = &s->graphics;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    set_graphics_state(s);
    ngli_glstate_use_program(gl, glstate, program_gl->id);

    bind_vertex_attribs(s, gl);

    const struct buffer_gl *indices_gl = (const struct buffer_gl *)gpu_ctx->index_buffer;
    const GLenum gl_indices_type = get_gl_indices_type(gpu_ctx->index_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_gl->id);

    const GLbitfield barriers = ngli_bindgroup_gl_get_memory_barriers(gpu_ctx->bindgroup);
    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);

    const GLenum gl_topology = get_gl_topology(graphics->topology);
    if (nb_instances > 1)
        ngli_glDrawElementsInstanced(gl, gl_topology, nb_indices, gl_indices_type, 0, nb_instances);
    else
        ngli_glDrawElements(gl, gl_topology, nb_indices, gl_indices_type, 0);

    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);
}

void ngli_pipeline_gl_dispatch(struct pipeline *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    ngli_glstate_use_program(gl, glstate, program_gl->id);

    const GLbitfield barriers = ngli_bindgroup_gl_get_memory_barriers(gpu_ctx->bindgroup);
    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);

    ngli_assert(gl->features & NGLI_FEATURE_GL_COMPUTE_SHADER);
    ngli_glDispatchCompute(gl, nb_group_x, nb_group_y, nb_group_z);

    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);
}

void ngli_pipeline_gl_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    ngli_darray_reset(&s_priv->attribute_bindings);

    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    ngli_glDeleteVertexArrays(gl, 1, &s_priv->vao_id);

    ngli_freep(sp);
}
