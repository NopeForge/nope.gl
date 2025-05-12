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

#include <stddef.h>
#include <string.h>

#include "nopegl.h"
#include "internal.h"
#include "math_utils.h"
#include "node_transform.h"
#include "node_uniform.h"
#include "transforms.h"

struct transform_opts {
    struct ngl_node *child;
    struct ngl_node *matrix_node;
    float matrix[4*4];
};

struct transform_priv {
    struct transform trf;
};

static int update_matrix(struct ngl_node *node)
{
    struct transform_priv *s = node->priv_data;
    const struct transform_opts *o = node->opts;
    memcpy(s->trf.matrix, o->matrix, sizeof(o->matrix));
    return 0;
}

#define OFFSET(x) offsetof(struct transform_opts, x)
static const struct node_param transform_params[] = {
    {"child",  NGLI_PARAM_TYPE_NODE, OFFSET(child), .flags=NGLI_PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to apply the transform to")},
    {"matrix", NGLI_PARAM_TYPE_MAT4, OFFSET(matrix_node), {.mat=NGLI_MAT4_IDENTITY},
               .flags=NGLI_PARAM_FLAG_ALLOW_NODE | NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=update_matrix,
               .desc=NGLI_DOCSTRING("transformation matrix")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_transform, offsetof(struct transform_priv, trf) == 0);

static int transform_init(struct ngl_node *node)
{
    struct transform_priv *s = node->priv_data;
    const struct transform_opts *o = node->opts;
    memcpy(s->trf.matrix, o->matrix, sizeof(o->matrix));
    s->trf.child = o->child;
    return 0;
}

static int transform_update(struct ngl_node *node, double t)
{
    struct transform_priv *s = node->priv_data;
    struct transform_opts *o = node->opts;

    int ret = ngli_node_update_children(node, t);
    if (ret < 0)
        return ret;

    if (o->matrix_node) {
        float *data = ngli_node_get_data_ptr(o->matrix_node, o->matrix);
        memcpy(s->trf.matrix, data, sizeof(s->trf.matrix));
    }

    return 0;
}

const struct node_class ngli_transform_class = {
    .id        = NGL_NODE_TRANSFORM,
    .category  = NGLI_NODE_CATEGORY_TRANSFORM,
    .name      = "Transform",
    .init      = transform_init,
    .update    = transform_update,
    .draw      = ngli_transform_draw,
    .opts_size = sizeof(struct transform_opts),
    .priv_size = sizeof(struct transform_priv),
    .params    = transform_params,
    .file      = __FILE__,
};
