/*
 * Copyright 2020-2022 GoPro Inc.
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

#include "log.h"
#include "nodegl.h"
#include "internal.h"
#include "math_utils.h"
#include "transforms.h"

struct skew_opts {
    struct ngl_node *child;
    struct ngl_node *angles_node;
    float angles[3];
    float axis[3];
    float anchor[3];
};

struct skew_priv {
    struct transform trf;
    float normed_axis[3];
    const float *anchor;
};

static void update_trf_matrix(struct ngl_node *node, const float *angles)
{
    struct skew_priv *s = node->priv_data;
    struct transform *trf = &s->trf;

    const float skx = tanf(NGLI_DEG2RAD(angles[0]));
    const float sky = tanf(NGLI_DEG2RAD(angles[1]));
    const float skz = tanf(NGLI_DEG2RAD(angles[2]));

    ngli_mat4_skew(trf->matrix, skx, sky, skz, s->normed_axis, s->anchor);
}

static int skew_init(struct ngl_node *node)
{
    struct skew_priv *s = node->priv_data;
    const struct skew_opts *o = node->opts;
    static const float zvec[3];
    if (!memcmp(o->axis, zvec, sizeof(o->axis))) {
        LOG(ERROR, "(0.0, 0.0, 0.0) is not a valid axis");
        return NGL_ERROR_INVALID_ARG;
    }
    if (memcmp(o->anchor, zvec, sizeof(zvec)))
        s->anchor = o->anchor;
    ngli_vec3_norm(s->normed_axis, o->axis);
    if (!o->angles_node)
        update_trf_matrix(node, o->angles);
    s->trf.child = o->child;
    return 0;
}

static int update_angles(struct ngl_node *node)
{
    const struct skew_opts *o = node->opts;
    update_trf_matrix(node, o->angles);
    return 0;
}

static int skew_update(struct ngl_node *node, double t)
{
    const struct skew_opts *o = node->opts;
    if (o->angles_node) {
        int ret = ngli_node_update(o->angles_node, t);
        if (ret < 0)
            return ret;
        struct variable_info *angles = o->angles_node->priv_data;
        update_trf_matrix(node, angles->data);
    }
    return ngli_node_update(o->child, t);
}

#define OFFSET(x) offsetof(struct skew_opts, x)
static const struct node_param skew_params[] = {
    {"child",  NGLI_PARAM_TYPE_NODE, OFFSET(child),
               .flags=NGLI_PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to skew")},
    {"angles", NGLI_PARAM_TYPE_VEC3,  OFFSET(angles_node),
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
               .update_func=update_angles,
               .desc=NGLI_DOCSTRING("skewing angles, only components forming a plane opposite to `axis` should be set")},
    {"axis",   NGLI_PARAM_TYPE_VEC3, OFFSET(axis), {.vec={1.0, 0.0, 0.0}},
               .desc=NGLI_DOCSTRING("skew axis")},
    {"anchor", NGLI_PARAM_TYPE_VEC3, OFFSET(anchor), {.vec={0.0, 0.0, 0.0}},
               .desc=NGLI_DOCSTRING("vector to the center point of the skew")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_skew, offsetof(struct skew_priv, trf) == 0);

const struct node_class ngli_skew_class = {
    .id        = NGL_NODE_SKEW,
    .name      = "Skew",
    .init      = skew_init,
    .update    = skew_update,
    .draw      = ngli_transform_draw,
    .opts_size = sizeof(struct skew_opts),
    .priv_size = sizeof(struct skew_priv),
    .params    = skew_params,
    .file      = __FILE__,
};
