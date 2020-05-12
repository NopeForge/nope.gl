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

#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "transforms.h"
#include "type.h"


static int set_live_changed(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    if (s->dynamic) {
        LOG(ERROR, "updating data on a dynamic uniform is unsupported");
        return NGL_ERROR_INVALID_USAGE;
    }
    s->live_changed = 1;
    return 0;
}

#define DECLARE_UPDATE_FUNC(name, src)                        \
static int uniform##name##_update_func(struct ngl_node *node) \
{                                                             \
    int ret = set_live_changed(node);                         \
    if (ret < 0)                                              \
        return ret;                                           \
    struct variable_priv *s = node->priv_data;                \
    memcpy(s->data, src, s->data_size);                       \
    return 0;                                                 \
}                                                             \

DECLARE_UPDATE_FUNC(int,   s->opt.ivec)
DECLARE_UPDATE_FUNC(vec,   s->opt.vec)
DECLARE_UPDATE_FUNC(mat4,  s->opt.mat)

static int uniformfloat_update_func(struct ngl_node *node)
{
    int ret = set_live_changed(node);
    if (ret < 0)
        return ret;
    struct variable_priv *s = node->priv_data;
    s->scalar = s->opt.dbl; // double -> float
    return 0;
}

static int uniformquat_update_func(struct ngl_node *node)
{
    int ret = set_live_changed(node);
    if (ret < 0)
        return ret;
    struct variable_priv *s = node->priv_data;
    memcpy(s->vector, s->opt.vec, s->data_size);
    if (s->as_mat4)
        ngli_mat4_rotate_from_quat(s->matrix, s->vector);
    return 0;
}

#define OFFSET(x) offsetof(struct variable_priv, x)
static const struct node_param uniformfloat_params[] = {
    {"value",  PARAM_TYPE_DBL,  OFFSET(opt.dbl),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformfloat_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {NULL}
};

static const struct node_param uniformvec2_params[] = {
    {"value",  PARAM_TYPE_VEC2, OFFSET(opt.vec),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformvec_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {NULL}
};

static const struct node_param uniformvec3_params[] = {
    {"value",  PARAM_TYPE_VEC3, OFFSET(opt.vec),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformvec_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {NULL}
};

static const struct node_param uniformvec4_params[] = {
    {"value",  PARAM_TYPE_VEC4, OFFSET(opt.vec),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformvec_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {NULL}
};

static const struct node_param uniformquat_params[] = {
    {"value",  PARAM_TYPE_VEC4, OFFSET(opt.vec), {.vec=NGLI_QUAT_IDENTITY},
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformquat_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"as_mat4", PARAM_TYPE_BOOL, OFFSET(as_mat4), {.i64=0},
                .desc=NGLI_DOCSTRING("exposed as a 4x4 rotation matrix in the program")},
    {NULL}
};

static const struct node_param uniformint_params[] = {
    {"value",  PARAM_TYPE_INT, OFFSET(opt.ivec),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformint_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {NULL}
};

static const struct node_param uniformmat4_params[] = {
    {"value",     PARAM_TYPE_MAT4, OFFSET(opt.mat), {.mat=NGLI_MAT4_IDENTITY},
                  .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
                  .update_func=uniformmat4_update_func,
                  .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"transform", PARAM_TYPE_NODE, OFFSET(transform), .node_types=TRANSFORM_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("`value` transformation chain")},
    {NULL}
};

static int uniform_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    s->live_changed = 0;
    return 0;
}

#define uniformfloat_update uniform_update
#define uniformvec2_update  uniform_update
#define uniformvec3_update  uniform_update
#define uniformvec4_update  uniform_update
#define uniformint_update   uniform_update

static int uniformquat_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    if (s->as_mat4)
        ngli_mat4_rotate_from_quat(s->matrix, s->vector);
    return uniform_update(node, t);
}

static int uniformmat4_update(struct ngl_node *node, double t)
{
    struct ngl_ctx *ctx = node->ctx;
    struct variable_priv *s = node->priv_data;
    if (s->transform) {
        int ret = ngli_node_update(s->transform, t);
        if (ret < 0)
            return ret;
        static const NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;
        if (!ngli_darray_push(&ctx->modelview_matrix_stack, id_matrix))
            return NGL_ERROR_MEMORY;
        ngli_node_draw(s->transform);
        ngli_darray_pop(&ctx->modelview_matrix_stack);
        if (s->transform_matrix)
            memcpy(s->matrix, s->transform_matrix, sizeof(s->matrix));
    }
    s->live_changed = 0;
    return 0;
}

static int uniformfloat_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = &s->scalar;
    s->data_size = sizeof(s->scalar);
    s->data_type = NGLI_TYPE_FLOAT;
    s->scalar = s->opt.dbl; // double -> float
    return 0;
}

static int uniformvec2_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->vector;
    s->data_size = 2 * sizeof(*s->vector);
    s->data_type = NGLI_TYPE_VEC2;
    memcpy(s->data, s->opt.vec, s->data_size);
    return 0;
}

static int uniformvec3_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->vector;
    s->data_size = 3 * sizeof(*s->vector);
    s->data_type = NGLI_TYPE_VEC3;
    memcpy(s->data, s->opt.vec, s->data_size);
    return 0;
}

static int uniformvec4_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->vector;
    s->data_size = 4 * sizeof(*s->vector);
    s->data_type = NGLI_TYPE_VEC4;
    memcpy(s->data, s->opt.vec, s->data_size);
    return 0;
}

static int uniformquat_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->vector;
    s->data_size = 4 * sizeof(*s->vector);
    s->data_type = NGLI_TYPE_VEC4;
    memcpy(s->data, s->opt.vec, s->data_size);
    if (s->as_mat4) {
        s->data = s->matrix;
        s->data_size = sizeof(s->matrix);
        s->data_type = NGLI_TYPE_MAT4;
        ngli_mat4_rotate_from_quat(s->matrix, s->vector);
    }
    return 0;
}

static int uniformint_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->ivector;
    s->data_size = sizeof(*s->ivector);
    s->data_type = NGLI_TYPE_INT;
    memcpy(s->data, s->opt.ivec, s->data_size);
    return 0;
}

static int uniformmat4_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->matrix;
    s->data_size = sizeof(s->matrix);
    s->data_type = NGLI_TYPE_MAT4;
    s->transform_matrix = ngli_get_last_transformation_matrix(s->transform);
    /* Note: we assume here that a transformation chain includes at least one
     * dynamic transform. We could crawl the chain to figure it out in the
     * details, but that would be limited since we would have to also detect
     * live changes in any of the transform node at update as well. That extra
     * complexity is probably not worth just for handling the case of a static
     * transformation list. */
    s->dynamic = !!s->transform_matrix;
    memcpy(s->data, s->opt.mat, s->data_size);
    return 0;
}

#define DEFINE_UNIFORM_CLASS(class_id, class_name, type)        \
const struct node_class ngli_uniform##type##_class = {          \
    .id        = class_id,                                      \
    .category  = NGLI_NODE_CATEGORY_UNIFORM,                    \
    .name      = class_name,                                    \
    .init      = uniform##type##_init,                          \
    .update    = uniform##type##_update,                        \
    .priv_size = sizeof(struct variable_priv),                  \
    .params    = uniform##type##_params,                        \
    .file      = __FILE__,                                      \
};

DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMFLOAT, "UniformFloat", float)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMVEC2,  "UniformVec2",  vec2)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMVEC3,  "UniformVec3",  vec3)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMVEC4,  "UniformVec4",  vec4)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMQUAT,  "UniformQuat",  quat)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMINT,   "UniformInt",   int)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMMAT4,  "UniformMat4",  mat4)
