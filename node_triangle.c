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
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct shape, x)
static const struct node_param triangle_params[] = {
    {"edge0", PARAM_TYPE_VEC3, OFFSET(triangle_edges[0]), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"edge1", PARAM_TYPE_VEC3, OFFSET(triangle_edges[3]), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"edge2", PARAM_TYPE_VEC3, OFFSET(triangle_edges[6]), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"uv_edge0", PARAM_TYPE_VEC2, OFFSET(triangle_uvs[0]), {.vec={0.0f, 0.0f}}},
    {"uv_edge1", PARAM_TYPE_VEC2, OFFSET(triangle_uvs[2]), {.vec={0.0f, 1.0f}}},
    {"uv_edge2", PARAM_TYPE_VEC2, OFFSET(triangle_uvs[4]), {.vec={1.0f, 1.0f}}},
    {NULL}
};

#define NB_VERTICES 3

static int triangle_init(struct ngl_node *node)
{
    struct shape *s = node->priv_data;

    s->nb_vertices = NB_VERTICES;
    s->vertices = calloc(1, NGLI_SHAPE_VERTICES_SIZE(s));
    if (!s->vertices)
        return -1;

    float a[3];
    float b[3];
    float normal[3];
    ngli_vec3_sub(a, s->triangle_edges + 1 * NGLI_SHAPE_COORDS_NB, s->triangle_edges);
    ngli_vec3_sub(b, s->triangle_edges + 2 * NGLI_SHAPE_COORDS_NB, s->triangle_edges);
    ngli_vec3_cross(normal, a, b);
    ngli_vec3_norm(normal, normal);

    float *dst = s->vertices;
    const float *y = s->triangle_edges;
    const float *uv = s->triangle_uvs;

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

    NGLI_SHAPE_GENERATE_BUFFERS(s);

    static const GLushort indices[] = { 0, 1, 2 };
    s->nb_indices = NGLI_ARRAY_NB(indices);
    s->indices = calloc(1, sizeof(indices));
    if (!s->indices)
        return -1;

    memcpy(s->indices, indices, sizeof(indices));
    NGLI_SHAPE_GENERATE_ELEMENT_BUFFERS(s);

    s->draw_mode = GL_TRIANGLES;
    s->draw_type = GL_UNSIGNED_SHORT;

    return 0;
}

static void triangle_uninit(struct ngl_node *node)
{
    struct shape *s = node->priv_data;

    free(s->vertices);
    free(s->indices);
}

const struct node_class ngli_triangle_class = {
    .id        = NGL_NODE_TRIANGLE,
    .name      = "Triangle",
    .init      = triangle_init,
    .uninit    = triangle_uninit,
    .priv_size = sizeof(struct shape),
    .params    = triangle_params,
};
