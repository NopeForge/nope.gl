/*
 * Copyright 2016 Clément Bœsch <cboesch@gopro.com>
 * Copyright 2016 Matthieu Bouron <mbouron@gopro.com>
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
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"

#define OFFSET(x) offsetof(struct translate, x)
static const struct node_param translate_params[] = {
    {"child",  PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"vector", PARAM_TYPE_VEC3, OFFSET(vector)},
    {"animkf", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1}},
    {NULL}
};

static int translate_init(struct ngl_node *node)
{
    struct translate *s = node->priv_data;
    LOG(VERBOSE, "translate %s by (%f,%f,%f)",
        s->child->class->name,
        s->vector[0], s->vector[1], s->vector[2]);
    ngli_node_init(s->child);
    for (int i = 0; i < s->nb_animkf; i++)
        ngli_node_init(s->animkf[i]);
    return 0;
}

static const float *get_vector(struct translate *s, double t)
{
    if (s->nb_animkf)
        ngli_animkf_interpolate(s->vector, s->animkf, s->nb_animkf, &s->current_kf, t);
    return s->vector;
}

static void translate_update(struct ngl_node *node, double t)
{
    struct translate *s = node->priv_data;
    struct ngl_node *child = s->child;
    const float *vec = get_vector(s, t);
    const float tm[4*4] = {
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   0.0f,
        vec[0], vec[1], vec[2], 1.0f,
    };
    ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, tm);
    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    ngli_node_update(child, t);
}

static void translate_draw(struct ngl_node *node)
{
    struct translate *s = node->priv_data;
    ngli_node_draw(s->child);
}

const struct node_class ngli_translate_class = {
    .id        = NGL_NODE_TRANSLATE,
    .name      = "Translate",
    .init      = translate_init,
    .update    = translate_update,
    .draw      = translate_draw,
    .priv_size = sizeof(struct translate),
    .params    = translate_params,
};
