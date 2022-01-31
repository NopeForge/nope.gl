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

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "geometry.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"
#include "topology.h"
#include "utils.h"

struct circle_opts {
    float radius;
    int npoints;
};

struct circle_priv {
    struct geometry geom;
    struct circle_opts opts;
};

#define OFFSET(x) offsetof(struct circle_priv, opts.x)
static const struct node_param circle_params[] = {
    {"radius",  NGLI_PARAM_TYPE_F32, OFFSET(radius),  {.f32=1.f},
                .desc=NGLI_DOCSTRING("circle radius")},
    {"npoints", NGLI_PARAM_TYPE_I32, OFFSET(npoints), {.i32=16},
                .desc=NGLI_DOCSTRING("number of points")},
    {NULL}
};

NGLI_STATIC_ASSERT(geom_on_top_of_circle, offsetof(struct circle_priv, geom) == 0);

static int circle_init(struct ngl_node *node)
{
    int ret = 0;
    struct circle_priv *s = node->priv_data;
    const struct circle_opts *o = &s->opts;

    if (o->npoints < 3) {
        LOG(ERROR, "invalid number of points (%d < 3)", o->npoints);
        return NGL_ERROR_INVALID_ARG;
    }
    const int nb_vertices = o->npoints + 1;
    const int nb_indices  = o->npoints * 3;

    float *vertices  = ngli_calloc(nb_vertices, sizeof(*vertices)  * 3);
    float *uvcoords  = ngli_calloc(nb_vertices, sizeof(*uvcoords)  * 2);
    float *normals   = ngli_calloc(nb_vertices, sizeof(*normals)   * 3);
    uint16_t *indices = ngli_calloc(nb_indices, sizeof(*indices));

    if (!vertices || !uvcoords || !normals || !indices) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    const float step = 2.0 * M_PI / o->npoints;

    vertices[0] = 0.0;
    vertices[1] = 0.0;
    uvcoords[0] = 0.5;
    uvcoords[1] = 0.5;
    for (int i = 1; i < nb_vertices; i++) {
        const float angle = (i - 1) * -step;
        const float x = sinf(angle) * o->radius;
        const float y = cosf(angle) * o->radius;
        vertices[i*3 + 0] = x;
        vertices[i*3 + 1] = y;
        uvcoords[i*2 + 0] = (x + 1.0) / 2.0;
        uvcoords[i*2 + 1] = (1.0 - y) / 2.0;
        indices[(i - 1) * 3 + 0]  = 0; // point to center coordinate
        indices[(i - 1) * 3 + 1]  = i;
        indices[(i - 1) * 3 + 2]  = i + 1;
    }
    /* Fix overflowing vertex reference back to the start for sealing the
     * circle */
    indices[nb_indices - 1] = 1;

    ngli_vec3_normalvec(normals, vertices, vertices + 3, vertices + 6);
    for (int i = 1; i < nb_vertices; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    struct gpu_ctx *gpu_ctx = node->ctx->gpu_ctx;

    if ((ret = ngli_geometry_gen_vec3(&s->geom.vertices_buffer,   &s->geom.vertices_layout, gpu_ctx, nb_vertices, vertices)) < 0 ||
        (ret = ngli_geometry_gen_vec2(&s->geom.uvcoords_buffer,   &s->geom.uvcoords_layout, gpu_ctx, nb_vertices, uvcoords)) < 0 ||
        (ret = ngli_geometry_gen_vec3(&s->geom.normals_buffer,    &s->geom.normals_layout,  gpu_ctx, nb_vertices, normals))  < 0 ||
        (ret = ngli_geometry_gen_indices(&s->geom.indices_buffer, &s->geom.indices_layout,  gpu_ctx, nb_indices,  indices))  < 0)
        goto end;

    s->geom.topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

end:
    ngli_free(vertices);
    ngli_free(uvcoords);
    ngli_free(normals);
    ngli_free(indices);
    return ret;
}

static void circle_uninit(struct ngl_node *node)
{
    struct circle_priv *s = node->priv_data;

    ngli_buffer_freep(&s->geom.vertices_buffer);
    ngli_buffer_freep(&s->geom.uvcoords_buffer);
    ngli_buffer_freep(&s->geom.normals_buffer);
    ngli_buffer_freep(&s->geom.indices_buffer);
}

const struct node_class ngli_circle_class = {
    .id        = NGL_NODE_CIRCLE,
    .name      = "Circle",
    .init      = circle_init,
    .uninit    = circle_uninit,
    .priv_size = sizeof(struct circle_priv),
    .params    = circle_params,
    .file      = __FILE__,
};
