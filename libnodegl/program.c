/*
 * Copyright 2018 GoPro Inc.
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

#include <stdlib.h>
#include <string.h>

#include "glincludes.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "program.h"
#include "type.h"

static int program_check_status(const struct glcontext *gl, GLuint id, GLenum status)
{
    char *info_log = NULL;
    int info_log_length = 0;

    void (*get_info)(const struct glcontext *gl, GLuint id, GLenum pname, GLint *params);
    void (*get_log)(const struct glcontext *gl, GLuint id, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    const char *type_str;

    if (status == GL_COMPILE_STATUS) {
        type_str = "compile";
        get_info = ngli_glGetShaderiv;
        get_log  = ngli_glGetShaderInfoLog;
    } else if (status == GL_LINK_STATUS) {
        type_str = "link";
        get_info = ngli_glGetProgramiv;
        get_log  = ngli_glGetProgramInfoLog;
    } else {
        ngli_assert(0);
    }

    GLint result = GL_FALSE;
    get_info(gl, id, status, &result);
    if (result == GL_TRUE)
        return 0;

    get_info(gl, id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (!info_log_length)
        return NGL_ERROR_BUG;

    info_log = ngli_malloc(info_log_length);
    if (!info_log)
        return NGL_ERROR_MEMORY;

    get_log(gl, id, info_log_length, NULL, info_log);
    while (info_log_length && strchr(" \r\n", info_log[info_log_length - 1]))
        info_log[--info_log_length] = 0;

    LOG(ERROR, "could not %s shader: %s", type_str, info_log);
    return NGL_ERROR_INVALID_DATA;
}

static void free_pinfo(void *user_arg, void *data)
{
    ngli_free(data);
}

static const struct {
    GLenum gl_type;
    int type;
} types_map[] = {
    {GL_INT,                         NGLI_TYPE_INT},
    {GL_INT_VEC2,                    NGLI_TYPE_IVEC2},
    {GL_INT_VEC3,                    NGLI_TYPE_IVEC3},
    {GL_INT_VEC4,                    NGLI_TYPE_IVEC4},
    {GL_UNSIGNED_INT,                NGLI_TYPE_UINT},
    {GL_UNSIGNED_INT_VEC2,           NGLI_TYPE_UIVEC2},
    {GL_UNSIGNED_INT_VEC3,           NGLI_TYPE_UIVEC3},
    {GL_UNSIGNED_INT_VEC4,           NGLI_TYPE_UIVEC4},
    {GL_FLOAT,                       NGLI_TYPE_FLOAT},
    {GL_FLOAT_VEC2,                  NGLI_TYPE_VEC2},
    {GL_FLOAT_VEC3,                  NGLI_TYPE_VEC3},
    {GL_FLOAT_VEC4,                  NGLI_TYPE_VEC4},
    {GL_FLOAT_MAT3,                  NGLI_TYPE_MAT3},
    {GL_FLOAT_MAT4,                  NGLI_TYPE_MAT4},
    {GL_BOOL,                        NGLI_TYPE_BOOL},
    {GL_SAMPLER_2D,                  NGLI_TYPE_SAMPLER_2D},
    {GL_SAMPLER_2D_RECT,             NGLI_TYPE_SAMPLER_2D_RECT},
    {GL_SAMPLER_3D,                  NGLI_TYPE_SAMPLER_3D},
    {GL_SAMPLER_CUBE,                NGLI_TYPE_SAMPLER_CUBE},
    {GL_SAMPLER_EXTERNAL_OES,        NGLI_TYPE_SAMPLER_EXTERNAL_OES},
    {GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT, NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT},
    {GL_IMAGE_2D,                    NGLI_TYPE_IMAGE_2D},
};

static int get_type(GLenum gl_type)
{
    for (int i = 0; i < NGLI_ARRAY_NB(types_map); i++)
        if (types_map[i].gl_type == gl_type)
            return types_map[i].type;
    return NGLI_TYPE_NONE;
}

static struct program_variable_info *program_variable_info_create()
{
    struct program_variable_info *info = ngli_calloc(1, sizeof(*info));
    if (!info)
        return NULL;
    info->size     = -1;
    info->binding  = -1;
    info->location = -1;
    return info;
}

static struct hmap *program_probe_uniforms(struct glcontext *gl, GLuint pid)
{
    struct hmap *umap = ngli_hmap_create();
    if (!umap)
        return NULL;
    ngli_hmap_set_free(umap, free_pinfo, NULL);

    int nb_active_uniforms;
    ngli_glGetProgramiv(gl, pid, GL_ACTIVE_UNIFORMS, &nb_active_uniforms);
    for (int i = 0; i < nb_active_uniforms; i++) {
        char name[MAX_ID_LEN];
        struct program_variable_info *info = program_variable_info_create();
        if (!info) {
            ngli_hmap_freep(&umap);
            return NULL;
        }

        GLenum type;
        ngli_glGetActiveUniform(gl, pid, i, sizeof(name), NULL,
                                &info->size, &type, name);

        info->type = get_type(type);
        if (info->type == NGLI_TYPE_NONE) {
            LOG(WARNING, "unrecognized uniform type 0x%x, ignore", type);
            ngli_free(info);
            continue;
        }

        /* Remove [0] suffix from names of uniform arrays */
        name[strcspn(name, "[")] = 0;
        info->location = ngli_glGetUniformLocation(gl, pid, name);

        if (info->type == NGLI_TYPE_IMAGE_2D) {
            ngli_glGetUniformiv(gl, pid, info->location, &info->binding);
        } else {
            info->binding = -1;
        }

        LOG(DEBUG, "uniform[%d/%d]: %s location:%d size=%d type=0x%x binding=%d",
            i + 1, nb_active_uniforms, name, info->location, info->size, info->type, info->binding);

        int ret = ngli_hmap_set(umap, name, info);
        if (ret < 0) {
            ngli_hmap_freep(&umap);
            return NULL;
        }
    }

    return umap;
}

static struct hmap *program_probe_attributes(struct glcontext *gl, GLuint pid)
{
    struct hmap *amap = ngli_hmap_create();
    if (!amap)
        return NULL;
    ngli_hmap_set_free(amap, free_pinfo, NULL);

    int nb_active_attributes;
    ngli_glGetProgramiv(gl, pid, GL_ACTIVE_ATTRIBUTES, &nb_active_attributes);
    for (int i = 0; i < nb_active_attributes; i++) {
        char name[MAX_ID_LEN];
        struct program_variable_info *info = program_variable_info_create();
        if (!info) {
            ngli_hmap_freep(&amap);
            return NULL;
        }

        GLenum type;
        ngli_glGetActiveAttrib(gl, pid, i, sizeof(name), NULL,
                               &info->size, &type, name);
        info->type = get_type(type);
        if (info->type == NGLI_TYPE_NONE) {
            LOG(WARNING, "unrecognized attribute type 0x%x, ignore", type);
            ngli_free(info);
            continue;
        }

        info->location = ngli_glGetAttribLocation(gl, pid, name);
        LOG(DEBUG, "attribute[%d/%d]: %s location:%d size=%d type=0x%x",
            i + 1, nb_active_attributes, name, info->location, info->size, info->type);

        int ret = ngli_hmap_set(amap, name, info);
        if (ret < 0) {
            ngli_hmap_freep(&amap);
            return NULL;
        }
    }

    return amap;
}

static struct hmap *program_probe_buffer_blocks(struct glcontext *gl, GLuint pid)
{
    struct hmap *bmap = ngli_hmap_create();
    if (!bmap)
        return NULL;
    ngli_hmap_set_free(bmap, free_pinfo, NULL);

    if (!(gl->features & NGLI_FEATURE_UNIFORM_BUFFER_OBJECT))
        return bmap;

    int binding = 0;

    /* Uniform Buffers */
    int nb_active_uniform_buffers;
    ngli_glGetProgramiv(gl, pid, GL_ACTIVE_UNIFORM_BLOCKS, &nb_active_uniform_buffers);
    for (int i = 0; i < nb_active_uniform_buffers; i++) {
        char name[MAX_ID_LEN] = {0};
        struct program_variable_info *info = program_variable_info_create();
        if (!info) {
            ngli_hmap_freep(&bmap);
            return NULL;
        }
        info->type = NGLI_TYPE_UNIFORM_BUFFER;

        ngli_glGetActiveUniformBlockName(gl, pid, i, sizeof(name), NULL, name);
        GLuint block_index = ngli_glGetUniformBlockIndex(gl, pid, name);
        info->binding = binding++;
        ngli_glUniformBlockBinding(gl, pid, block_index, info->binding);

        LOG(DEBUG, "ubo[%d/%d]: %s binding:%d",
            i + 1, nb_active_uniform_buffers, name, info->binding);

        int ret = ngli_hmap_set(bmap, name, info);
        if (ret < 0) {
            ngli_hmap_freep(&bmap);
            return NULL;
        }
    }

    if (!((gl->features & NGLI_FEATURE_PROGRAM_INTERFACE_QUERY) &&
          (gl->features & NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)))
        return bmap;

    /* Shader Storage Buffers */
    int nb_active_buffers;
    ngli_glGetProgramInterfaceiv(gl, pid, GL_SHADER_STORAGE_BLOCK,
                                 GL_ACTIVE_RESOURCES, &nb_active_buffers);
    for (int i = 0; i < nb_active_buffers; i++) {
        char name[MAX_ID_LEN] = {0};
        struct program_variable_info *info = program_variable_info_create();
        if (!info) {
            ngli_hmap_freep(&bmap);
            return NULL;
        }
        info->type = NGLI_TYPE_STORAGE_BUFFER;

        ngli_glGetProgramResourceName(gl, pid, GL_SHADER_STORAGE_BLOCK, i, sizeof(name), NULL, name);
        GLuint block_index = ngli_glGetProgramResourceIndex(gl, pid, GL_SHADER_STORAGE_BLOCK, name);
        static const GLenum props[] = {GL_BUFFER_BINDING};
        ngli_glGetProgramResourceiv(gl, pid, GL_SHADER_STORAGE_BLOCK, block_index,
                                    NGLI_ARRAY_NB(props), props, 1, NULL, &info->binding);

        LOG(DEBUG, "ssbo[%d/%d]: %s binding:%d",
            i + 1, nb_active_buffers, name, info->binding);

        int ret = ngli_hmap_set(bmap, name, info);
        if (ret < 0) {
            ngli_hmap_freep(&bmap);
            return NULL;
        }
    }

    return bmap;
}

int ngli_program_init(struct program *s, struct ngl_ctx *ctx, const char *vertex, const char *fragment, const char *compute)
{
    int ret = 0;
    struct {
        GLenum type;
        const char *src;
        GLuint id;
    } shaders[] = {
        [NGLI_PROGRAM_SHADER_VERT] = {GL_VERTEX_SHADER,   vertex,   0},
        [NGLI_PROGRAM_SHADER_FRAG] = {GL_FRAGMENT_SHADER, fragment, 0},
        [NGLI_PROGRAM_SHADER_COMP] = {GL_COMPUTE_SHADER,  compute,  0},
    };

    struct glcontext *gl = ctx->glcontext;

    if (compute && (gl->features & NGLI_FEATURE_COMPUTE_SHADER_ALL) != NGLI_FEATURE_COMPUTE_SHADER_ALL) {
        LOG(ERROR, "context does not support compute shaders");
        return NGL_ERROR_UNSUPPORTED;
    }

    s->ctx = ctx;
    s->id = ngli_glCreateProgram(gl);

    for (int i = 0; i < NGLI_ARRAY_NB(shaders); i++) {
        if (!shaders[i].src)
            continue;
        GLuint shader = ngli_glCreateShader(gl, shaders[i].type);
        shaders[i].id = shader;
        ngli_glShaderSource(gl, shader, 1, &shaders[i].src, NULL);
        ngli_glCompileShader(gl, shader);
        ret = program_check_status(gl, shader, GL_COMPILE_STATUS);
        if (ret < 0)
            goto fail;
        ngli_glAttachShader(gl, s->id, shader);
    }

    ngli_glLinkProgram(gl, s->id);
    ret = program_check_status(gl, s->id, GL_LINK_STATUS);
    if (ret < 0)
        goto fail;

    for (int i = 0; i < NGLI_ARRAY_NB(shaders); i++)
        ngli_glDeleteShader(gl, shaders[i].id);

    s->uniforms = program_probe_uniforms(gl, s->id);
    s->attributes = program_probe_attributes(gl, s->id);
    s->buffer_blocks = program_probe_buffer_blocks(gl, s->id);
    if (!s->uniforms || !s->attributes || !s->buffer_blocks) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    return 0;

fail:
    for (int i = 0; i < NGLI_ARRAY_NB(shaders); i++)
        ngli_glDeleteShader(gl, shaders[i].id);

    return ret;
}

void ngli_program_reset(struct program *s)
{
    if (!s->ctx)
        return;
    ngli_hmap_freep(&s->uniforms);
    ngli_hmap_freep(&s->attributes);
    ngli_hmap_freep(&s->buffer_blocks);
    struct glcontext *gl = s->ctx->glcontext;
    ngli_glDeleteProgram(gl, s->id);
    memset(s, 0, sizeof(*s));
}
