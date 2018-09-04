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
#include "math_utils.h"

#define OFFSET(x) offsetof(struct rotate, x)
static const struct node_param rotate_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR,
              .desc=NGLI_DOCSTRING("scene to rotate")},
    {"angle",  PARAM_TYPE_DBL,  OFFSET(angle),
               .desc=NGLI_DOCSTRING("rotation angle in degrees")},
    {"axis",   PARAM_TYPE_VEC3, OFFSET(axis),   {.vec={0.0, 0.0, 1.0}},
               .desc=NGLI_DOCSTRING("rotation axis")},
    {"anchor", PARAM_TYPE_VEC3, OFFSET(anchor), {.vec={0.0, 0.0, 0.0}},
               .desc=NGLI_DOCSTRING("vector to the center point of the rotation")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDFLOAT, -1},
               .desc=NGLI_DOCSTRING("`angle` animation")},
    {NULL}
};

static const double get_angle(struct rotate *s, double t)
{
    if (!s->anim)
        return s->angle;
    struct ngl_node *anim_node = s->anim;
    struct animation *anim = anim_node->priv_data;
    int ret = ngli_node_update(anim_node, t);
    if (ret < 0)
        return s->angle;
    return anim->scalar;
}

static int rotate_init(struct ngl_node *node)
{
    struct rotate *s = node->priv_data;
    static const float zero_axis[3] = {0.0, 0.0, 0.0};

    if (!memcmp(s->axis, zero_axis, sizeof(s->axis))) {
        LOG(ERROR, "(0.0, 0.0, 0.0) is not a valid axis");
        return -1;
    }

    return 0;
}

static int rotate_update(struct ngl_node *node, double t)
{
    struct rotate *s = node->priv_data;
    struct ngl_node *child = s->child;
    float axis[3];
    ngli_vec3_norm(axis, s->axis);

    NGLI_ALIGNED_MAT(rotm);
    const double angle = get_angle(s, t) * (2.0f * M_PI / 360.0f);
    ngli_mat4_rotate(rotm, angle, axis);

    static const float zero_anchor[3] = {0.0, 0.0, 0.0};
    int translate = memcmp(s->anchor, zero_anchor, sizeof(s->anchor));
    if (translate) {
        float *a = s->anchor;
        NGLI_ALIGNED_MAT(transm);
        ngli_mat4_translate(transm, a[0], a[1], a[2]);
        ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, transm);
        ngli_mat4_mul(child->modelview_matrix, child->modelview_matrix, rotm);
        ngli_mat4_translate(transm, -a[0], -a[1], -a[2]);
        ngli_mat4_mul(child->modelview_matrix, child->modelview_matrix, transm);
    } else {
        ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, rotm);
    }

    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    return ngli_node_update(child, t);
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
    .file      = __FILE__,
};
