/*
 * Copyright 2017 GoPro Inc.
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

#define OFFSET(x) offsetof(struct program, x)
static const struct node_param computeprogram_params[] = {
    {"compute", PARAM_TYPE_STR, OFFSET(compute), .flags=PARAM_FLAG_CONSTRUCTOR,
                .desc=NGLI_DOCSTRING("compute shader")},
    {NULL}
};

static GLuint load_shader(struct ngl_node *node, const char *compute_shader_data)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    GLuint program = ngli_glCreateProgram(gl);
    GLuint compute_shader = ngli_glCreateShader(gl, GL_COMPUTE_SHADER);

    ngli_glShaderSource(gl, compute_shader, 1, &compute_shader_data, NULL);
    ngli_glCompileShader(gl, compute_shader);
    if (ngli_program_check_status(gl, compute_shader, GL_COMPILE_STATUS) < 0)
        goto fail;

    ngli_glAttachShader(gl, program, compute_shader);
    ngli_glLinkProgram(gl, program);
    if (ngli_program_check_status(gl, program, GL_LINK_STATUS) < 0)
        goto fail;

    ngli_glDeleteShader(gl, compute_shader);

    return program;

fail:
    if (compute_shader)
        ngli_glDeleteShader(gl, compute_shader);
    if (program)
        ngli_glDeleteProgram(gl, program);

    return 0;
}

static int computeprogram_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct program *s = node->priv_data;

    if (!(gl->features & NGLI_FEATURE_COMPUTE_SHADER_ALL)) {
        LOG(ERROR, "context does not support compute shaders");
        return -1;
    }

    s->program_id = load_shader(node, s->compute);
    if (!s->program_id)
        return -1;

    s->active_uniforms = ngli_program_probe_uniforms(node->name, gl, s->program_id);
    s->active_buffer_blocks = ngli_program_probe_buffer_blocks(node->name, gl, s->program_id);
    if (!s->active_uniforms || !s->active_buffer_blocks)
        return -1;

    return 0;
}

static void computeprogram_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct program *s = node->priv_data;

    ngli_hmap_freep(&s->active_uniforms);
    ngli_hmap_freep(&s->active_buffer_blocks);
    ngli_glDeleteProgram(gl, s->program_id);
}

const struct node_class ngli_computeprogram_class = {
    .id        = NGL_NODE_COMPUTEPROGRAM,
    .name      = "ComputeProgram",
    .init      = computeprogram_init,
    .uninit    = computeprogram_uninit,
    .priv_size = sizeof(struct program),
    .params    = computeprogram_params,
    .file      = __FILE__,
};
