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

#include <float.h>
#include <stddef.h>
#include <string.h>

#include "animation.h"
#include "colorconv.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/type.h"
#include "node_animkeyframe.h"
#include "node_uniform.h"
#include "node_velocity.h"
#include "nopegl.h"
#include "path.h"

#define OFFSET(x) offsetof(struct variable_opts, x)
static const struct node_param animatedtime_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEFLOAT, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("time key frames to interpolate from")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedfloat_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEFLOAT, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("float key frames to interpolate from")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedvec2_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEVEC2, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("vec2 key frames to interpolate from")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedvec3_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEVEC3, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("vec3 key frames to interpolate from")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedvec4_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEVEC4, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("vec4 key frames to interpolate from")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedquat_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEQUAT, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("quaternion key frames to interpolate from")},
    {"as_mat4",   NGLI_PARAM_TYPE_BOOL, OFFSET(as_mat4), {.i32=0},
                  .desc=NGLI_DOCSTRING("exposed as a 4x4 rotation matrix in the program")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedpath_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEFLOAT, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("float key frames to interpolate from, representing the normed distance from the start of the `path`")},
    {"path",      NGLI_PARAM_TYPE_NODE, OFFSET(path_node),
                  .node_types=(const uint32_t[]){NGL_NODE_PATH, NGL_NODE_SMOOTHPATH, NGLI_NODE_NONE},
                  .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .desc=NGLI_DOCSTRING("path to follow")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

static const struct node_param animatedcolor_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMECOLOR, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("color key frames to interpolate from")},
    {"space",     NGLI_PARAM_TYPE_SELECT, OFFSET(space), {.i32=NGLI_COLORCONV_SPACE_SRGB},
                  .choices=&ngli_colorconv_colorspace_choices,
                  .desc=NGLI_DOCSTRING("color space defining how to interpret `value`")},
    {"time_offset", NGLI_PARAM_TYPE_F64, OFFSET(time_offset),
                    .desc=NGLI_DOCSTRING("apply a time offset before evaluating the animation")},
    {NULL}
};

struct animated_priv {
    struct variable_info var;
    float vector[4];
    float matrix[4*4];
    double dval;
    struct animation anim;
    struct animation anim_eval;
};

NGLI_STATIC_ASSERT(variable_info_is_first, offsetof(struct animated_priv, var) == 0);

static void mix_time(void *user_arg, void *dst,
                     const struct animkeyframe_opts *kf0,
                     const struct animkeyframe_opts *kf1,
                     double ratio)
{
    double *dstd = dst;
    dstd[0] = NGLI_MIX_F64(kf0->scalar, kf1->scalar, ratio);
}

static void mix_float(void *user_arg, void *dst,
                      const struct animkeyframe_opts *kf0,
                      const struct animkeyframe_opts *kf1,
                      double ratio)
{
    float *dstd = dst;
    dstd[0] = (float)NGLI_MIX_F64(kf0->scalar, kf1->scalar, ratio);
}

static void mix_path(void *user_arg, void *dst,
                     const struct animkeyframe_opts *kf0,
                     const struct animkeyframe_opts *kf1,
                     double ratio)
{
    const float t = (float)NGLI_MIX_F64(kf0->scalar, kf1->scalar, ratio);
    const struct variable_opts *o = user_arg;
    struct path *path = *(struct path **)o->path_node->priv_data;
    ngli_path_evaluate(path, dst, t);
}

static void mix_quat(void *user_arg, void *dst,
                     const struct animkeyframe_opts *kf0,
                     const struct animkeyframe_opts *kf1,
                     double ratio)
{
    ngli_quat_slerp(dst, kf0->value, kf1->value, (float)ratio);
}

static void mix_vector(void *user_arg, void *dst,
                       const struct animkeyframe_opts *kf0,
                       const struct animkeyframe_opts *kf1,
                       double ratio, size_t len)
{
    float *dstf = dst;
    for (size_t i = 0; i < len; i++)
        dstf[i] = NGLI_MIX_F32(kf0->value[i], kf1->value[i], (float)ratio);
}

#define DECLARE_COLOR_MIX_FUNCS(space)                          \
static void mix_##space(void *user_arg, void *dst,              \
                        const struct animkeyframe_opts *kf0,    \
                        const struct animkeyframe_opts *kf1,    \
                        double ratio)                           \
{                                                               \
    float rgb0[3], rgb1[3];                                     \
    ngli_colorconv_##space##2linear(rgb0, kf0->value);          \
    ngli_colorconv_##space##2linear(rgb1, kf1->value);          \
    const float mixed[3] = {                                    \
        NGLI_MIX_F32(rgb0[0], rgb1[0], (float)ratio),           \
        NGLI_MIX_F32(rgb0[1], rgb1[1], (float)ratio),           \
        NGLI_MIX_F32(rgb0[2], rgb1[2], (float)ratio),           \
    };                                                          \
    ngli_colorconv_linear2srgb(dst, mixed);                     \
}                                                               \

DECLARE_COLOR_MIX_FUNCS(srgb)
DECLARE_COLOR_MIX_FUNCS(hsl)
DECLARE_COLOR_MIX_FUNCS(hsv)

#define DECLARE_COLOR_CPY_FUNCS(space)                          \
static void cpy_##space(void *user_arg, void *dst,              \
                        const struct animkeyframe_opts *kf)     \
{                                                               \
    ngli_colorconv_##space##2srgb(dst, kf->value);              \
}

DECLARE_COLOR_CPY_FUNCS(hsl)
DECLARE_COLOR_CPY_FUNCS(hsv)

#define DECLARE_VEC_MIX_AND_CPY_FUNCS(len)                      \
static void mix_vec##len(void *user_arg, void *dst,             \
                         const struct animkeyframe_opts *kf0,   \
                         const struct animkeyframe_opts *kf1,   \
                         double ratio)                          \
{                                                               \
    mix_vector(user_arg, dst, kf0, kf1, ratio, len);            \
}                                                               \
                                                                \
static void cpy_vec##len(void *user_arg, void *dst,             \
                         const struct animkeyframe_opts *kf)    \
{                                                               \
    memcpy(dst, kf->value, len * sizeof(*kf->value));           \
}                                                               \

DECLARE_VEC_MIX_AND_CPY_FUNCS(2)
DECLARE_VEC_MIX_AND_CPY_FUNCS(3)
DECLARE_VEC_MIX_AND_CPY_FUNCS(4)

static void cpy_path(void *user_arg, void *dst,
                     const struct animkeyframe_opts *kf)
{
    const struct variable_opts *o = user_arg;
    struct path *path = *(struct path **)o->path_node->priv_data;
    ngli_path_evaluate(path, dst, (float)kf->scalar);
}

static void cpy_time(void *user_arg, void *dst,
                     const struct animkeyframe_opts *kf)
{
    memcpy(dst, &kf->scalar, sizeof(kf->scalar));
}

static void cpy_scalar(void *user_arg, void *dst,
                       const struct animkeyframe_opts *kf)
{
    *(float *)dst = (float)kf->scalar;  // double → float
}

static ngli_animation_mix_func_type get_color_mix_func(int space)
{
    switch (space) {
    case NGLI_COLORCONV_SPACE_SRGB:  return mix_srgb;
    case NGLI_COLORCONV_SPACE_HSL:   return mix_hsl;
    case NGLI_COLORCONV_SPACE_HSV:   return mix_hsv;
    }
    return NULL;
}

static ngli_animation_cpy_func_type get_color_cpy_func(int space)
{
    switch (space) {
    case NGLI_COLORCONV_SPACE_SRGB:  return cpy_vec3;
    case NGLI_COLORCONV_SPACE_HSL:   return cpy_hsl;
    case NGLI_COLORCONV_SPACE_HSV:   return cpy_hsv;
    }
    return NULL;
}

static ngli_animation_mix_func_type get_mix_func(const struct variable_opts *o, uint32_t node_class)
{
    switch (node_class) {
        case NGL_NODE_ANIMATEDTIME:  return mix_time;
        case NGL_NODE_ANIMATEDFLOAT: return mix_float;
        case NGL_NODE_ANIMATEDVEC2:  return mix_vec2;
        case NGL_NODE_ANIMATEDVEC3:  return mix_vec3;
        case NGL_NODE_ANIMATEDVEC4:  return mix_vec4;
        case NGL_NODE_ANIMATEDQUAT:  return mix_quat;
        case NGL_NODE_ANIMATEDPATH:  return mix_path;
        case NGL_NODE_ANIMATEDCOLOR: return get_color_mix_func(o->space);
    }
    return NULL;
}

static ngli_animation_cpy_func_type get_cpy_func(const struct variable_opts *o, uint32_t node_class)
{
    switch (node_class) {
        case NGL_NODE_ANIMATEDTIME:  return cpy_time;
        case NGL_NODE_ANIMATEDFLOAT: return cpy_scalar;
        case NGL_NODE_ANIMATEDVEC2:  return cpy_vec2;
        case NGL_NODE_ANIMATEDVEC3:  return cpy_vec3;
        case NGL_NODE_ANIMATEDVEC4:  return cpy_vec4;
        case NGL_NODE_ANIMATEDQUAT:  return cpy_vec4;
        case NGL_NODE_ANIMATEDPATH:  return cpy_path;
        case NGL_NODE_ANIMATEDCOLOR: return get_color_cpy_func(o->space);
    }
    return NULL;
}

int ngl_anim_evaluate(struct ngl_node *node, void *dst, double t)
{
    if (node->cls->id == NGL_NODE_VELOCITYFLOAT ||
        node->cls->id == NGL_NODE_VELOCITYVEC2 ||
        node->cls->id == NGL_NODE_VELOCITYVEC3 ||
        node->cls->id == NGL_NODE_VELOCITYVEC4)
        return ngli_velocity_evaluate(node, dst, t);

    if (node->cls->id != NGL_NODE_ANIMATEDFLOAT &&
        node->cls->id != NGL_NODE_ANIMATEDVEC2 &&
        node->cls->id != NGL_NODE_ANIMATEDVEC3 &&
        node->cls->id != NGL_NODE_ANIMATEDVEC4 &&
        node->cls->id != NGL_NODE_ANIMATEDQUAT)
        return NGL_ERROR_INVALID_ARG;

    struct animated_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    if (!o->nb_animkf)
        return NGL_ERROR_INVALID_ARG;

    if (node->cls->id == NGL_NODE_ANIMATEDQUAT && o->as_mat4) {
        LOG(ERROR, "evaluating an AnimatedQuat to a mat4 is not supported");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (!s->anim_eval.kfs) {
        int ret = ngli_animation_init(&s->anim_eval, s,
                                      o->animkf, o->nb_animkf,
                                      get_mix_func(o, node->cls->id),
                                      get_cpy_func(o, node->cls->id));
        if (ret < 0)
            return ret;
    }

    struct animkeyframe_priv *kf0 = o->animkf[0]->priv_data;
    if (!kf0->function) {
        for (size_t i = 0; i < o->nb_animkf; i++) {
            int ret = o->animkf[i]->cls->init(o->animkf[i]);
            if (ret < 0)
                return ret;
        }
    }

    return ngli_animation_evaluate(&s->anim_eval, dst, t - o->time_offset);
}

static int animation_init(struct ngl_node *node)
{
    struct animated_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    s->var.dynamic = 1;
    return ngli_animation_init(&s->anim, node->opts,
                               o->animkf, o->nb_animkf,
                               get_mix_func(o, node->cls->id),
                               get_cpy_func(o, node->cls->id));
}

#define DECLARE_INIT_FUNC(suffix, class_data, class_data_size, class_data_type) \
static int animated##suffix##_init(struct ngl_node *node)                       \
{                                                                               \
    struct animated_priv *s = node->priv_data;                                  \
    s->var.data = class_data;                                                   \
    s->var.data_size = class_data_size;                                         \
    s->var.data_type = class_data_type;                                         \
    return animation_init(node);                                                \
}

DECLARE_INIT_FUNC(float, s->vector,  1 * sizeof(*s->vector), NGPU_TYPE_F32)
DECLARE_INIT_FUNC(vec2,  s->vector,  2 * sizeof(*s->vector), NGPU_TYPE_VEC2)
DECLARE_INIT_FUNC(vec3,  s->vector,  3 * sizeof(*s->vector), NGPU_TYPE_VEC3)
DECLARE_INIT_FUNC(vec4,  s->vector,  4 * sizeof(*s->vector), NGPU_TYPE_VEC4)
DECLARE_INIT_FUNC(color, s->vector,  3 * sizeof(*s->vector), NGPU_TYPE_VEC3)

static int animatedtime_init(struct ngl_node *node)
{
    struct animated_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;

    s->var.data = &s->dval;
    s->var.data_size = sizeof(s->dval);
    s->var.data_type = NGPU_TYPE_NONE;

    // Sanity checks for time animation keyframe
    double prev_time = 0;
    for (size_t i = 0; i < o->nb_animkf; i++) {
        const struct animkeyframe_opts *kf = o->animkf[i]->opts;
        if (kf->easing != EASING_LINEAR) {
            LOG(ERROR, "only linear interpolation is allowed for time animation");
            return NGL_ERROR_INVALID_ARG;
        }
        if (kf->scalar < prev_time) {
            LOG(ERROR, "times must be positive and monotonically increasing: %g < %g",
                kf->scalar, prev_time);
            return NGL_ERROR_INVALID_ARG;
        }
        prev_time = kf->scalar;
    }

    return animation_init(node);
}

static int animatedquat_init(struct ngl_node *node)
{
    struct animated_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;

    s->var.data = s->vector;
    s->var.data_size = 4 * sizeof(*s->vector);
    s->var.data_type = NGPU_TYPE_VEC4;
    if (o->as_mat4) {
        s->var.data = s->matrix;
        s->var.data_size = sizeof(s->matrix);
        s->var.data_type = NGPU_TYPE_MAT4;
    }
    return animation_init(node);
}

static int animatedpath_init(struct ngl_node *node)
{
    struct animated_priv *s = node->priv_data;
    s->var.data = s->vector;
    s->var.data_size = 3 * sizeof(*s->vector);
    s->var.data_type = NGPU_TYPE_VEC3;
    return animation_init(node);
}

static int animation_update(struct ngl_node *node, double t)
{
    struct animated_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    return ngli_animation_evaluate(&s->anim, s->var.data, t - o->time_offset);
}

#define animatedtime_update  animation_update
#define animatedfloat_update animation_update
#define animatedvec2_update  animation_update
#define animatedvec3_update  animation_update
#define animatedvec4_update  animation_update
#define animatedpath_update  animation_update
#define animatedcolor_update animation_update

static int animatedquat_update(struct ngl_node *node, double t)
{
    struct animated_priv *s = node->priv_data;
    const struct variable_opts *o = node->opts;
    int ret = ngli_animation_evaluate(&s->anim, s->vector, t - o->time_offset);
    if (ret < 0)
        return ret;
    if (o->as_mat4)
        ngli_mat4_from_quat(s->matrix, s->vector, NULL);
    return 0;
}

#define DEFINE_ANIMATED_CLASS(class_id, class_name, type)       \
const struct node_class ngli_animated##type##_class = {         \
    .id        = class_id,                                      \
    .category  = NGLI_NODE_CATEGORY_VARIABLE,                   \
    .name      = class_name,                                    \
    .init      = animated##type##_init,                         \
    .update    = animated##type##_update,                       \
    .opts_size = sizeof(struct variable_opts),                  \
    .priv_size = sizeof(struct animated_priv),                  \
    .params    = animated##type##_params,                       \
    .file      = __FILE__,                                      \
};

DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDTIME,  "AnimatedTime",  time)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDFLOAT, "AnimatedFloat", float)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDVEC2,  "AnimatedVec2",  vec2)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDVEC3,  "AnimatedVec3",  vec3)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDVEC4,  "AnimatedVec4",  vec4)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDQUAT,  "AnimatedQuat",  quat)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDPATH,  "AnimatedPath",  path)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDCOLOR, "AnimatedColor", color)
