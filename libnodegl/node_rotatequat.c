/*
 * Copyright 2019 GoPro Inc.
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
#include "internal.h"
#include "math_utils.h"
#include "transforms.h"

struct rotatequat_opts {
    struct ngl_node *child;
    struct ngl_node *quat_node;
    float quat[4];
    float anchor[3];
};

struct rotatequat_priv {
    struct transform trf;
    int use_anchor;
};

static void update_trf_matrix(struct ngl_node *node, const float *quat)
{
    struct rotatequat_priv *s = node->priv_data;
    const struct rotatequat_opts *o = node->opts;
    struct transform *trf = &s->trf;
    float *matrix = trf->matrix;

    ngli_mat4_rotate_from_quat(matrix, quat);

    if (s->use_anchor) {
        const float *a = o->anchor;
        NGLI_ALIGNED_MAT(transm);
        ngli_mat4_translate(transm, a[0], a[1], a[2]);
        ngli_mat4_mul(matrix, transm, matrix);
        ngli_mat4_translate(transm, -a[0], -a[1], -a[2]);
        ngli_mat4_mul(matrix, matrix, transm);
    }
}

static int rotatequat_init(struct ngl_node *node)
{
    struct rotatequat_priv *s = node->priv_data;
    const struct rotatequat_opts *o = node->opts;
    static const float zvec[3];
    s->use_anchor = memcmp(o->anchor, zvec, sizeof(zvec));
    if (!o->quat_node)
        update_trf_matrix(node, o->quat);
    s->trf.child = o->child;
    return 0;
}

static int update_quat(struct ngl_node *node)
{
    const struct rotatequat_opts *o = node->opts;
    update_trf_matrix(node, o->quat);
    return 0;
}

static int rotatequat_update(struct ngl_node *node, double t)
{
    const struct rotatequat_opts *o = node->opts;
    if (o->quat_node) {
        int ret = ngli_node_update(o->quat_node, t);
        if (ret < 0)
            return ret;
        struct variable_info *quat = o->quat_node->priv_data;
        update_trf_matrix(node, quat->data);
    }
    return ngli_node_update(o->child, t);
}

#define OFFSET(x) offsetof(struct rotatequat_opts, x)
static const struct node_param rotatequat_params[] = {
    {"child",  NGLI_PARAM_TYPE_NODE, OFFSET(child),
               .flags=NGLI_PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to rotate")},
    {"quat",   NGLI_PARAM_TYPE_VEC4, OFFSET(quat_node), {.vec=NGLI_QUAT_IDENTITY},
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
               .update_func=update_quat,
               .desc=NGLI_DOCSTRING("quaternion")},
    {"anchor", NGLI_PARAM_TYPE_VEC3, OFFSET(anchor), {.vec={0.0, 0.0, 0.0}},
               .desc=NGLI_DOCSTRING("vector to the center point of the rotation")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_rotatequat, offsetof(struct rotatequat_priv, trf) == 0);

const struct node_class ngli_rotatequat_class = {
    .id        = NGL_NODE_ROTATEQUAT,
    .name      = "RotateQuat",
    .init      = rotatequat_init,
    .update    = rotatequat_update,
    .draw      = ngli_transform_draw,
    .opts_size = sizeof(struct rotatequat_opts),
    .priv_size = sizeof(struct rotatequat_priv),
    .params    = rotatequat_params,
    .file      = __FILE__,
};
