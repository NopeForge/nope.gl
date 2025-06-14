/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2018-2022 GoPro Inc.
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
#include "ctx_gl.h"
#include "glcontext.h"
#include "glincludes.h"
#include "utils/memory.h"

static GLbitfield get_gl_barriers(uint32_t usage)
{
    GLbitfield barriers = 0;
    if (usage & NGPU_BUFFER_USAGE_TRANSFER_SRC_BIT)
        barriers |= GL_BUFFER_UPDATE_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_TRANSFER_DST_BIT)
        barriers |= GL_BUFFER_UPDATE_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        barriers |= GL_UNIFORM_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        barriers |= GL_SHADER_STORAGE_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_INDEX_BUFFER_BIT)
        barriers |= GL_ELEMENT_ARRAY_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        barriers |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_MAP_READ)
        barriers |= GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
    if (usage & NGPU_BUFFER_USAGE_MAP_WRITE)
        barriers |= GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
    return barriers;
}

static GLenum get_gl_usage(uint32_t usage)
{
    if (usage & NGPU_BUFFER_USAGE_DYNAMIC_BIT)
        return GL_DYNAMIC_DRAW;
    return GL_STATIC_DRAW;
}

static GLbitfield get_gl_map_flags(uint32_t usage)
{
    GLbitfield flags = 0;
    if (usage & NGPU_BUFFER_USAGE_MAP_READ)
        flags |= GL_MAP_READ_BIT;
    if (usage & NGPU_BUFFER_USAGE_MAP_WRITE)
        flags |= GL_MAP_WRITE_BIT;
    if (usage & NGPU_BUFFER_USAGE_MAP_PERSISTENT) {
        flags |= GL_MAP_COHERENT_BIT;
        flags |= GL_MAP_PERSISTENT_BIT;
    }
    return flags;
}

struct ngpu_buffer *ngpu_buffer_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_buffer_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_buffer *)s;
}

static void unref_cmd_buffer(void *user_arg, void *data)
{
    struct ngpu_cmd_buffer_gl **cmd_bufferp = data;
    ngpu_cmd_buffer_gl_freep(cmd_bufferp);
}

int ngpu_buffer_gl_init(struct ngpu_buffer *s)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;

    ngli_darray_init(&s_priv->cmd_buffers, sizeof(struct cmd_buffer_vk *), 0);
    ngli_darray_set_free_func(&s_priv->cmd_buffers, unref_cmd_buffer, NULL);

    s_priv->map_flags = get_gl_map_flags(s->usage);
    s_priv->barriers = get_gl_barriers(s->usage);

    GLsizeiptr size = (GLsizeiptr)s->size;

    gl->funcs.GenBuffers(1, &s_priv->id);
    gl->funcs.BindBuffer(GL_ARRAY_BUFFER, s_priv->id);
    if (gl->features & NGLI_FEATURE_GL_BUFFER_STORAGE) {
        const GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT;
        gl->funcs.BufferStorage(GL_ARRAY_BUFFER, size, NULL, storage_flags | s_priv->map_flags);
    } else if (gl->features & NGLI_FEATURE_GL_EXT_BUFFER_STORAGE) {
        const GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT;
        gl->funcs.BufferStorageEXT(GL_ARRAY_BUFFER, size, NULL, storage_flags | s_priv->map_flags);
    } else {
        ngli_assert(!NGLI_HAS_ALL_FLAGS(s->usage, NGPU_BUFFER_USAGE_MAP_PERSISTENT));
        gl->funcs.BufferData(GL_ARRAY_BUFFER, size, NULL, get_gl_usage(s->usage));
    }

    return 0;
}

int ngpu_buffer_gl_wait(struct ngpu_buffer *s)
{
    struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;

    struct ngpu_cmd_buffer_gl **cmd_buffers = ngli_darray_data(&s_priv->cmd_buffers);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->cmd_buffers); i++) {
        struct ngpu_cmd_buffer_gl *cmd_buffer = cmd_buffers[i];
        ngpu_cmd_buffer_gl_wait(cmd_buffer);
    }
    ngli_darray_clear(&s_priv->cmd_buffers);

    return 0;
}

int ngpu_buffer_gl_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;
    gl->funcs.BindBuffer(GL_ARRAY_BUFFER, s_priv->id);
    gl->funcs.BufferSubData(GL_ARRAY_BUFFER, (GLsizeiptr)offset, (GLsizeiptr)size, data);
    return 0;
}

int ngpu_buffer_gl_map(struct ngpu_buffer *s, size_t offset, size_t size, void **datap)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;
    gl->funcs.BindBuffer(GL_ARRAY_BUFFER, s_priv->id);
    void *data = gl->funcs.MapBufferRange(GL_ARRAY_BUFFER, (GLsizeiptr)offset, (GLsizeiptr)size, s_priv->map_flags);
    if (!data)
        return NGL_ERROR_GRAPHICS_GENERIC;
    *datap = data;
    return 0;
}

void ngpu_buffer_gl_unmap(struct ngpu_buffer *s)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;
    gl->funcs.BindBuffer(GL_ARRAY_BUFFER, s_priv->id);
    gl->funcs.UnmapBuffer(GL_ARRAY_BUFFER);
}

static size_t buffer_gl_find_cmd_buffer(struct ngpu_buffer *s, struct ngpu_cmd_buffer_gl *cmd_buffer)
{
    struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;

    struct ngpu_cmd_buffer_gl **cmd_buffers = ngli_darray_data(&s_priv->cmd_buffers);
    for (size_t i = 0; i < ngli_darray_count(&s_priv->cmd_buffers); i++) {
        if (cmd_buffers[i] == cmd_buffer)
            return i;
    }

    return SIZE_MAX;
}

int ngpu_buffer_gl_ref_cmd_buffer(struct ngpu_buffer *s, struct ngpu_cmd_buffer_gl *cmd_buffer)
{
    struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;

    size_t index = buffer_gl_find_cmd_buffer(s, cmd_buffer);
    if (index != SIZE_MAX)
        return 0;

    if (!ngli_darray_push(&s_priv->cmd_buffers, cmd_buffer))
        return NGL_ERROR_MEMORY;

    NGLI_RC_REF(cmd_buffer);

    return 0;
}

int ngpu_buffer_gl_unref_cmd_buffer(struct ngpu_buffer *s, struct ngpu_cmd_buffer_gl *cmd_buffer)
{
    struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;

    size_t index = buffer_gl_find_cmd_buffer(s, cmd_buffer);
    if (index == SIZE_MAX)
        return 0;

    ngli_darray_remove(&s_priv->cmd_buffers, index);
    NGLI_RC_UNREFP(&cmd_buffer);

    return 0;
}

void ngpu_buffer_gl_freep(struct ngpu_buffer **sp)
{
    if (!*sp)
        return;

    struct ngpu_buffer *s = *sp;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct ngpu_buffer_gl *s_priv = (struct ngpu_buffer_gl *)s;

    ngli_darray_reset(&s_priv->cmd_buffers);

    gl->funcs.DeleteBuffers(1, &s_priv->id);
    ngli_freep(sp);
}
