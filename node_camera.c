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
#include "math_utils.h"

#define OFFSET(x) offsetof(struct camera, x)
static const struct node_param camera_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"eye", PARAM_TYPE_VEC3,  OFFSET(eye)},
    {"center", PARAM_TYPE_VEC3,  OFFSET(center)},
    {"up", PARAM_TYPE_VEC3,  OFFSET(up)},
    {"perspective", PARAM_TYPE_VEC4,  OFFSET(perspective)},
    {"eye_animkf", PARAM_TYPE_NODELIST, OFFSET(eye_animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED, .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1}},
    {"center_animkf", PARAM_TYPE_NODELIST, OFFSET(center_animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED, .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1}},
    {"fov_animkf", PARAM_TYPE_NODELIST, OFFSET(fov_animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED, .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMESCALAR, -1}},
    {NULL}
};

static int camera_init(struct ngl_node *node)
{
    struct camera *s = node->priv_data;
    ngli_node_init(s->child);
    return 0;
}

static void camera_update(struct ngl_node *node, double t)
{
    struct camera *s = node->priv_data;
    struct ngl_node *child = s->child;

    float perspective[4*4];
    float view[4*4];

    if (s->nb_eye_animkf)
        ngli_animkf_interpolate(s->eye, s->eye_animkf, s->nb_eye_animkf, &s->current_eye_kf, t);

    if (s->nb_center_animkf)
        ngli_animkf_interpolate(s->center, s->center_animkf, s->nb_center_animkf, &s->current_center_kf, t);

    ngli_mat4_look_at(
        view,
        s->eye,
        s->center,
        s->up
    );

    if (s->nb_fov_animkf)
        ngli_animkf_interpolate(&s->perspective[0], s->fov_animkf, s->nb_fov_animkf, &s->current_fov_kf, t);

    ngli_mat4_perspective(
        perspective,
        s->perspective[0],
        s->perspective[1],
        s->perspective[2],
        s->perspective[3]
    );

    memcpy(child->modelview_matrix, view, sizeof(view));
    memcpy(child->projection_matrix, perspective, sizeof(perspective));

    ngli_node_update(child, t);
}

static void camera_draw(struct ngl_node *node)
{
    struct camera *s = node->priv_data;
    ngli_node_draw(s->child);
}

const struct node_class ngli_camera_class = {
    .id        = NGL_NODE_CAMERA,
    .name      = "Camera",
    .init      = camera_init,
    .update    = camera_update,
    .draw      = camera_draw,
    .priv_size = sizeof(struct camera),
    .params    = camera_params,
};
