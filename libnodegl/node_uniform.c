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
static const struct node_param uniformfloat_params[] = {
    {"value",  PARAM_TYPE_DBL,  OFFSET(scalar),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDFLOAT, -1},
               .desc=NGLI_DOCSTRING("`value` animation")},
    {NULL}
};

static const struct node_param uniformvec2_params[] = {
    {"value",  PARAM_TYPE_VEC2, OFFSET(vector),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDVEC2, -1},
               .desc=NGLI_DOCSTRING("`value` animation")},
    {NULL}
};

static const struct node_param uniformvec3_params[] = {
    {"value",  PARAM_TYPE_VEC3, OFFSET(vector),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDVEC3, -1},
               .desc=NGLI_DOCSTRING("`value` animation")},
    {NULL}
};

static const struct node_param uniformvec4_params[] = {
    {"value",  PARAM_TYPE_VEC4, OFFSET(vector),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDVEC4, -1},
               .desc=NGLI_DOCSTRING("`value` animation")},
    {NULL}
};

static const struct node_param uniformquat_params[] = {
    {"value",  PARAM_TYPE_VEC4, OFFSET(vector),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDQUAT, -1},
               .desc=NGLI_DOCSTRING("`value` animation")},
    {NULL}
};

static const struct node_param uniformint_params[] = {
    {"value",  PARAM_TYPE_INT, OFFSET(ival),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {NULL}
};

static const struct node_param uniformmat4_params[] = {
    {"value",     PARAM_TYPE_MAT4, OFFSET(matrix), {.mat=NGLI_MAT4_IDENTITY},
                  .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
                  .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"transform", PARAM_TYPE_NODE, OFFSET(transform), .node_types=TRANSFORM_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("`value` transformation chain")},
    {NULL}
};

static inline int uniform_update(struct uniform *s, double t, int len)
{
    if (s->anim) {
        struct ngl_node *anim_node = s->anim;
        struct animation *anim = anim_node->priv_data;
        int ret = ngli_node_update(anim_node, t);
        if (ret < 0)
            return ret;
        if (len == 1)
            s->scalar = anim->scalar;
        else
            memcpy(s->vector, anim->values, len * sizeof(*s->vector));
    }
    return 0;
}

#define UPDATE_FUNC(type, len)                                          \
static int uniform##type##_update(struct ngl_node *node, double t)      \
{                                                                       \
    return uniform_update(node->priv_data, t, len);                     \
}

UPDATE_FUNC(float,  1);
UPDATE_FUNC(vec2,   2);
UPDATE_FUNC(vec3,   3);
UPDATE_FUNC(vec4,   4);

static int uniformquat_update(struct ngl_node *node, double t)
{
    struct uniform *s = node->priv_data;
    int ret = uniform_update(node->priv_data, t, 4);
    if (ret < 0)
        return ret;
    ngli_mat4_rotate_from_quat(s->matrix, s->vector);
    return ret;
}

static int uniform_mat_init(struct ngl_node *node)
{
    struct uniform *s = node->priv_data;
    s->transform_matrix = ngli_get_last_transformation_matrix(s->transform);
    return 0;
}

static int uniform_mat_update(struct ngl_node *node, double t)
{
    struct ngl_ctx *ctx = node->ctx;
    struct uniform *s = node->priv_data;
    if (s->transform) {
        int ret = ngli_node_update(s->transform, t);
        if (ret < 0)
            return ret;
        static const NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;
        if (!ngli_darray_push(&ctx->modelview_matrix_stack, id_matrix))
            return -1;
        ngli_node_draw(s->transform);
        ngli_darray_pop(&ctx->modelview_matrix_stack);
        if (s->transform_matrix)
            memcpy(s->matrix, s->transform_matrix, sizeof(s->matrix));
    }
    return 0;
}

const struct node_class ngli_uniformfloat_class = {
    .id        = NGL_NODE_UNIFORMFLOAT,
    .name      = "UniformFloat",
    .update    = uniformfloat_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformfloat_params,
    .file      = __FILE__,
};

const struct node_class ngli_uniformvec2_class = {
    .id        = NGL_NODE_UNIFORMVEC2,
    .name      = "UniformVec2",
    .update    = uniformvec2_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec2_params,
    .file      = __FILE__,
};

const struct node_class ngli_uniformvec3_class = {
    .id        = NGL_NODE_UNIFORMVEC3,
    .name      = "UniformVec3",
    .update    = uniformvec3_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec3_params,
    .file      = __FILE__,
};

const struct node_class ngli_uniformvec4_class = {
    .id        = NGL_NODE_UNIFORMVEC4,
    .name      = "UniformVec4",
    .update    = uniformvec4_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec4_params,
    .file      = __FILE__,
};

const struct node_class ngli_uniformquat_class = {
    .id        = NGL_NODE_UNIFORMQUAT,
    .name      = "UniformQuat",
    .update    = uniformquat_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformquat_params,
    .file      = __FILE__,
};

const struct node_class ngli_uniformint_class = {
    .id        = NGL_NODE_UNIFORMINT,
    .name      = "UniformInt",
    .priv_size = sizeof(struct uniform),
    .params    = uniformint_params,
    .file      = __FILE__,
};

const struct node_class ngli_uniformmat4_class = {
    .id        = NGL_NODE_UNIFORMMAT4,
    .name      = "UniformMat4",
    .init      = uniform_mat_init,
    .update    = uniform_mat_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformmat4_params,
    .file      = __FILE__,
};
