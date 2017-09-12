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
#include <stdlib.h>
#include <string.h>

#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "transforms.h"

#define OFFSET(x) offsetof(struct uniform, x)
static const struct node_param uniformscalar_params[] = {
    {"value",  PARAM_TYPE_DBL,  OFFSET(scalar)},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMATIONSCALAR, -1}},
    {NULL}
};

static const struct node_param uniformvec2_params[] = {
    {"value",  PARAM_TYPE_VEC2, OFFSET(vector)},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMATIONVEC2, -1}},
    {NULL}
};

static const struct node_param uniformvec3_params[] = {
    {"value",  PARAM_TYPE_VEC3, OFFSET(vector)},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMATIONVEC3, -1}},
    {NULL}
};

static const struct node_param uniformvec4_params[] = {
    {"value",  PARAM_TYPE_VEC4, OFFSET(vector)},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMATIONVEC4, -1}},
    {NULL}
};

static const struct node_param uniformint_params[] = {
    {"value",  PARAM_TYPE_INT, OFFSET(ival)},
    {NULL}
};

static const struct node_param uniformmat4_params[] = {
    {"transform", PARAM_TYPE_NODE, OFFSET(transform), .node_types=TRANSFORM_TYPES_LIST},
    {NULL}
};

static inline void uniform_update(struct uniform *s, double t, int len)
{
    if (s->anim) {
        struct ngl_node *anim_node = s->anim;
        struct animation *anim = anim_node->priv_data;
        ngli_node_update(anim_node, t);
        if (len == 1)
            s->scalar = anim->values[0];
        else
            memcpy(s->vector, anim->values, len * sizeof(*s->vector));
    }
}

#define UPDATE_FUNC(type, len)                                          \
static void uniform##type##_update(struct ngl_node *node, double t)     \
{                                                                       \
    uniform_update(node->priv_data, t, len);                            \
}

UPDATE_FUNC(scalar, 1);
UPDATE_FUNC(vec2,   2);
UPDATE_FUNC(vec3,   3);
UPDATE_FUNC(vec4,   4);

static void uniform_mat_update(struct ngl_node *node, double t)
{
    struct uniform *s = node->priv_data;
    if (s->transform) {
        ngli_node_update(s->transform, t);
        const float *matrix = ngli_get_last_transformation_matrix(s->transform);
        memcpy(s->matrix, matrix, sizeof(s->matrix));
    }
}

static int uniform_init(struct ngl_node *node)
{
    struct uniform *s = node->priv_data;

    s->matrix[0] =
    s->matrix[5] =
    s->matrix[10] =
    s->matrix[15] = 1.0f;

    return 0;
}

const struct node_class ngli_uniformscalar_class = {
    .id        = NGL_NODE_UNIFORMSCALAR,
    .name      = "UniformScalar",
    .init      = uniform_init,
    .update    = uniformscalar_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformscalar_params,
};

const struct node_class ngli_uniformvec2_class = {
    .id        = NGL_NODE_UNIFORMVEC2,
    .name      = "UniformVec2",
    .init      = uniform_init,
    .update    = uniformvec2_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec2_params,
};

const struct node_class ngli_uniformvec3_class = {
    .id        = NGL_NODE_UNIFORMVEC3,
    .name      = "UniformVec3",
    .init      = uniform_init,
    .update    = uniformvec3_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec3_params,
};

const struct node_class ngli_uniformvec4_class = {
    .id        = NGL_NODE_UNIFORMVEC4,
    .name      = "UniformVec4",
    .init      = uniform_init,
    .update    = uniformvec4_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec4_params,
};

const struct node_class ngli_uniformint_class = {
    .id        = NGL_NODE_UNIFORMINT,
    .name      = "UniformInt",
    .init      = uniform_init,
    .priv_size = sizeof(struct uniform),
    .params    = uniformint_params,
};

const struct node_class ngli_uniformmat4_class = {
    .id        = NGL_NODE_UNIFORMMAT4,
    .name      = "UniformMat4",
    .init      = uniform_init,
    .update    = uniform_mat_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformmat4_params,
};
