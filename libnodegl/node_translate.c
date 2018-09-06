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
#include <string.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"

#define OFFSET(x) offsetof(struct translate, x)
static const struct node_param translate_params[] = {
    {"child",  PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR,
               .desc=NGLI_DOCSTRING("scene to translate")},
    {"vector", PARAM_TYPE_VEC3, OFFSET(vector),
               .desc=NGLI_DOCSTRING("translation vector")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDVEC3, -1},
               .desc=NGLI_DOCSTRING("`vector` animation")},
    {NULL}
};

static const float *get_vector(struct translate *s, double t)
{
    if (!s->anim)
        return s->vector;
    struct ngl_node *anim_node = s->anim;
    struct animation *anim = anim_node->priv_data;
    int ret = ngli_node_update(anim_node, t);
    if (ret < 0)
        return s->vector;
    return anim->values;
}

static int translate_update(struct ngl_node *node, double t)
{
    struct translate *s = node->priv_data;
    struct ngl_node *child = s->child;
    const float *vec = get_vector(s, t);
    ngli_mat4_translate(s->matrix, vec[0], vec[1], vec[2]);
    return ngli_node_update(child, t);
}

static void translate_draw(struct ngl_node *node)
{
    struct translate *s = node->priv_data;
    struct ngl_node *child = s->child;
    ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, s->matrix);
    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    ngli_node_draw(s->child);
}

const struct node_class ngli_translate_class = {
    .id        = NGL_NODE_TRANSLATE,
    .name      = "Translate",
    .update    = translate_update,
    .draw      = translate_draw,
    .priv_size = sizeof(struct translate),
    .params    = translate_params,
    .file      = __FILE__,
};
