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
#include "internal.h"
#include "transforms.h"
#include "type.h"

struct uniform_priv {
    struct variable_info var;
    float vector[4];
    float matrix[4*4];
    int ivector[4];
    unsigned uvector[4];
};

NGLI_STATIC_ASSERT(variable_info_is_first, offsetof(struct uniform_priv, var) == 0);

#define DECLARE_UPDATE_FUNCS(type, opt_val, opt_min, opt_max, fmt, n)                                       \
static void live_boundaries_clamp_##type(struct ngl_node *node)                                             \
{                                                                                                           \
    struct variable_opts *o = node->opts;                                                                   \
                                                                                                            \
    if (!o->live.id)                                                                                        \
        return;                                                                                             \
                                                                                                            \
    for (int i = 0; i < n; i++) {                                                                           \
        if (opt_val[i] < opt_min[i]) {                                                                      \
            if (n == 1)                                                                                     \
                LOG(WARNING, "value (" fmt ") is smaller than live_min (" fmt "), clamping",                \
                    opt_val[0], opt_min[0]);                                                                \
            else                                                                                            \
                LOG(WARNING, "value component %d (" fmt ") is smaller than live_min (" fmt "), clamping",   \
                    i, opt_val[i], opt_min[i]);                                                             \
            opt_val[i] = opt_min[i];                                                                        \
        }                                                                                                   \
        if (opt_val[i] > opt_max[i]) {                                                                      \
            if (n == 1)                                                                                     \
                LOG(WARNING, "value (" fmt ") is larger than live_max (" fmt "), clamping",                 \
                    opt_val[0], opt_max[0]);                                                                \
            else                                                                                            \
                LOG(WARNING, "value component %d (" fmt ") is larger than live_max (" fmt "), clamping",    \
                    i, opt_val[i], opt_max[i]);                                                             \
            opt_val[i] = opt_max[i];                                                                        \
        }                                                                                                   \
    }                                                                                                       \
}                                                                                                           \
                                                                                                            \
static int uniform##type##_update_func(struct ngl_node *node)                                               \
{                                                                                                           \
    struct uniform_priv *s = node->priv_data;                                                               \
    const struct variable_opts *o = node->opts;                                                             \
    live_boundaries_clamp_##type(node);                                                                     \
    memcpy(s->var.data, opt_val, s->var.data_size);                                                         \
    return 0;                                                                                               \
}                                                                                                           \

DECLARE_UPDATE_FUNCS(int,   o->live.val.i, o->live.min.i, o->live.max.i, "%i", 1)
DECLARE_UPDATE_FUNCS(ivec2, o->live.val.i, o->live.min.i, o->live.max.i, "%i", 2)
DECLARE_UPDATE_FUNCS(ivec3, o->live.val.i, o->live.min.i, o->live.max.i, "%i", 3)
DECLARE_UPDATE_FUNCS(ivec4, o->live.val.i, o->live.min.i, o->live.max.i, "%i", 4)

DECLARE_UPDATE_FUNCS(uint,  o->live.val.u, o->live.min.u, o->live.max.u, "%u", 1)
DECLARE_UPDATE_FUNCS(uvec2, o->live.val.u, o->live.min.u, o->live.max.u, "%u", 2)
DECLARE_UPDATE_FUNCS(uvec3, o->live.val.u, o->live.min.u, o->live.max.u, "%u", 3)
DECLARE_UPDATE_FUNCS(uvec4, o->live.val.u, o->live.min.u, o->live.max.u, "%u", 4)

DECLARE_UPDATE_FUNCS(float, o->live.val.f, o->live.min.f, o->live.max.f, "%g", 1)
DECLARE_UPDATE_FUNCS(vec2,  o->live.val.f, o->live.min.f, o->live.max.f, "%g", 2)
DECLARE_UPDATE_FUNCS(vec3,  o->live.val.f, o->live.min.f, o->live.max.f, "%g", 3)
DECLARE_UPDATE_FUNCS(vec4,  o->live.val.f, o->live.min.f, o->live.max.f, "%g", 4)

static int uniformbool_update_func(struct ngl_node *node)
{
    struct uniform_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    memcpy(s->var.data, o->live.val.i, s->var.data_size);
    return 0;
}

static int uniformmat4_update_func(struct ngl_node *node)
{
    struct uniform_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    if (o->transform) {
        LOG(ERROR, "updating the matrix on a UniformMat4 with transforms is invalid");
        return NGL_ERROR_INVALID_USAGE;
    }
    memcpy(s->var.data, o->live.val.m, s->var.data_size);
    return 0;
}

static int uniformquat_update_func(struct ngl_node *node)
{
    struct uniform_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    live_boundaries_clamp_vec4(node);
    memcpy(s->vector, o->live.val.f, s->var.data_size);
    if (o->as_mat4)
        ngli_mat4_rotate_from_quat(s->matrix, s->vector, NULL);
    return 0;
}

#define OFFSET(x) offsetof(struct variable_opts, x)

static const struct node_param uniformbool_params[] = {
    {"value",    NGLI_PARAM_TYPE_BOOL, OFFSET(live.val.i),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformbool_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {NULL}
};

static const struct node_param uniformfloat_params[] = {
    {"value",    NGLI_PARAM_TYPE_F32, OFFSET(live.val.f),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformfloat_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_F32, OFFSET(live.min.f), {.f32=0.f},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_F32, OFFSET(live.max.f), {.f32=1.f},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformvec2_params[] = {
    {"value",    NGLI_PARAM_TYPE_VEC2, OFFSET(live.val.f),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformvec2_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_VEC2, OFFSET(live.min.f), {.vec={0.f, 0.f}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_VEC2, OFFSET(live.max.f), {.vec={1.f, 1.f}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformvec3_params[] = {
    {"value",    NGLI_PARAM_TYPE_VEC3, OFFSET(live.val.f),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformvec3_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_VEC3, OFFSET(live.min.f), {.vec={0.f, 0.f, 0.f}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_VEC3, OFFSET(live.max.f), {.vec={1.f, 1.f, 1.f}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformvec4_params[] = {
    {"value",    NGLI_PARAM_TYPE_VEC4, OFFSET(live.val.f),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformvec4_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_VEC4, OFFSET(live.min.f), {.vec={0.f, 0.f, 0.f, 0.f}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_VEC4, OFFSET(live.max.f), {.vec={1.f, 1.f, 1.f, 1.f}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformint_params[] = {
    {"value",    NGLI_PARAM_TYPE_I32, OFFSET(live.val.i),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformint_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_I32, OFFSET(live.min.i), {.i32=-100},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_I32, OFFSET(live.max.i), {.i32=100},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformivec2_params[] = {
    {"value",    NGLI_PARAM_TYPE_IVEC2, OFFSET(live.val.i),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformivec2_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_IVEC2, OFFSET(live.min.i), {.ivec={-100, -100}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_IVEC2, OFFSET(live.max.i), {.ivec={100, 100}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformivec3_params[] = {
    {"value",    NGLI_PARAM_TYPE_IVEC3, OFFSET(live.val.i),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformivec3_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_IVEC3, OFFSET(live.min.i), {.ivec={-100, -100, -100}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_IVEC3, OFFSET(live.max.i), {.ivec={100, 100, 100}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformivec4_params[] = {
    {"value",    NGLI_PARAM_TYPE_IVEC4, OFFSET(live.val.i),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformivec4_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_IVEC4, OFFSET(live.min.i), {.ivec={-100, -100, -100, -100}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_IVEC4, OFFSET(live.max.i), {.ivec={100, 100, 100, 100}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformuint_params[] = {
    {"value",    NGLI_PARAM_TYPE_U32, OFFSET(live.val.u),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformuint_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_U32, OFFSET(live.min.u), {.u32=0},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_U32, OFFSET(live.max.u), {.u32=100},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformuivec2_params[] = {
    {"value",    NGLI_PARAM_TYPE_UVEC2, OFFSET(live.val.u),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformuvec2_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_UVEC2, OFFSET(live.min.u), {.uvec={0, 0}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_UVEC2, OFFSET(live.max.u), {.uvec={100, 100}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformuivec3_params[] = {
    {"value",    NGLI_PARAM_TYPE_UVEC3, OFFSET(live.val.u),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformuvec3_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_UVEC3, OFFSET(live.min.u), {.uvec={0, 0, 0}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_UVEC3, OFFSET(live.max.u), {.uvec={100, 100, 100}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformuivec4_params[] = {
    {"value",    NGLI_PARAM_TYPE_UVEC4, OFFSET(live.val.u),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformuvec4_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_UVEC4, OFFSET(live.min.u), {.uvec={0, 0, 0, 0}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_UVEC4, OFFSET(live.max.u), {.uvec={100, 100, 100, 100}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformcolor_params[] = {
    {"value",    NGLI_PARAM_TYPE_VEC3, OFFSET(live.val.f),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformvec4_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_VEC3, OFFSET(live.min.f), {.vec={0.f, 0.f, 0.f}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_VEC3, OFFSET(live.max.f), {.vec={1.f, 1.f, 1.f}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformcolora_params[] = {
    {"value",    NGLI_PARAM_TYPE_VEC4, OFFSET(live.val.f),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=uniformvec4_update_func,
                 .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_VEC4, OFFSET(live.min.f), {.vec={0.f, 0.f, 0.f, 0.f}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_VEC4, OFFSET(live.max.f), {.vec={1.f, 1.f, 1.f, 1.f}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {NULL}
};

static const struct node_param uniformquat_params[] = {
    {"value",  NGLI_PARAM_TYPE_VEC4, OFFSET(live.val.f), {.vec=NGLI_QUAT_IDENTITY},
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=uniformquat_update_func,
               .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_VEC4, OFFSET(live.min.f), {.vec={-1.f, -1.f, -1.f, -1.f}},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_VEC4, OFFSET(live.max.f), {.vec={1.f, 1.f, 1.f, 1.f}},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only honored when live_id is set)")},
    {"as_mat4", NGLI_PARAM_TYPE_BOOL, OFFSET(as_mat4), {.i32=0},
                .desc=NGLI_DOCSTRING("exposed as a 4x4 rotation matrix in the program")},
    {NULL}
};

static const struct node_param uniformmat4_params[] = {
    {"value",     NGLI_PARAM_TYPE_MAT4, OFFSET(live.val.m), {.mat=NGLI_MAT4_IDENTITY},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                  .update_func=uniformmat4_update_func,
                  .desc=NGLI_DOCSTRING("value exposed to the shader")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"transform", NGLI_PARAM_TYPE_NODE, OFFSET(transform), .node_types=TRANSFORM_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("`value` transformation chain")},
    {NULL}
};

#define uniformbool_update   NULL
#define uniformfloat_update  NULL
#define uniformvec2_update   NULL
#define uniformvec3_update   NULL
#define uniformvec4_update   NULL
#define uniformquat_update   NULL
#define uniformint_update    NULL
#define uniformivec2_update  NULL
#define uniformivec3_update  NULL
#define uniformivec4_update  NULL
#define uniformuint_update   NULL
#define uniformuivec2_update NULL
#define uniformuivec3_update NULL
#define uniformuivec4_update NULL
#define uniformcolor_update  NULL
#define uniformcolora_update NULL

static int uniformmat4_update(struct ngl_node *node, double t)
{
    struct uniform_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    if (o->transform) {
        int ret = ngli_node_update(o->transform, t);
        if (ret < 0)
            return ret;
        ngli_transform_chain_compute(o->transform, s->matrix);
    }
    return 0;
}

#define DECLARE_INIT_FUNC(type, dtype, count, dst, src)     \
static int uniform##type##_init(struct ngl_node *node)      \
{                                                           \
    struct uniform_priv *s = node->priv_data;               \
    const struct variable_opts *o = node->opts;             \
    s->var.data = dst;                                      \
    s->var.data_size = count * sizeof(*dst);                \
    s->var.data_type = dtype;                               \
    memcpy(s->var.data, src, s->var.data_size);             \
    return 0;                                               \
}

DECLARE_INIT_FUNC(bool,   NGLI_TYPE_BOOL,   1, s->ivector, o->live.val.i)
DECLARE_INIT_FUNC(int,    NGLI_TYPE_INT,    1, s->ivector, o->live.val.i)
DECLARE_INIT_FUNC(ivec2,  NGLI_TYPE_IVEC2,  2, s->ivector, o->live.val.i)
DECLARE_INIT_FUNC(ivec3,  NGLI_TYPE_IVEC3,  3, s->ivector, o->live.val.i)
DECLARE_INIT_FUNC(ivec4,  NGLI_TYPE_IVEC4,  4, s->ivector, o->live.val.i)
DECLARE_INIT_FUNC(uint,   NGLI_TYPE_UINT,   1, s->uvector, o->live.val.u)
DECLARE_INIT_FUNC(uivec2, NGLI_TYPE_UIVEC2, 2, s->uvector, o->live.val.u)
DECLARE_INIT_FUNC(uivec3, NGLI_TYPE_UIVEC3, 3, s->uvector, o->live.val.u)
DECLARE_INIT_FUNC(uivec4, NGLI_TYPE_UIVEC4, 4, s->uvector, o->live.val.u)
DECLARE_INIT_FUNC(float,  NGLI_TYPE_FLOAT,  1, s->vector,  o->live.val.f)
DECLARE_INIT_FUNC(vec2,   NGLI_TYPE_VEC2,   2, s->vector,  o->live.val.f)
DECLARE_INIT_FUNC(vec3,   NGLI_TYPE_VEC3,   3, s->vector,  o->live.val.f)
DECLARE_INIT_FUNC(vec4,   NGLI_TYPE_VEC4,   4, s->vector,  o->live.val.f)
DECLARE_INIT_FUNC(color,  NGLI_TYPE_VEC3,   3, s->vector,  o->live.val.f)
DECLARE_INIT_FUNC(colora, NGLI_TYPE_VEC4,   4, s->vector,  o->live.val.f)

static int uniformquat_init(struct ngl_node *node)
{
    struct uniform_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;

    s->var.data = s->vector;
    s->var.data_size = 4 * sizeof(*s->vector);
    s->var.data_type = NGLI_TYPE_VEC4;
    memcpy(s->var.data, o->live.val.f, s->var.data_size);
    if (o->as_mat4) {
        s->var.data = s->matrix;
        s->var.data_size = sizeof(s->matrix);
        s->var.data_type = NGLI_TYPE_MAT4;
        ngli_mat4_rotate_from_quat(s->matrix, s->vector, NULL);
    }
    return 0;
}

static int uniformmat4_init(struct ngl_node *node)
{
    struct uniform_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;

    int ret = ngli_transform_chain_check(o->transform);
    if (ret < 0)
        return ret;

    s->var.data = s->matrix;
    s->var.data_size = sizeof(s->matrix);
    s->var.data_type = NGLI_TYPE_MAT4;
    /* Note: we assume here that a transformation chain includes at least one
     * dynamic transform. We could crawl the chain to figure it out in the
     * details, but that would be limited since we would have to also detect
     * live changes in any of the transform node at update as well. That extra
     * complexity is probably not worth just for handling the case of a static
     * transformation list. */
    s->var.dynamic = !!o->transform;
    memcpy(s->var.data, o->live.val.m, s->var.data_size);
    return 0;
}

#define DEFINE_UNIFORM_CLASS(class_id, class_name, type)        \
const struct node_class ngli_uniform##type##_class = {          \
    .id             = class_id,                                 \
    .category       = NGLI_NODE_CATEGORY_VARIABLE,              \
    .name           = class_name,                               \
    .init           = uniform##type##_init,                     \
    .update         = uniform##type##_update,                   \
    .opts_size      = sizeof(struct variable_opts),             \
    .priv_size      = sizeof(struct uniform_priv),              \
    .params         = uniform##type##_params,                   \
    .flags          = NGLI_NODE_FLAG_LIVECTL,                   \
    .livectl_offset = OFFSET(live),                             \
    .file           = __FILE__,                                 \
};

DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMBOOL,   "UniformBool",   bool)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMFLOAT,  "UniformFloat",  float)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMVEC2,   "UniformVec2",   vec2)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMVEC3,   "UniformVec3",   vec3)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMVEC4,   "UniformVec4",   vec4)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMQUAT,   "UniformQuat",   quat)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMINT,    "UniformInt",    int)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMIVEC2,  "UniformIVec2",  ivec2)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMIVEC3,  "UniformIVec3",  ivec3)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMIVEC4,  "UniformIVec4",  ivec4)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMUINT,   "UniformUInt",   uint)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMUIVEC2, "UniformUIVec2", uivec2)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMUIVEC3, "UniformUIVec3", uivec3)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMUIVEC4, "UniformUIVec4", uivec4)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMMAT4,   "UniformMat4",   mat4)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMCOLOR,  "UniformColor",  color)
DEFINE_UNIFORM_CLASS(NGL_NODE_UNIFORMCOLORA, "UniformColorA", colora)
