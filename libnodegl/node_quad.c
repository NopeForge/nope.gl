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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "glincludes.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct shape, x)
static const struct node_param quad_params[] = {
    {"corner",    PARAM_TYPE_VEC3, OFFSET(quad_corner),    {.vec={-0.5f, -0.5f}}},
    {"width",     PARAM_TYPE_VEC3, OFFSET(quad_width),     {.vec={ 1.0f,  0.0f}}},
    {"height",    PARAM_TYPE_VEC3, OFFSET(quad_height),    {.vec={ 0.0f,  1.0f}}},
    {"uv_corner", PARAM_TYPE_VEC2, OFFSET(quad_uv_corner), {.vec={0.0f, 0.0f}}},
    {"uv_width",  PARAM_TYPE_VEC2, OFFSET(quad_uv_width),  {.vec={1.0f, 0.0f}}},
    {"uv_height", PARAM_TYPE_VEC2, OFFSET(quad_uv_height), {.vec={0.0f, 1.0f}}},
    {NULL}
};

#define NB_VERTICES 4

#define C(index) s->quad_corner[(index)]
#define W(index) s->quad_width[(index)]
#define H(index) s->quad_height[(index)]

#define UV_C(index) s->quad_uv_corner[(index)]
#define UV_W(index) s->quad_uv_width[(index)]
#define UV_H(index) s->quad_uv_height[(index)]

static int quad_init(struct ngl_node *node)
{
    struct shape *s = node->priv_data;

    const GLfloat vertices[NB_VERTICES*NGLI_SHAPE_COORDS_NB] = {
        C(0),               C(1),               C(2),
        C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
        C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
        C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
    };

    const GLfloat uvs[NB_VERTICES*NGLI_SHAPE_TEXCOORDS_NB] = {
        UV_C(0),                     1.0f - UV_C(1),
        UV_C(0) + UV_W(0),           1.0f - UV_C(1) - UV_W(1),
        UV_C(0) + UV_H(0) + UV_W(0), 1.0f - UV_C(1) - UV_H(1) - UV_W(1),
        UV_C(0) + UV_H(0),           1.0f - UV_C(1) - UV_H(1),
    };

    float normal[3];
    ngli_vec3_cross(normal, s->quad_width, s->quad_height);
    ngli_vec3_norm(normal, normal);

    s->nb_vertices = NB_VERTICES;
    s->vertices = calloc(1, NGLI_SHAPE_VERTICES_SIZE(s));
    if (!s->vertices)
        return -1;

    float *dst = s->vertices;
    const float *y = vertices;
    const float *uv = uvs;

    for (int i = 0; i < NB_VERTICES; i++) {
        memcpy(dst, y, sizeof(*s->vertices) * NGLI_SHAPE_COORDS_NB);
        y   += NGLI_SHAPE_COORDS_NB;
        dst += NGLI_SHAPE_COORDS_NB;

        memcpy(dst, uv, sizeof(*s->vertices) * NGLI_SHAPE_TEXCOORDS_NB);
        uv  += NGLI_SHAPE_TEXCOORDS_NB;
        dst += NGLI_SHAPE_TEXCOORDS_NB;

        memcpy(dst, normal, sizeof(normal));
        dst += NGLI_SHAPE_NORMALS_NB;
    }

    static const GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
    s->indice_size = sizeof(*indices);
    s->nb_indices = NGLI_ARRAY_NB(indices);
    s->indices = calloc(1, sizeof(indices));
    if (!s->indices)
        return -1;
    memcpy(s->indices, indices, sizeof(indices));

    ngli_shape_generate_buffers(node);

    s->draw_mode = GL_TRIANGLES;
    s->draw_type = GL_UNSIGNED_SHORT;

    return 0;
}

static void quad_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct shape *s = node->priv_data;

    ngli_glDeleteBuffers(gl, 1, &s->vertices_buffer_id);
    ngli_glDeleteBuffers(gl, 1, &s->texcoords_buffer_id);
    ngli_glDeleteBuffers(gl, 1, &s->normals_buffer_id);
    ngli_glDeleteBuffers(gl, 1, &s->indices_buffer_id);

    free(s->vertices);
    free(s->indices);
}

const struct node_class ngli_quad_class = {
    .id        = NGL_NODE_QUAD,
    .name      = "Quad",
    .init      = quad_init,
    .uninit    = quad_uninit,
    .priv_size = sizeof(struct shape),
    .params    = quad_params,
};
