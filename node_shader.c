/*
 * Copyright 2016 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "bstr.h"
#include "glincludes.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

static const char default_fragment_shader_data[] =
    "#version 100"                                                                      "\n"
#ifdef __ANDROID__
    "#extension GL_OES_EGL_image_external : require"                                    "\n"
#endif
    ""                                                                                  "\n"
    "precision mediump float;"                                                          "\n"
    "uniform sampler2D tex0_sampler;"                                                   "\n"
#ifdef __ANDROID__
    "uniform samplerExternalOES tex0_external_sampler;"                                 "\n"
#endif
    "varying vec2 var_tex0_coords;"                                                     "\n"
    "void main(void)"                                                                   "\n"
    "{"                                                                                 "\n"
    "    vec4 t;"                                                                       "\n"
    "    t  = texture2D(tex0_sampler, var_tex0_coords);"                                "\n"
#ifdef __ANDROID__
    "    t += texture2D(tex0_external_sampler, var_tex0_coords);"                       "\n"
#endif
    "    gl_FragColor = vec4(t.rgb, 1.0);"                                              "\n"
    "}";

static const char default_vertex_shader_data[] =
    "#version 100"                                                                      "\n"
    "attribute vec4 ngl_position;"                                                      "\n"
    "attribute vec3 ngl_normal;"                                                        "\n"
    "uniform mat4 ngl_modelview_matrix;"                                                "\n"
    "uniform mat4 ngl_projection_matrix;"                                               "\n"
    "uniform mat3 ngl_normal_matrix;"                                                   "\n"

    "attribute vec2 tex0_coords;"                                                       "\n"
    "uniform mat4 tex0_coords_matrix;"                                                  "\n"
    "uniform vec2 tex0_dimensions;"                                                     "\n"

    "varying vec2 var_tex0_coords;"                                                     "\n"
    "varying vec3 var_normal;"                                                          "\n"
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;"    "\n"
    "    var_tex0_coords = (tex0_coords_matrix * vec4(tex0_coords, 0, 1)).xy;"          "\n"
    "    var_normal = ngl_normal_matrix * ngl_normal;"                                  "\n"
    "}";

#define OFFSET(x) offsetof(struct shader, x)
static const struct node_param shader_params[] = {
    {"vertex_data",   PARAM_TYPE_STR, OFFSET(vertex_data),   {.str=default_vertex_shader_data}},
    {"fragment_data", PARAM_TYPE_STR, OFFSET(fragment_data), {.str=default_fragment_shader_data}},
    {NULL}
};

#define DEFINE_GET_INFO_LOG_FUNCTION(func, name)                                      \
static void get_##func##_info_log(const struct glfunctions *gl, GLuint id,            \
                                  char **info_logp, int *info_log_lengthp)            \
{                                                                                     \
    gl->Get##name##iv(id, GL_INFO_LOG_LENGTH, info_log_lengthp);                      \
    if (!*info_log_lengthp) {                                                         \
        *info_logp = NULL;                                                            \
        return;                                                                       \
    }                                                                                 \
                                                                                      \
    *info_logp = malloc(*info_log_lengthp);                                           \
    if (!*info_logp) {                                                                \
        *info_log_lengthp = 0;                                                        \
        return;                                                                       \
    }                                                                                 \
                                                                                      \
    gl->Get##name##InfoLog(id, *info_log_lengthp, NULL, *info_logp);                  \
    while (*info_log_lengthp && strchr(" \r\n", (*info_logp)[*info_log_lengthp - 1])) \
        (*info_logp)[--*info_log_lengthp] = 0;                                        \
}                                                                                     \

DEFINE_GET_INFO_LOG_FUNCTION(shader, Shader)
DEFINE_GET_INFO_LOG_FUNCTION(program, Program)

static GLuint load_shader(struct ngl_node *node, const char *vertex_shader_data, const char *fragment_shader_data)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    char *info_log = NULL;
    int info_log_length = 0;

    GLint result = GL_FALSE;

    GLuint program = gl->CreateProgram();
    GLuint vertex_shader = gl->CreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = gl->CreateShader(GL_FRAGMENT_SHADER);

    gl->ShaderSource(vertex_shader, 1, &vertex_shader_data, NULL);
    gl->CompileShader(vertex_shader);

    gl->GetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        get_shader_info_log(gl, vertex_shader, &info_log, &info_log_length);
        goto fail;
    }

    gl->ShaderSource(fragment_shader, 1, &fragment_shader_data, NULL);
    gl->CompileShader(fragment_shader);

    gl->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        get_shader_info_log(gl, fragment_shader, &info_log, &info_log_length);
        goto fail;
    }

    gl->AttachShader(program, vertex_shader);
    gl->AttachShader(program, fragment_shader);
    gl->LinkProgram(program);

    gl->GetProgramiv(program, GL_LINK_STATUS, &result);
    if (!result) {
        get_program_info_log(gl, program, &info_log, &info_log_length);
        goto fail;
    }

    gl->DeleteShader(vertex_shader);
    gl->DeleteShader(fragment_shader);

    return program;

fail:
    if (info_log) {
        LOG(ERROR, "could not compile or link shader: %s", info_log);
        free(info_log);
    }

    if (vertex_shader) {
        gl->DeleteShader(vertex_shader);
    }

    if (fragment_shader) {
        gl->DeleteShader(fragment_shader);
    }

    if (program) {
        gl->DeleteProgram(program);
    }

    return 0;
}

static int shader_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct shader *s = node->priv_data;

    s->program_id = load_shader(node, s->vertex_data, s->fragment_data);
    if (!s->program_id)
        return -1;

    s->position_location_id          = gl->GetAttribLocation(s->program_id,  "ngl_position");
    s->normal_location_id            = gl->GetAttribLocation(s->program_id,  "ngl_normal");
    s->modelview_matrix_location_id  = gl->GetUniformLocation(s->program_id, "ngl_modelview_matrix");
    s->projection_matrix_location_id = gl->GetUniformLocation(s->program_id, "ngl_projection_matrix");
    s->normal_matrix_location_id     = gl->GetUniformLocation(s->program_id, "ngl_normal_matrix");

    return 0;
}

static void shader_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct shader *s = node->priv_data;

    gl->DeleteProgram(s->program_id);
}

const struct node_class ngli_shader_class = {
    .id        = NGL_NODE_SHADER,
    .name      = "Shader",
    .init      = shader_init,
    .uninit    = shader_uninit,
    .priv_size = sizeof(struct shader),
    .params    = shader_params,
};
