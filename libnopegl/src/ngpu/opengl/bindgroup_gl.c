/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "bindgroup_gl.h"
#include "buffer_gl.h"
#include "ctx_gl.h"
#include "log.h"
#include "utils/memory.h"
#include "texture_gl.h"

struct ngpu_bindgroup_layout *ngpu_bindgroup_layout_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_bindgroup_layout_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_bindgroup_layout *)s;
}

int ngpu_bindgroup_layout_gl_init(struct ngpu_bindgroup_layout *s)
{
    return 0;
}

void ngpu_bindgroup_layout_gl_freep(struct ngpu_bindgroup_layout **sp)
{
    struct ngpu_bindgroup_layout *s = *sp;
    if (!s)
        return;
    ngli_freep(sp);
}

static int build_texture_bindings(struct ngpu_bindgroup *s)
{
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;
    const struct ngpu_limits *limits = &s->gpu_ctx->limits;
    const struct glcontext *gl = ((const struct ngpu_ctx_gl *)s->gpu_ctx)->glcontext;

    size_t nb_textures = 0;
    size_t nb_images = 0;
    const struct ngpu_bindgroup_layout *layout = s->layout;
    for (size_t i = 0; i < layout->nb_textures; i++) {
        const struct ngpu_bindgroup_layout_entry *layout_entry = &layout->textures[i];
        if (layout_entry->type == NGPU_TYPE_IMAGE_2D ||
            layout_entry->type == NGPU_TYPE_IMAGE_2D_ARRAY ||
            layout_entry->type == NGPU_TYPE_IMAGE_3D ||
            layout_entry->type == NGPU_TYPE_IMAGE_CUBE) {
            if (layout_entry->access & NGPU_ACCESS_WRITE_BIT)
                s_priv->use_barriers = 1;
            nb_images++;
        } else {
            nb_textures++;
        }

        struct texture_binding_gl binding_gl = {
            .layout_entry = *layout_entry,
        };
        if (!ngli_darray_push(&s_priv->texture_bindings, &binding_gl))
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

static const GLenum gl_target_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_UNIFORM_BUFFER]         = GL_UNIFORM_BUFFER,
    [NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC] = GL_UNIFORM_BUFFER,
    [NGPU_TYPE_STORAGE_BUFFER]         = GL_SHADER_STORAGE_BUFFER,
    [NGPU_TYPE_STORAGE_BUFFER_DYNAMIC] = GL_SHADER_STORAGE_BUFFER,
};

static GLenum get_gl_target(enum ngpu_type type)
{
    return gl_target_map[type];
}

static int build_buffer_bindings(struct ngpu_bindgroup *s)
{
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    const struct ngpu_bindgroup_layout *layout = s->layout;
    for (size_t i = 0; i < layout->nb_buffers; i++) {
        const struct ngpu_bindgroup_layout_entry *layout_entry = &layout->buffers[i];
        const enum ngpu_type type = layout_entry->type;

        if (type == NGPU_TYPE_STORAGE_BUFFER ||
            type == NGPU_TYPE_STORAGE_BUFFER_DYNAMIC)
            ngli_assert(gl->features & NGLI_FEATURE_GL_SHADER_STORAGE_BUFFER_OBJECT);

        if (layout_entry->access & NGPU_ACCESS_WRITE_BIT) {
            s_priv->use_barriers = 1;
        }

        struct buffer_binding_gl binding = {
            .layout_entry = *layout_entry,
        };
        if (!ngli_darray_push(&s_priv->buffer_bindings, &binding))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static GLbitfield get_memory_barriers(const struct ngpu_bindgroup *s)
{
    const struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    const struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_bindgroup_gl *s_priv = (const struct ngpu_bindgroup_gl *)s;

    GLbitfield barriers = 0;
    const struct buffer_binding_gl *buffer_bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        const struct buffer_binding_gl *binding_gl = &buffer_bindings[i];
        const struct ngpu_buffer_gl *buffer_gl = (const struct ngpu_buffer_gl *)binding_gl->buffer;
        if (!buffer_gl)
            continue;
        const struct ngpu_bindgroup_layout_entry *entry = &binding_gl->layout_entry;
        if (entry->access & NGPU_ACCESS_WRITE_BIT)
            barriers |= buffer_gl->barriers;
    }

    const struct texture_binding_gl *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        const struct texture_binding_gl *binding_gl = &texture_bindings[i];
        const struct ngpu_texture_gl *texture_gl = (const struct ngpu_texture_gl *)binding_gl->texture;
        if (!texture_gl)
            continue;
        const struct ngpu_bindgroup_layout_entry *entry = &binding_gl->layout_entry;
        if (entry->access & NGPU_ACCESS_WRITE_BIT)
            barriers |= texture_gl->barriers;
        if (gl->workaround_radeonsi_sync)
            barriers |= (texture_gl->barriers & GL_FRAMEBUFFER_BARRIER_BIT);
    }

    return barriers;
}

GLbitfield ngpu_bindgroup_gl_get_memory_barriers(struct ngpu_bindgroup *s)
{
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;

    if (!s_priv->use_barriers)
        return 0;

    return get_memory_barriers(s);
}

struct ngpu_bindgroup *ngpu_bindgroup_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_bindgroup_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_bindgroup *)s;
}

static void unref_texture_binding(void *user_arg, void *data)
{
    struct texture_binding_gl *binding = data;
    NGLI_RC_UNREFP(&binding->texture);
}

static void unref_buffer_binding(void *user_arg, void *data)
{
    struct buffer_binding_gl *binding = data;
    NGLI_RC_UNREFP(&binding->buffer);
}

int ngpu_bindgroup_gl_init(struct ngpu_bindgroup *s, const struct ngpu_bindgroup_params *params)
{
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;

    s->layout = params->layout;

    ngli_darray_init(&s_priv->texture_bindings, sizeof(struct texture_binding_gl), 0);
    ngli_darray_init(&s_priv->buffer_bindings, sizeof(struct buffer_binding_gl), 0);

    ngli_darray_set_free_func(&s_priv->texture_bindings, unref_texture_binding, NULL);
    ngli_darray_set_free_func(&s_priv->buffer_bindings, unref_buffer_binding, NULL);

    int ret;
    if((ret = build_texture_bindings(s)) < 0 ||
       (ret = build_buffer_bindings(s)) < 0)
        return ret;

    return 0;
}

int ngpu_bindgroup_gl_update_texture(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_texture_binding *binding)
{
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;
    struct texture_binding_gl *binding_gl = ngli_darray_get(&s_priv->texture_bindings, index);
    NGLI_RC_UNREFP(&binding_gl->texture);
    binding_gl->texture = binding->texture ? NGLI_RC_REF(binding->texture) : NULL;

    return 0;
}

int ngpu_bindgroup_gl_update_buffer(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_buffer_binding *binding)
{
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;
    struct buffer_binding_gl *binding_gl = ngli_darray_get(&s_priv->buffer_bindings, index);
    NGLI_RC_UNREFP(&binding_gl->buffer);
    binding_gl->buffer = NGLI_RC_REF(binding->buffer);
    binding_gl->offset = binding->offset;
    binding_gl->size = binding->size;

    return 0;
}

static const GLenum gl_access_map[NGPU_ACCESS_NB] = {
    [NGPU_ACCESS_READ_BIT]   = GL_READ_ONLY,
    [NGPU_ACCESS_WRITE_BIT]  = GL_WRITE_ONLY,
    [NGPU_ACCESS_READ_WRITE] = GL_READ_WRITE,
};

static GLenum get_gl_access(enum ngpu_access access)
{
    return gl_access_map[access];
}

void ngpu_bindgroup_gl_bind(struct ngpu_bindgroup *s, const uint32_t *dynamic_offsets, size_t nb_dynamic_offsets)
{
    const struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;
    const struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    const struct glcontext *gl = gpu_ctx_gl->glcontext;

    const struct texture_binding_gl *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        const struct texture_binding_gl *texture_binding = &texture_bindings[i];
        const struct ngpu_bindgroup_layout_entry *layout_entry = &texture_binding->layout_entry;
        const struct ngpu_texture *texture = texture_binding->texture;
        const struct ngpu_texture_gl *texture_gl = (const struct ngpu_texture_gl *)texture;

        if (layout_entry->type == NGPU_TYPE_IMAGE_2D ||
            layout_entry->type == NGPU_TYPE_IMAGE_2D_ARRAY ||
            layout_entry->type == NGPU_TYPE_IMAGE_3D ||
            layout_entry->type == NGPU_TYPE_IMAGE_CUBE) {
            GLuint texture_id = 0;
            const GLenum access = get_gl_access(texture_binding->layout_entry.access);
            GLenum internal_format = GL_RGBA8;
            if (texture_gl) {
                texture_id = texture_gl->id;
                internal_format = texture_gl->internal_format;
            }
            GLboolean layered = GL_FALSE;
            if (texture_binding->layout_entry.type == NGPU_TYPE_IMAGE_2D_ARRAY ||
                texture_binding->layout_entry.type == NGPU_TYPE_IMAGE_3D ||
                texture_binding->layout_entry.type == NGPU_TYPE_IMAGE_CUBE)
                layered = GL_TRUE;
            gl->funcs.BindImageTexture(texture_binding->layout_entry.binding, texture_id, 0, layered, 0, access, internal_format);
        } else {
            gl->funcs.ActiveTexture(GL_TEXTURE0 + texture_binding->layout_entry.binding);
            if (texture_gl) {
                gl->funcs.BindTexture(texture_gl->target, texture_gl->id);
            } else {
                gl->funcs.BindTexture(GL_TEXTURE_2D, 0);
                gl->funcs.BindTexture(GL_TEXTURE_2D_ARRAY, 0);
                gl->funcs.BindTexture(GL_TEXTURE_3D, 0);
                if (gl->features & NGLI_FEATURE_GL_OES_EGL_EXTERNAL_IMAGE)
                    gl->funcs.BindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            }
        }
    }

    size_t current_dynamic_offset = 0;
    const struct buffer_binding_gl *buffer_bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        const struct buffer_binding_gl *buffer_binding = &buffer_bindings[i];
        const struct ngpu_buffer *buffer = buffer_binding->buffer;
        const struct ngpu_buffer_gl *buffer_gl = (const struct ngpu_buffer_gl *)buffer;
        const struct ngpu_bindgroup_layout_entry *layout_entry = &buffer_binding->layout_entry;
        const GLenum target = get_gl_target(layout_entry->type);
        size_t offset = buffer_binding->offset;
        if (layout_entry->type == NGPU_TYPE_STORAGE_BUFFER_DYNAMIC ||
            layout_entry->type == NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC) {
            offset += dynamic_offsets[current_dynamic_offset++];
        }
        const size_t size = buffer_binding->size;
        gl->funcs.BindBufferRange(target, layout_entry->binding, buffer_gl->id, offset, size);
    }
}

void ngpu_bindgroup_gl_freep(struct ngpu_bindgroup **sp)
{
    if (!*sp)
        return;

    struct ngpu_bindgroup *s = *sp;
    struct ngpu_bindgroup_gl *s_priv = (struct ngpu_bindgroup_gl *)s;

    ngli_darray_reset(&s_priv->texture_bindings);
    ngli_darray_reset(&s_priv->buffer_bindings);
    ngli_freep(sp);
}
