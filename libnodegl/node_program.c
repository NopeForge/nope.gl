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
#include "program.h"

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

static GLuint load_program(struct ngl_node *node, const char *vertex, const char *fragment)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    GLuint program = ngli_glCreateProgram(gl);
    GLuint vertex_shader = ngli_glCreateShader(gl, GL_VERTEX_SHADER);
    GLuint fragment_shader = ngli_glCreateShader(gl, GL_FRAGMENT_SHADER);

    ngli_glShaderSource(gl, vertex_shader, 1, &vertex, NULL);
    ngli_glCompileShader(gl, vertex_shader);
    if (ngli_program_check_status(gl, vertex_shader, GL_COMPILE_STATUS) < 0)
        goto fail;

    ngli_glShaderSource(gl, fragment_shader, 1, &fragment, NULL);
    ngli_glCompileShader(gl, fragment_shader);
    if (ngli_program_check_status(gl, fragment_shader, GL_COMPILE_STATUS) < 0)
        goto fail;

    ngli_glAttachShader(gl, program, vertex_shader);
    ngli_glAttachShader(gl, program, fragment_shader);
    ngli_glLinkProgram(gl, program);
    if (ngli_program_check_status(gl, program, GL_LINK_STATUS) < 0)
        goto fail;

    ngli_glDeleteShader(gl, vertex_shader);
    ngli_glDeleteShader(gl, fragment_shader);

    return program;

fail:
    if (vertex_shader)
        ngli_glDeleteShader(gl, vertex_shader);
    if (fragment_shader)
        ngli_glDeleteShader(gl, fragment_shader);
    if (program)
        ngli_glDeleteProgram(gl, program);

    return 0;
}

static int program_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct program *s = node->priv_data;

    s->info.program_id = load_program(node, s->vertex, s->fragment);
    if (!s->info.program_id)
        return -1;

    s->info.active_uniforms = ngli_program_probe_uniforms(node->name, gl, s->info.program_id);
    s->active_attributes    = ngli_program_probe_attributes(node->name, gl, s->info.program_id);
    if (!s->info.active_uniforms || !s->active_attributes)
        return -1;

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
