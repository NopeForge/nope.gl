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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"

#define OFFSET(x) offsetof(struct rotate, x)
static const struct node_param rotate_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"angle", PARAM_TYPE_DBL,  OFFSET(angle)},
    {"axis",  PARAM_TYPE_VEC3, OFFSET(axis)},
    {"animkf", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMESCALAR, -1}},
    {NULL}
};

static int rotate_init(struct ngl_node *node)
{
    struct rotate *s = node->priv_data;
    LOG(VERBOSE, "rotate %s by %f on (%f,%f,%f)",
        s->child->class->name, s->angle,
        s->axis[0], s->axis[1], s->axis[2]);
    ngli_node_init(s->child);
    for (int i = 0; i < s->nb_animkf; i++)
        ngli_node_init(s->animkf[i]);
    return 0;
}

static const float get_angle(struct rotate *s, double t)
{
    float angle = s->angle;
    if (s->nb_animkf)
        ngli_animkf_interpolate(&angle, s->animkf, s->nb_animkf, &s->current_kf, t);
    return angle;
}

static void rotate_update(struct ngl_node *node, double t)
{
    struct rotate *s = node->priv_data;
    struct ngl_node *child = s->child;
    const float x = get_angle(s, t) * 2.0f * M_PI / 360.0f;

    if (s->axis[0] == 1) {
        const float rotm[4*4] = {
            1.0f,    0.0f,    0.0f, 0.0f,
            0.0f,  cos(x),  sin(x), 0.0f,
            0.0f, -sin(x),  cos(x), 0.0f,
            0.0f,    0.0f,    0.0f, 1.0f,
        };
        ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, rotm);
    } else if (s->axis[1] == 1) {
        const float rotm[4*4] = {
            cos(x), 0.0f, -sin(x), 0.0f,
              0.0f, 1.0f,    0.0f, 0.0f,
            sin(x), 0.0f,  cos(x), 0.0f,
              0.0f, 0.0f,    0.0f, 1.0f,
        };
        ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, rotm);
    } else if (s->axis[2] == 1) {
        const float rotm[4*4] = {
            cos(x),  sin(x), 0.0f, 0.0f,
           -sin(x),  cos(x), 0.0f, 0.0f,
              0.0f,    0.0f, 1.0f, 0.0f,
              0.0f,    0.0f, 0.0f, 1.0f,
        };
        ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, rotm);
    }
    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    ngli_node_update(child, t);
}

static void rotate_draw(struct ngl_node *node)
{
    struct rotate *s = node->priv_data;
    ngli_node_draw(s->child);
}

const struct node_class ngli_rotate_class = {
    .id        = NGL_NODE_ROTATE,
    .name      = "Rotate",
    .init      = rotate_init,
    .update    = rotate_update,
    .draw      = rotate_draw,
    .priv_size = sizeof(struct rotate),
    .params    = rotate_params,
};
