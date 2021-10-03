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

#include <stddef.h>
#include <stdio.h>
#include "nodegl.h"
#include "internal.h"
#include "math_utils.h"
#include "transforms.h"

struct translate_priv {
    struct transform_priv trf;
    struct ngl_node *vector_node;
    float vector[3];
};

static void update_trf_matrix(struct ngl_node *node, const float *vec)
{
    struct translate_priv *s = node->priv_data;
    struct transform_priv *trf = &s->trf;
    ngli_mat4_translate(trf->matrix, vec[0], vec[1], vec[2]);
}

static int update_vector(struct ngl_node *node)
{
    struct translate_priv *s = node->priv_data;
    update_trf_matrix(node, s->vector);
    return 0;
}

static int translate_init(struct ngl_node *node)
{
    struct translate_priv *s = node->priv_data;
    if (!s->vector_node)
        update_trf_matrix(node, s->vector);
    return 0;
}

static int translate_update(struct ngl_node *node, double t)
{
    struct translate_priv *s = node->priv_data;
    struct transform_priv *trf = &s->trf;
    struct ngl_node *child = trf->child;
    if (s->vector_node) {
        int ret = ngli_node_update(s->vector_node, t);
        if (ret < 0)
            return ret;
        struct variable_priv *vector = s->vector_node->priv_data;
        update_trf_matrix(node, vector->vector);
    }
    return ngli_node_update(child, t);
}

#define OFFSET(x) offsetof(struct translate_priv, x)
static const struct node_param translate_params[] = {
    {"child",  NGLI_PARAM_TYPE_NODE, OFFSET(trf.child),
               .flags=NGLI_PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to translate")},
    {"vector", NGLI_PARAM_TYPE_VEC3, OFFSET(vector_node),
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
               .update_func=update_vector,
               .desc=NGLI_DOCSTRING("translation vector")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_translate, OFFSET(trf) == 0);

const struct node_class ngli_translate_class = {
    .id        = NGL_NODE_TRANSLATE,
    .name      = "Translate",
    .init      = translate_init,
    .update    = translate_update,
    .draw      = ngli_transform_draw,
    .priv_size = sizeof(struct translate_priv),
    .params    = translate_params,
    .file      = __FILE__,
};
