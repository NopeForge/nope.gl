/*
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

#include <stdlib.h>
#include <string.h>

#include "ctx_gl.h"
#include "glincludes.h"
#include "log.h"
#include "ngpu/type.h"
#include "program_gl.h"
#include "utils/bstr.h"
#include "utils/memory.h"
#include "utils/string.h"

static int program_check_status(const struct glcontext *gl, GLuint id, GLenum status)
{
    char *info_log = NULL;
    int info_log_length = 0;

    void (NGLI_GL_APIENTRY *get_info)(GLuint id, GLenum pname, GLint *params);
    void (NGLI_GL_APIENTRY *get_log)(GLuint id, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    const char *type_str;

    if (status == GL_COMPILE_STATUS) {
        type_str = "compile";
        get_info = gl->funcs.GetShaderiv;
        get_log  = gl->funcs.GetShaderInfoLog;
    } else if (status == GL_LINK_STATUS) {
        type_str = "link";
        get_info = gl->funcs.GetProgramiv;
        get_log  = gl->funcs.GetProgramInfoLog;
    } else {
        ngli_assert(0);
    }

    GLint result = GL_FALSE;
    get_info(id, status, &result);
    if (result == GL_TRUE)
        return 0;

    get_info(id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (!info_log_length)
        return NGL_ERROR_BUG;

    info_log = ngli_malloc(info_log_length);
    if (!info_log)
        return NGL_ERROR_MEMORY;

    get_log(id, info_log_length, NULL, info_log);
    while (info_log_length && strchr(" \r\n", info_log[info_log_length - 1]))
        info_log_length--;

    LOG(ERROR, "could not %s shader: %.*s", type_str, info_log_length, info_log);
    ngli_free(info_log);
    return NGL_ERROR_INVALID_DATA;
}

struct ngpu_program *ngpu_program_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_program_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_program *)s;
}

int ngpu_program_gl_init(struct ngpu_program *s, const struct ngpu_program_params *params)
{
    struct ngpu_program_gl *s_priv = (struct ngpu_program_gl *)s;

    int ret = 0;
    struct {
        const char *name;
        GLenum type;
        const char *src;
        GLuint id;
    } shaders[] = {
        [NGPU_PROGRAM_SHADER_VERT] = {"vertex", GL_VERTEX_SHADER, params->vertex, 0},
        [NGPU_PROGRAM_SHADER_FRAG] = {"fragment", GL_FRAGMENT_SHADER, params->fragment, 0},
        [NGPU_PROGRAM_SHADER_COMP] = {"compute", GL_COMPUTE_SHADER, params->compute, 0},
    };

    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    s_priv->id = gl->funcs.CreateProgram();

    for (size_t i = 0; i < NGLI_ARRAY_NB(shaders); i++) {
        if (!shaders[i].src)
            continue;
        GLuint shader = gl->funcs.CreateShader(shaders[i].type);
        shaders[i].id = shader;
        gl->funcs.ShaderSource(shader, 1, &shaders[i].src, NULL);
        gl->funcs.CompileShader(shader);
        ret = program_check_status(gl, shader, GL_COMPILE_STATUS);
        if (ret < 0) {
            char *s_with_numbers = ngli_numbered_lines(shaders[i].src);
            if (s_with_numbers) {
                LOG(ERROR, "failed to compile shader \"%s\":\n%s",
                    params->label ? params->label : "", s_with_numbers);
                ngli_free(s_with_numbers);
            }
            goto fail;
        }
        gl->funcs.AttachShader(s_priv->id, shader);
    }

    gl->funcs.LinkProgram(s_priv->id);
    ret = program_check_status(gl, s_priv->id, GL_LINK_STATUS);
    if (ret < 0) {
        struct bstr *bstr = ngli_bstr_create();
        if (bstr) {
            ngli_bstr_printf(bstr, "failed to link shaders \"%s\":",
                             params->label ? params->label : "");
            for (size_t i = 0; i < NGLI_ARRAY_NB(shaders); i++) {
                if (!shaders[i].src)
                    continue;
                char *s_with_numbers = ngli_numbered_lines(shaders[i].src);
                if (s_with_numbers) {
                    ngli_bstr_printf(bstr, "\n\n%s shader:\n%s", shaders[i].name, s_with_numbers);
                    ngli_free(s_with_numbers);
                }
            }
            LOG(ERROR, "%s", ngli_bstr_strptr(bstr));
            ngli_bstr_freep(&bstr);
        }
        goto fail;
    }

    for (size_t i = 0; i < NGLI_ARRAY_NB(shaders); i++)
        gl->funcs.DeleteShader(shaders[i].id);

    return 0;

fail:
    for (size_t i = 0; i < NGLI_ARRAY_NB(shaders); i++)
        gl->funcs.DeleteShader(shaders[i].id);

    return ret;
}

void ngpu_program_gl_freep(struct ngpu_program **sp)
{
    if (!*sp)
        return;
    struct ngpu_program *s = *sp;
    struct ngpu_program_gl *s_priv = (struct ngpu_program_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    gl->funcs.DeleteProgram(s_priv->id);
    ngli_freep(sp);
}
