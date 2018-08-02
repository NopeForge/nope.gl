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

#ifdef TARGET_ANDROID
static const char default_fragment_shader[] =
    "#version 100"                                                                      "\n"
    "#extension GL_OES_EGL_image_external : require"                                    "\n"
    ""                                                                                  "\n"
    "precision highp float;"                                                            "\n"
    "uniform int tex0_sampling_mode;"                                                   "\n"
    "uniform sampler2D tex0_sampler;"                                                   "\n"
    "uniform samplerExternalOES tex0_external_sampler;"                                 "\n"
    "varying vec2 var_uvcoord;"                                                         "\n"
    "varying vec2 var_tex0_coord;"                                                      "\n"
    "void main(void)"                                                                   "\n"
    "{"                                                                                 "\n"
    "    if (tex0_sampling_mode == 1)"                                                  "\n"
    "        gl_FragColor = texture2D(tex0_sampler, var_tex0_coord);"                   "\n"
    "    else if (tex0_sampling_mode == 2)"                                             "\n"
    "        gl_FragColor = texture2D(tex0_external_sampler, var_tex0_coord);"          "\n"
    "}";
#else
static const char default_fragment_shader[] =
    "#version 100"                                                                      "\n"
    ""                                                                                  "\n"
    "precision highp float;"                                                            "\n"
    "uniform sampler2D tex0_sampler;"                                                   "\n"
    "varying vec2 var_uvcoord;"                                                         "\n"
    "varying vec2 var_tex0_coord;"                                                      "\n"
    "void main(void)"                                                                   "\n"
    "{"                                                                                 "\n"
    "    gl_FragColor = texture2D(tex0_sampler, var_tex0_coord);"                       "\n"
    "}";
#endif

static const char default_vertex_shader[] =
    "#version 100"                                                                      "\n"
    ""                                                                                  "\n"
    "precision highp float;"                                                            "\n"
    "attribute vec4 ngl_position;"                                                      "\n"
    "attribute vec2 ngl_uvcoord;"                                                       "\n"
    "attribute vec3 ngl_normal;"                                                        "\n"
    "uniform mat4 ngl_modelview_matrix;"                                                "\n"
    "uniform mat4 ngl_projection_matrix;"                                               "\n"
    "uniform mat3 ngl_normal_matrix;"                                                   "\n"

    "uniform mat4 tex0_coord_matrix;"                                                   "\n"
    "uniform vec2 tex0_dimensions;"                                                     "\n"

    "varying vec2 var_uvcoord;"                                                         "\n"
    "varying vec3 var_normal;"                                                          "\n"
    "varying vec2 var_tex0_coord;"                                                      "\n"
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;"    "\n"
    "    var_uvcoord = ngl_uvcoord;"                                                    "\n"
    "    var_normal = ngl_normal_matrix * ngl_normal;"                                  "\n"
    "    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0, 1)).xy;"            "\n"
    "}";

#define OFFSET(x) offsetof(struct program, x)
static const struct node_param program_params[] = {
    {"vertex",   PARAM_TYPE_STR, OFFSET(vertex),   {.str=default_vertex_shader},
                 .desc=NGLI_DOCSTRING("vertex shader")},
    {"fragment", PARAM_TYPE_STR, OFFSET(fragment), {.str=default_fragment_shader},
                 .desc=NGLI_DOCSTRING("fragment shader")},
    {NULL}
};

#define DEFINE_GET_INFO_LOG_FUNCTION(func, name)                                      \
static void get_##func##_info_log(const struct glcontext *gl, GLuint id,              \
                                  char **info_logp, int *info_log_lengthp)            \
{                                                                                     \
    ngli_glGet##name##iv(gl, id, GL_INFO_LOG_LENGTH, info_log_lengthp);               \
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
    ngli_glGet##name##InfoLog(gl, id, *info_log_lengthp, NULL, *info_logp);           \
    while (*info_log_lengthp && strchr(" \r\n", (*info_logp)[*info_log_lengthp - 1])) \
        (*info_logp)[--*info_log_lengthp] = 0;                                        \
}                                                                                     \

DEFINE_GET_INFO_LOG_FUNCTION(shader, Shader)
DEFINE_GET_INFO_LOG_FUNCTION(program, Program)

static GLuint load_program(struct ngl_node *node, const char *vertex, const char *fragment)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    char *info_log = NULL;
    int info_log_length = 0;

    GLint result = GL_FALSE;

    GLuint program = ngli_glCreateProgram(gl);
    GLuint vertex_shader = ngli_glCreateShader(gl, GL_VERTEX_SHADER);
    GLuint fragment_shader = ngli_glCreateShader(gl, GL_FRAGMENT_SHADER);

    ngli_glShaderSource(gl, vertex_shader, 1, &vertex, NULL);
    ngli_glCompileShader(gl, vertex_shader);

    ngli_glGetShaderiv(gl, vertex_shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        get_shader_info_log(gl, vertex_shader, &info_log, &info_log_length);
        goto fail;
    }

    ngli_glShaderSource(gl, fragment_shader, 1, &fragment, NULL);
    ngli_glCompileShader(gl, fragment_shader);

    ngli_glGetShaderiv(gl, fragment_shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        get_shader_info_log(gl, fragment_shader, &info_log, &info_log_length);
        goto fail;
    }

    ngli_glAttachShader(gl, program, vertex_shader);
    ngli_glAttachShader(gl, program, fragment_shader);
    ngli_glLinkProgram(gl, program);

    ngli_glGetProgramiv(gl, program, GL_LINK_STATUS, &result);
    if (!result) {
        get_program_info_log(gl, program, &info_log, &info_log_length);
        goto fail;
    }

    ngli_glDeleteShader(gl, vertex_shader);
    ngli_glDeleteShader(gl, fragment_shader);

    return program;

fail:
    if (info_log) {
        LOG(ERROR, "could not compile or link shader: %s", info_log);
        free(info_log);
    }

    if (vertex_shader) {
        ngli_glDeleteShader(gl, vertex_shader);
    }

    if (fragment_shader) {
        ngli_glDeleteShader(gl, fragment_shader);
    }

    if (program) {
        ngli_glDeleteProgram(gl, program);
    }

    return 0;
}

static void free_pinfo(void *user_arg, void *data)
{
    free(data);
}

static int program_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct program *s = node->priv_data;

    s->info.program_id = load_program(node, s->vertex, s->fragment);
    if (!s->info.program_id)
        return -1;

    s->info.active_uniforms = ngli_hmap_create();
    if (!s->info.active_uniforms)
        return -1;
    ngli_hmap_set_free(s->info.active_uniforms, free_pinfo, NULL);

    s->active_attributes = ngli_hmap_create();
    if (!s->active_attributes)
        return -1;
    ngli_hmap_set_free(s->active_attributes, free_pinfo, NULL);

    s->position_location_id          = ngli_glGetAttribLocation(gl, s->info.program_id,  "ngl_position");
    s->uvcoord_location_id           = ngli_glGetAttribLocation(gl, s->info.program_id,  "ngl_uvcoord");
    s->normal_location_id            = ngli_glGetAttribLocation(gl, s->info.program_id,  "ngl_normal");
    s->modelview_matrix_location_id  = ngli_glGetUniformLocation(gl, s->info.program_id, "ngl_modelview_matrix");
    s->projection_matrix_location_id = ngli_glGetUniformLocation(gl, s->info.program_id, "ngl_projection_matrix");
    s->normal_matrix_location_id     = ngli_glGetUniformLocation(gl, s->info.program_id, "ngl_normal_matrix");

    int nb_active_uniforms;
    ngli_glGetProgramiv(gl, s->info.program_id, GL_ACTIVE_UNIFORMS, &nb_active_uniforms);
    for (int i = 0; i < nb_active_uniforms; i++) {
        struct uniformprograminfo *info = malloc(sizeof(*info));
        if (!info)
            return -1;
        ngli_glGetActiveUniform(gl,
                                s->info.program_id,
                                i,
                                sizeof(info->name),
                                NULL,
                                &info->size,
                                &info->type,
                                info->name);

        /* Remove [0] suffix from names of uniform arrays */
        info->name[strcspn(info->name, "[")] = 0;

        info->id = ngli_glGetUniformLocation(gl,
                                             s->info.program_id,
                                             info->name);

        if (info->type == GL_IMAGE_2D) {
            ngli_glGetUniformiv(gl, s->info.program_id, info->id, &info->binding);
        } else {
            info->binding = -1;
        }

        LOG(DEBUG, "%s.uniform[%d/%d]: %s location:%d size=%d type=0x%x binding=%d", node->name,
            i + 1, nb_active_uniforms, info->name, info->id, info->size, info->type, info->binding);

        int ret = ngli_hmap_set(s->info.active_uniforms, info->name, info);
        if (ret < 0)
            return ret;
    }

    int nb_active_attributes;
    ngli_glGetProgramiv(gl, s->info.program_id, GL_ACTIVE_ATTRIBUTES, &nb_active_attributes);
    for (int i = 0; i < nb_active_attributes; i++) {
        struct attributeprograminfo *info = malloc(sizeof(*info));
        if (!info)
            return -1;
        ngli_glGetActiveAttrib(gl,
                               s->info.program_id,
                               i,
                               sizeof(info->name),
                               NULL,
                               &info->size,
                               &info->type,
                               info->name);

        info->id = ngli_glGetAttribLocation(gl,
                                            s->info.program_id,
                                            info->name);
        LOG(DEBUG, "%s.attribute[%d/%d]: %s location:%d size=%d type=0x%x", node->name,
            i + 1, nb_active_attributes, info->name, info->id, info->size, info->type);

        int ret = ngli_hmap_set(s->active_attributes, info->name, info);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void program_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct program *s = node->priv_data;

    ngli_hmap_freep(&s->info.active_uniforms);
    ngli_hmap_freep(&s->active_attributes);
    ngli_glDeleteProgram(gl, s->info.program_id);
}

const struct node_class ngli_program_class = {
    .id        = NGL_NODE_PROGRAM,
    .name      = "Program",
    .init      = program_init,
    .uninit    = program_uninit,
    .priv_size = sizeof(struct program),
    .params    = program_params,
    .file      = __FILE__,
};
