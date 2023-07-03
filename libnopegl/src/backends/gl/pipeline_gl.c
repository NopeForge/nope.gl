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

#include "buffer_gl.h"
#include "format.h"
#include "gpu_ctx_gl.h"
#include "glcontext.h"
#include "log.h"
#include "memory.h"
#include "internal.h"
#include "pipeline_gl.h"
#include "program_gl.h"
#include "texture_gl.h"
#include "topology.h"
#include "type.h"

struct texture_binding_gl {
    struct pipeline_resource_desc desc;
    const struct texture *texture;
};

struct buffer_binding_gl {
    GLenum type;
    struct pipeline_resource_desc desc;
    const struct buffer *buffer;
    size_t offset;
    size_t size;
};

struct attribute_binding_gl {
    size_t binding;
    int location;
    int format;
    size_t stride;
    size_t offset;
};

static int build_texture_bindings(struct pipeline *s)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    const struct gpu_limits *limits = &s->gpu_ctx->limits;
    const struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    const struct glcontext *gl = gpu_ctx_gl->glcontext;

    size_t nb_textures = 0;
    size_t nb_images = 0;
    const struct pipeline_layout *layout = &s->layout;
    for (size_t i = 0; i < layout->nb_texture_descs; i++) {
        const struct pipeline_resource_desc *texture_desc = &layout->texture_descs[i];

        if (texture_desc->type == NGLI_TYPE_IMAGE_2D ||
            texture_desc->type == NGLI_TYPE_IMAGE_2D_ARRAY ||
            texture_desc->type == NGLI_TYPE_IMAGE_3D ||
            texture_desc->type == NGLI_TYPE_IMAGE_CUBE) {
            if (texture_desc->access & NGLI_ACCESS_WRITE_BIT)
                s_priv->use_barriers = 1;
            nb_images++;
        } else {
            nb_textures++;
        }

        struct texture_binding_gl binding = {
            .desc = *texture_desc,
        };
        if (!ngli_darray_push(&s_priv->texture_bindings, &binding))
            return NGL_ERROR_MEMORY;
    }

    if (nb_textures > limits->max_texture_image_units) {
        LOG(ERROR, "number of texture units (%zu) exceeds device limits (%u)", nb_textures, limits->max_texture_image_units);
        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }

    if (nb_images)
        ngli_assert(gl->features & NGLI_FEATURE_GL_SHADER_IMAGE_LOAD_STORE);

    if (nb_images > limits->max_image_units) {
        LOG(ERROR, "number of image units (%zu) exceeds device limits (%u)", nb_images, limits->max_image_units);
        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }

    return 0;
}

static const GLenum gl_access_map[NGLI_ACCESS_NB] = {
    [NGLI_ACCESS_READ_BIT]   = GL_READ_ONLY,
    [NGLI_ACCESS_WRITE_BIT]  = GL_WRITE_ONLY,
    [NGLI_ACCESS_READ_WRITE] = GL_READ_WRITE,
};

static GLenum get_gl_access(int access)
{
    return gl_access_map[access];
}

static void set_textures(struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    const struct texture_binding_gl *bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        const struct texture_binding_gl *texture_binding = &bindings[i];
        const struct texture *texture = texture_binding->texture;
        const struct texture_gl *texture_gl = (const struct texture_gl *)texture;

        if (texture_binding->desc.type == NGLI_TYPE_IMAGE_2D ||
            texture_binding->desc.type == NGLI_TYPE_IMAGE_2D_ARRAY ||
            texture_binding->desc.type == NGLI_TYPE_IMAGE_3D ||
            texture_binding->desc.type == NGLI_TYPE_IMAGE_CUBE) {
            GLuint texture_id = 0;
            const GLenum access = get_gl_access(texture_binding->desc.access);
            GLenum internal_format = GL_RGBA8;
            if (texture) {
                texture_id = texture_gl->id;
                internal_format = texture_gl->internal_format;
            }
            GLboolean layered = GL_FALSE;
            if (texture_binding->desc.type == NGLI_TYPE_IMAGE_2D_ARRAY ||
                texture_binding->desc.type == NGLI_TYPE_IMAGE_3D ||
                texture_binding->desc.type == NGLI_TYPE_IMAGE_CUBE)
                layered = GL_TRUE;
            ngli_glBindImageTexture(gl, texture_binding->desc.binding, texture_id, 0, layered, 0, access, internal_format);
        } else {
            ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_binding->desc.binding);
            if (texture) {
                ngli_glBindTexture(gl, texture_gl->target, texture_gl->id);
            } else {
                ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
                ngli_glBindTexture(gl, GL_TEXTURE_2D_ARRAY, 0);
                ngli_glBindTexture(gl, GL_TEXTURE_3D, 0);
                if (gl->features & NGLI_FEATURE_GL_OES_EGL_EXTERNAL_IMAGE)
                    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, 0);
            }
        }
    }
}

static void set_buffers(struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    size_t current_dynamic_offset = 0;
    const struct buffer_binding_gl *bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        const struct buffer_binding_gl *buffer_binding = &bindings[i];
        const struct buffer *buffer = buffer_binding->buffer;
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)buffer;
        const struct pipeline_resource_desc *buffer_desc = &buffer_binding->desc;
        size_t offset = buffer_binding->offset;
        if (buffer_desc->type == NGLI_TYPE_STORAGE_BUFFER_DYNAMIC ||
            buffer_desc->type == NGLI_TYPE_UNIFORM_BUFFER_DYNAMIC) {
            offset += s->dynamic_offsets[current_dynamic_offset++];
        }
        const size_t size = buffer_binding->size ? buffer_binding->size : buffer->size;
        ngli_glBindBufferRange(gl, buffer_binding->type, buffer_desc->binding, buffer_gl->id, offset, size);
    }
}

static const GLenum gl_target_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_UNIFORM_BUFFER]         = GL_UNIFORM_BUFFER,
    [NGLI_TYPE_UNIFORM_BUFFER_DYNAMIC] = GL_UNIFORM_BUFFER,
    [NGLI_TYPE_STORAGE_BUFFER]         = GL_SHADER_STORAGE_BUFFER,
    [NGLI_TYPE_STORAGE_BUFFER_DYNAMIC] = GL_SHADER_STORAGE_BUFFER,
};

static GLenum get_gl_target(int type)
{
    return gl_target_map[type];
}

static int build_buffer_bindings(struct pipeline *s)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    const struct pipeline_layout *layout = &s->layout;
    for (size_t i = 0; i < layout->nb_buffer_descs; i++) {
        const struct pipeline_resource_desc *pipeline_buffer_desc = &layout->buffer_descs[i];
        const int type = pipeline_buffer_desc->type;

        if (type == NGLI_TYPE_STORAGE_BUFFER ||
            type == NGLI_TYPE_STORAGE_BUFFER_DYNAMIC)
            ngli_assert(gl->features & NGLI_FEATURE_GL_SHADER_STORAGE_BUFFER_OBJECT);

        if (pipeline_buffer_desc->access & NGLI_ACCESS_WRITE_BIT)
            s_priv->use_barriers = 1;

        struct buffer_binding_gl binding = {
            .type = get_gl_target(type),
            .desc = *pipeline_buffer_desc,
        };
        if (!ngli_darray_push(&s_priv->buffer_bindings, &binding))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

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

static GLbitfield get_memory_barriers(const struct pipeline *s)
{
    const struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    const struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct pipeline_gl *s_priv = (const struct pipeline_gl *)s;

    GLbitfield barriers = 0;
    const struct buffer_binding_gl *buffer_bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        const struct buffer_binding_gl *binding = &buffer_bindings[i];
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)binding->buffer;
        if (!buffer_gl)
            continue;
        const struct pipeline_resource_desc *desc = &binding->desc;
        if (desc->access & NGLI_ACCESS_WRITE_BIT)
            barriers |= buffer_gl->barriers;
    }

    const struct texture_binding_gl *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        const struct texture_binding_gl *binding = &texture_bindings[i];
        const struct texture_gl *texture_gl = (const struct texture_gl *)binding->texture;
        if (!texture_gl)
            continue;
        const struct pipeline_resource_desc *desc = &binding->desc;
        if (desc->access & NGLI_ACCESS_WRITE_BIT)
            barriers |= texture_gl->barriers;
        if (gl->workaround_radeonsi_sync)
            barriers |= (texture_gl->barriers & GL_FRAMEBUFFER_BARRIER_BIT);
    }

    return barriers;
}

static void insert_memory_barriers_noop(struct pipeline *s)
{
}

static void insert_memory_barriers(struct pipeline *s)
{
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    /*
     * Compute the required barriers before execution as the associated
     * resources (textures, buffers) can change between executions.
     */
    GLbitfield barriers = get_memory_barriers(s);
    if (barriers)
        ngli_glMemoryBarrier(gl, barriers);
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

    ngli_darray_init(&s_priv->texture_bindings, sizeof(struct texture_binding_gl), 0);
    ngli_darray_init(&s_priv->buffer_bindings, sizeof(struct buffer_binding_gl), 0);
    ngli_darray_init(&s_priv->attribute_bindings, sizeof(struct attribute_binding_gl), 0);

    int ret;
    if ((ret = build_texture_bindings(s)) < 0 ||
        (ret = build_buffer_bindings(s)) < 0)
        return ret;

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        ret = pipeline_graphics_init(s);
        if (ret < 0)
            return ret;
    } else if (s->type == NGLI_PIPELINE_TYPE_COMPUTE) {
        ret = pipeline_compute_init(s);
        if (ret < 0)
            return ret;
    } else {
        ngli_assert(0);
    }

    s_priv->insert_memory_barriers = s_priv->use_barriers
                                   ? insert_memory_barriers
                                   : insert_memory_barriers_noop;

    return 0;
}

int ngli_pipeline_gl_update_texture(struct pipeline *s, int32_t index, const struct texture *texture)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct texture_binding_gl *texture_binding = ngli_darray_get(&s_priv->texture_bindings, index);
    texture_binding->texture = texture;

    return 0;
}

int ngli_pipeline_gl_update_buffer(struct pipeline *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct buffer_binding_gl *buffer_binding = ngli_darray_get(&s_priv->buffer_bindings, index);
    buffer_binding->buffer = buffer;
    buffer_binding->offset = offset;
    buffer_binding->size = size;

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
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    struct pipeline_graphics *graphics = &s->graphics;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    set_graphics_state(s);
    ngli_glstate_use_program(gl, glstate, program_gl->id);

    set_buffers(s, gl);
    set_textures(s, gl);
    bind_vertex_attribs(s, gl);

    s_priv->insert_memory_barriers(s);

    const GLenum gl_topology = get_gl_topology(graphics->topology);
    if (nb_instances > 1)
        ngli_glDrawArraysInstanced(gl, gl_topology, 0, nb_vertices, nb_instances);
    else
        ngli_glDrawArrays(gl, gl_topology, 0, nb_vertices);

    s_priv->insert_memory_barriers(s);
}

void ngli_pipeline_gl_draw_indexed(struct pipeline *s, int nb_indices, int nb_instances)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    struct pipeline_graphics *graphics = &s->graphics;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    set_graphics_state(s);
    ngli_glstate_use_program(gl, glstate, program_gl->id);

    set_buffers(s, gl);
    set_textures(s, gl);
    bind_vertex_attribs(s, gl);

    const struct buffer_gl *indices_gl = (const struct buffer_gl *)gpu_ctx->index_buffer;
    const GLenum gl_indices_type = get_gl_indices_type(gpu_ctx->index_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_gl->id);

    s_priv->insert_memory_barriers(s);

    const GLenum gl_topology = get_gl_topology(graphics->topology);
    if (nb_instances > 1)
        ngli_glDrawElementsInstanced(gl, gl_topology, nb_indices, gl_indices_type, 0, nb_instances);
    else
        ngli_glDrawElements(gl, gl_topology, nb_indices, gl_indices_type, 0);

    s_priv->insert_memory_barriers(s);
}

void ngli_pipeline_gl_dispatch(struct pipeline *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    ngli_glstate_use_program(gl, glstate, program_gl->id);
    set_buffers(s, gl);
    set_textures(s, gl);

    s_priv->insert_memory_barriers(s);

    ngli_assert(gl->features & NGLI_FEATURE_GL_COMPUTE_SHADER);
    ngli_glDispatchCompute(gl, nb_group_x, nb_group_y, nb_group_z);

    s_priv->insert_memory_barriers(s);
}

void ngli_pipeline_gl_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    ngli_darray_reset(&s_priv->texture_bindings);
    ngli_darray_reset(&s_priv->buffer_bindings);
    ngli_darray_reset(&s_priv->attribute_bindings);

    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    ngli_glDeleteVertexArrays(gl, 1, &s_priv->vao_id);

    ngli_freep(sp);
}
