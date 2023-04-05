/*
 * Copyright 2016-2022 GoPro Inc.
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
#include "nopegl.h"
#include "internal.h"
#include "math_utils.h"
#include "transforms.h"

struct rotate_opts {
    struct ngl_node *child;
    struct ngl_node *angle_node;
    float angle;
    float axis[3];
    float anchor[3];
};

struct rotate_priv {
    struct transform trf;
    float normed_axis[3];
    const float *anchor;
};

static void update_trf_matrix(struct ngl_node *node, float deg_angle)
{
    struct rotate_priv *s = node->priv_data;
    struct transform *trf = &s->trf;

    const float angle = NGLI_DEG2RAD(deg_angle);
    ngli_mat4_rotate(trf->matrix, angle, s->normed_axis, s->anchor);
}

static int rotate_init(struct ngl_node *node)
{
    struct rotate_priv *s = node->priv_data;
    const struct rotate_opts *o = node->opts;
    static const float zvec[3];
    if (!memcmp(o->axis, zvec, sizeof(o->axis))) {
        LOG(ERROR, "(0.0, 0.0, 0.0) is not a valid axis");
        return NGL_ERROR_INVALID_ARG;
    }
    if (memcmp(o->anchor, zvec, sizeof(zvec)))
        s->anchor = o->anchor;
    ngli_vec3_norm(s->normed_axis, o->axis);
    if (!o->angle_node)
        update_trf_matrix(node, o->angle);
    s->trf.child = o->child;
    return 0;
}

static int update_angle(struct ngl_node *node)
{
    const struct rotate_opts *o = node->opts;
    update_trf_matrix(node, o->angle);
    return 0;
}

static int rotate_update(struct ngl_node *node, double t)
{
    const struct rotate_opts *o = node->opts;
    if (o->angle_node) {
        int ret = ngli_node_update(o->angle_node, t);
        if (ret < 0)
            return ret;
        struct variable_info *angle = o->angle_node->priv_data;
        update_trf_matrix(node, *(float *)angle->data);
    }
    return ngli_node_update(o->child, t);
}

#define OFFSET(x) offsetof(struct rotate_opts, x)
static const struct node_param rotate_params[] = {
    {"child",  NGLI_PARAM_TYPE_NODE, OFFSET(child),
               .flags=NGLI_PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to rotate")},
    {"angle",  NGLI_PARAM_TYPE_F32,  OFFSET(angle_node),
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
               .update_func=update_angle,
               .desc=NGLI_DOCSTRING("rotation angle in degrees")},
    {"axis",   NGLI_PARAM_TYPE_VEC3, OFFSET(axis),   {.vec={0.0, 0.0, 1.0}},
               .desc=NGLI_DOCSTRING("rotation axis")},
    {"anchor", NGLI_PARAM_TYPE_VEC3, OFFSET(anchor), {.vec={0.0, 0.0, 0.0}},
               .desc=NGLI_DOCSTRING("vector to the center point of the rotation")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_rotate, offsetof(struct rotate_priv, trf) == 0);

const struct node_class ngli_rotate_class = {
    .id        = NGL_NODE_ROTATE,
    .name      = "Rotate",
    .init      = rotate_init,
    .update    = rotate_update,
    .draw      = ngli_transform_draw,
    .opts_size = sizeof(struct rotate_opts),
    .priv_size = sizeof(struct rotate_priv),
    .params    = rotate_params,
    .file      = __FILE__,
};
