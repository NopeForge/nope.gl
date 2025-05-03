/*
 * Copyright 2017-2022 GoPro Inc.
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
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "nopegl.h"
#include "utils/memory.h"
#include "utils/utils.h"

struct circle_opts {
    float radius;
    uint32_t npoints;
};

struct circle_priv {
    struct geometry *geom;
};

#define OFFSET(x) offsetof(struct circle_opts, x)
static const struct node_param circle_params[] = {
    {"radius",  NGLI_PARAM_TYPE_F32, OFFSET(radius),  {.f32=1.f},
                .desc=NGLI_DOCSTRING("circle radius")},
    {"npoints", NGLI_PARAM_TYPE_U32, OFFSET(npoints), {.u32=16},
                .desc=NGLI_DOCSTRING("number of points")},
    {NULL}
};

NGLI_STATIC_ASSERT(geom_on_top_of_circle, offsetof(struct circle_priv, geom) == 0);

static int circle_init(struct ngl_node *node)
{
    int ret = 0;
    struct circle_priv *s = node->priv_data;
    const struct circle_opts *o = node->opts;

    if (o->npoints < 3) {
        LOG(ERROR, "invalid number of points (%d < 3)", o->npoints);
        return NGL_ERROR_INVALID_ARG;
    }
    const size_t nb_vertices = o->npoints + 1;
    const size_t nb_indices  = o->npoints * 3;

    float *vertices  = ngli_calloc(nb_vertices, sizeof(*vertices)  * 3);
    float *uvcoords  = ngli_calloc(nb_vertices, sizeof(*uvcoords)  * 2);
    float *normals   = ngli_calloc(nb_vertices, sizeof(*normals)   * 3);
    uint16_t *indices = ngli_calloc(nb_indices, sizeof(*indices));

    if (!vertices || !uvcoords || !normals || !indices) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    const float step = TAU_F32 / (float)o->npoints;

    vertices[0] = 0.0f;
    vertices[1] = 0.0f;
    uvcoords[0] = 0.5f;
    uvcoords[1] = 0.5f;
    for (size_t i = 1; i < nb_vertices; i++) {
        const float angle = (float)(i - 1) * -step;
        const float x = sinf(angle) * o->radius;
        const float y = cosf(angle) * o->radius;
        vertices[i*3 + 0] = x;
        vertices[i*3 + 1] = y;
        uvcoords[i*2 + 0] = (x + 1.0f) / 2.0f;
        uvcoords[i*2 + 1] = (1.0f - y) / 2.0f;
        indices[(i - 1) * 3 + 0]  = 0; // point to center coordinate
        indices[(i - 1) * 3 + 1]  = (uint16_t)i;
        indices[(i - 1) * 3 + 2]  = (uint16_t)(i + 1);
    }
    /* Fix overflowing vertex reference back to the start for sealing the
     * circle */
    indices[nb_indices - 1] = 1;

    ngli_vec3_normalvec(normals, vertices, vertices + 3, vertices + 6);
    for (size_t i = 1; i < nb_vertices; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    struct ngpu_ctx *gpu_ctx = node->ctx->gpu_ctx;

    s->geom = ngli_geometry_create(gpu_ctx);
    if (!s->geom) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    if ((ret = ngli_geometry_set_vertices(s->geom, nb_vertices, vertices)) < 0 ||
        (ret = ngli_geometry_set_uvcoords(s->geom, nb_vertices, uvcoords)) < 0 ||
        (ret = ngli_geometry_set_normals(s->geom, nb_vertices, normals))   < 0 ||
        (ret = ngli_geometry_set_indices(s->geom, nb_indices, indices))    < 0)
        goto end;

end:
    ngli_free(vertices);
    ngli_free(uvcoords);
    ngli_free(normals);
    ngli_free(indices);

    if (ret < 0)
        return ret;

    return ngli_geometry_init(s->geom, NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

static void circle_uninit(struct ngl_node *node)
{
    struct circle_priv *s = node->priv_data;
    ngli_geometry_freep(&s->geom);
}

const struct node_class ngli_circle_class = {
    .id        = NGL_NODE_CIRCLE,
    .name      = "Circle",
    .init      = circle_init,
    .uninit    = circle_uninit,
    .opts_size = sizeof(struct circle_opts),
    .priv_size = sizeof(struct circle_priv),
    .params    = circle_params,
    .file      = __FILE__,
};
