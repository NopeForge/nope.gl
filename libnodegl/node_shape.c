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
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct shape, x)
static const struct node_param shape_params[] = {
    {"primitives", PARAM_TYPE_NODELIST, OFFSET(primitives), .node_types=(const int[]){NGL_NODE_SHAPEPRIMITIVE, -1}},
    {"draw_mode", PARAM_TYPE_INT, OFFSET(draw_mode), {.i64=GL_TRIANGLES}},
    {"draw_type", PARAM_TYPE_INT, OFFSET(draw_type), {.i64=GL_UNSIGNED_SHORT}},
    {NULL}
};

static int shape_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct shape *s = node->priv_data;

    s->nb_vertices = s->nb_primitives;
    s->vertices = calloc(s->nb_vertices, NGLI_SHAPE_VERTICES_STRIDE(s));
    if (!s->vertices)
        return -1;

    GLfloat *p = s->vertices;
    for (int i = 0; i < s->nb_vertices; i++) {
        const struct shapeprimitive *primitive = s->primitives[i]->priv_data;

        memcpy(p, primitive->coordinates, sizeof(primitive->coordinates));
        p += NGLI_ARRAY_NB(primitive->coordinates);
        memcpy(p, primitive->texture_coordinates, sizeof(primitive->texture_coordinates));
        p += NGLI_ARRAY_NB(primitive->texture_coordinates);
        memcpy(p, primitive->normals, sizeof(primitive->normals));
        p += NGLI_ARRAY_NB(primitive->normals);
    }

    NGLI_SHAPE_GENERATE_BUFFERS(gl, s);

    s->nb_indices = s->nb_primitives;
    s->indices = calloc(s->nb_primitives, sizeof(*s->indices));
    if (!s->indices)
        return -1;

    for (int i = 0; i < s->nb_primitives; i++) {
        s->indices[i] = i;
    }

    NGLI_SHAPE_GENERATE_ELEMENT_BUFFERS(gl, s);

    return 0;
}

static void shape_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct shape *s = node->priv_data;

    gl->DeleteBuffers(1, &s->vertices_buffer_id);
    gl->DeleteBuffers(1, &s->texcoords_buffer_id);
    gl->DeleteBuffers(1, &s->normals_buffer_id);
    gl->DeleteBuffers(1, &s->indices_buffer_id);

    free(s->vertices);
    free(s->indices);
}

const struct node_class ngli_shape_class = {
    .id        = NGL_NODE_SHAPE,
    .name      = "Shape",
    .init      = shape_init,
    .uninit    = shape_uninit,
    .priv_size = sizeof(struct shape),
    .params    = shape_params,
};
