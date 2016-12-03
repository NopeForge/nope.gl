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
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct shape, x)
static const struct node_param quad_params[] = {
    {"corner", PARAM_TYPE_VEC3, OFFSET(quad_corner), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"width",  PARAM_TYPE_VEC3, OFFSET(quad_width),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"height", PARAM_TYPE_VEC3, OFFSET(quad_height), .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

#define C(index) s->quad_corner[(index)]
#define W(index) s->quad_width[(index)]
#define H(index) s->quad_height[(index)]

static int quad_init(struct ngl_node *node)
{
    struct shape *s = node->priv_data;

    const GLfloat vertices[] = {
        C(0) + H(0),        C(1) + H(1),        C(2) + H(2),        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        C(0) + W(0),        C(1) + W(1),        C(2) + W(2),        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        C(0),               C(1),               C(2),               1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2), 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };
    s->nb_vertices = NGLI_ARRAY_NB(vertices) / 9;
    s->vertices = calloc(1, sizeof(vertices));
    if (!s->vertices)
        return -1;

    memcpy(s->vertices, vertices, sizeof(vertices));

    static const GLushort indices[] = { 0, 1, 2, 0, 3, 1 };
    s->nb_indices = NGLI_ARRAY_NB(indices);
    s->indices = calloc(1, sizeof(indices));
    if (!s->indices)
        return -1;

    memcpy(s->indices, indices, sizeof(indices));

    s->draw_mode = GL_TRIANGLES;
    s->draw_type = GL_UNSIGNED_SHORT;

    return 0;
}

static void quad_uninit(struct ngl_node *node)
{
    struct shape *s = node->priv_data;

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
