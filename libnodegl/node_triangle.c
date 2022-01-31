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

#include "geometry.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "internal.h"
#include "topology.h"
#include "utils.h"

struct triangle_opts {
    float edges[9];
    float uvs[6];
};

struct triangle_priv {
    struct geometry geom;
    struct triangle_opts opts;
};

#define OFFSET(x) offsetof(struct triangle_priv, opts.x)
static const struct node_param triangle_params[] = {
    {"edge0", NGLI_PARAM_TYPE_VEC3, OFFSET(edges[0]),
              {.vec={1.0, -1.0, 0.0}},
              .desc=NGLI_DOCSTRING("first edge coordinate of the triangle")},
    {"edge1", NGLI_PARAM_TYPE_VEC3, OFFSET(edges[3]),
              {.vec={0.0, 1.0, 0.0}},
              .desc=NGLI_DOCSTRING("second edge coordinate of the triangle")},
    {"edge2", NGLI_PARAM_TYPE_VEC3, OFFSET(edges[6]),
              {.vec={-1.0, -1.0, 0.0}},
             .desc=NGLI_DOCSTRING("third edge coordinate of the triangle")},
    {"uv_edge0", NGLI_PARAM_TYPE_VEC2, OFFSET(uvs[0]), {.vec={0.0f, 0.0f}},
                 .desc=NGLI_DOCSTRING("UV coordinate associated with `edge0`")},
    {"uv_edge1", NGLI_PARAM_TYPE_VEC2, OFFSET(uvs[2]), {.vec={0.0f, 1.0f}},
                 .desc=NGLI_DOCSTRING("UV coordinate associated with `edge1`")},
    {"uv_edge2", NGLI_PARAM_TYPE_VEC2, OFFSET(uvs[4]), {.vec={1.0f, 1.0f}},
                 .desc=NGLI_DOCSTRING("UV coordinate associated with `edge2`")},
    {NULL}
};

NGLI_STATIC_ASSERT(geom_on_top_of_triangle, offsetof(struct triangle_priv, geom) == 0);

#define NB_VERTICES 3

static int triangle_init(struct ngl_node *node)
{
    struct triangle_priv *s = node->priv_data;
    const struct triangle_opts *o = &s->opts;

    float normals[3 * NB_VERTICES];
    ngli_vec3_normalvec(normals, o->edges, o->edges + 3, o->edges + 6);

    for (int i = 1; i < NB_VERTICES; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    struct gpu_ctx *gpu_ctx = node->ctx->gpu_ctx;

    int ret;
    if ((ret = ngli_geometry_gen_vec3(&s->geom.vertices_buffer, &s->geom.vertices_layout, gpu_ctx, NB_VERTICES, o->edges)) < 0 ||
        (ret = ngli_geometry_gen_vec2(&s->geom.uvcoords_buffer, &s->geom.uvcoords_layout, gpu_ctx, NB_VERTICES, o->uvs))   < 0 ||
        (ret = ngli_geometry_gen_vec3(&s->geom.normals_buffer,  &s->geom.normals_layout,  gpu_ctx, NB_VERTICES, normals))  < 0)
        return 0;

    s->geom.topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    return 0;
}

static void triangle_uninit(struct ngl_node *node)
{
    struct triangle_priv *s = node->priv_data;

    ngli_buffer_freep(&s->geom.vertices_buffer);
    ngli_buffer_freep(&s->geom.uvcoords_buffer);
    ngli_buffer_freep(&s->geom.normals_buffer);
}

const struct node_class ngli_triangle_class = {
    .id        = NGL_NODE_TRIANGLE,
    .name      = "Triangle",
    .init      = triangle_init,
    .uninit    = triangle_uninit,
    .priv_size = sizeof(struct triangle_priv),
    .params    = triangle_params,
    .file      = __FILE__,
};
