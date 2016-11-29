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
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct animkeyframe, x)
static const struct node_param animkeyframescalar_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_DBL,  OFFSET(scalar), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {NULL}
};

static const struct node_param animkeyframevec2_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC2, OFFSET(value),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {NULL}
};

static const struct node_param animkeyframevec3_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC3, OFFSET(value),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {NULL}
};

static const struct node_param animkeyframevec4_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC4, OFFSET(value),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {NULL}
};

#ifdef __ANDROID__
   #define log2(x)  (log(x) / log(2))
#endif

// Create a easing :
// 1) define function with optional default parameters
// 2) define optional function resolution with optional default parameters
// 3) need to add it in easings[] :
//      {"function",          function,         function_resolution},
//
// Template without parameters :
// static easing_type function(easing_type t, int args_nb, const easing_type *args)
// {
//     return f(t);
// }
// static easing_type function_resolution(easing_type v, int args_nb, const easing_type *args)
// {
//     return r(v);
// }
//
// template with parameters :
// static easing_type function(easing_type t, int args_nb, const easing_type *args)
// {
//     DEFAULT_PARAMETER(0, my_paramater, 1.0)
//     DEFAULT_PARAMETER(1, second_paramater, 0.5)
//     return f(t, my_paramater, second_parameter);
// }
// static easing_type function_resolution(easing_type v, int args_nb, const easing_type *args)
// {
//     DEFAULT_PARAMETER(0, my_paramater, 1.0)
//     DEFAULT_PARAMETER(1, second_paramater, 0.5)
//     return r(v, my_paramater, second_parameter);
// }


// useful macros to declare simple easings using only one line
// Transformations in/out/in_out/out_in
#define TRANSFORM_IN(function) function(x)
#define TRANSFORM_OUT(function) (1.0 - function(1.0 - x))
#define TRANSFORM_IN_OUT(function) (x < 0.5 ? function(2.0 * x) / 2.0 \
                                            : 1.0 - function(2.0 * (1.0 - x)) / 2.0)
#define TRANSFORM_OUT_IN(function) (x < 0.5 ? (1.0 - function(1.0 - 2.0 * x)) / 2.0 \
                                            : (1.0 + function(2.0 * x - 1.0)) / 2.0)

// Easing or resolution declaration
#define DECLARE_EASING(base_name, name, transform)                            \
static easing_type name(easing_type x, int args_nb, const easing_type *args)  \
{                                                                             \
    return transform(base_name##_helper);                                     \
}

// Easing or resolution helper
#define DECLARE_HELPER(base_name, formula)                              \
static inline easing_type base_name##_helper(easing_type x)             \
{                                                                       \
    return formula;                                                     \
}

// Declarations of all easings or resolutions
#define DECLARE_EASINGS(base_name, suffix, formula) \
DECLARE_HELPER(base_name##suffix, formula) \
DECLARE_EASING(base_name##suffix, base_name##_in##suffix,       TRANSFORM_IN)       \
DECLARE_EASING(base_name##suffix, base_name##_out##suffix,      TRANSFORM_OUT)      \
DECLARE_EASING(base_name##suffix, base_name##_in_out##suffix,   TRANSFORM_IN_OUT)   \
DECLARE_EASING(base_name##suffix, base_name##_out_in##suffix,   TRANSFORM_OUT_IN)

// Easings + resolutions declarations
#define DECLARE_EASINGS_WITH_RESOLUTIONS(base_name, direct_function, resolution_function)   \
DECLARE_EASINGS(base_name,            , direct_function)                                    \
DECLARE_EASINGS(base_name, _resolution, resolution_function)

// Default parameter(s)
#define DEFAULT_PARAMETER(index, name, default_value) easing_type name = args_nb > index ? args[index] : default_value


// Easings

// Linear
static easing_type linear(easing_type t, int args_nb, const easing_type *args)
{
    return t;
}
static easing_type linear_resolution(easing_type v, int args_nb, const easing_type *args)
{
    return v;
}

DECLARE_EASINGS_WITH_RESOLUTIONS(quadratic, x * x ,             sqrt(x))
DECLARE_EASINGS_WITH_RESOLUTIONS(cubic,     x * x * x,          pow(x, 1.0 / 3.0))
DECLARE_EASINGS_WITH_RESOLUTIONS(quartic,   x * x * x * x,      pow(x, 1.0 / 4.0))
DECLARE_EASINGS_WITH_RESOLUTIONS(quintic,   x * x * x * x * x,  pow(x, 1.0 / 5.0))

DECLARE_EASINGS_WITH_RESOLUTIONS(sinus, 1.0 - cos(x * M_PI / 2.0), acos(1.0 - x) / M_PI * 2.0)
DECLARE_EASINGS_WITH_RESOLUTIONS(circular, 1.0 - sqrt(1.0 - x * x), sqrt(x*(2.0 - x)))


// Exponential
static inline easing_type exp_helper(easing_type x, easing_type exp_base)
{
    return (pow(exp_base, x) - 1.0) / (exp_base - 1.0);
}
static inline easing_type exp_resolution_helper(easing_type x, easing_type exp_base)
{
    return log2(x * (exp_base - 1.0) + 1.0 ) / log2(exp_base);
}

static easing_type exp_in(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    return exp_helper(t, exp_base);
}
static easing_type exp_in_resolution(easing_type v, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    return exp_resolution_helper(v, exp_base);
}

static easing_type exp_out(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    return 1.0 - exp_helper(1.0 - t, exp_base);
}
static easing_type exp_out_resolution(easing_type v, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    return 1.0 - exp_resolution_helper(1.0 - v, exp_base);
}

static easing_type exp_in_out(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    if (t < 0.5)
        return exp_helper(2.0 * t, exp_base) / 2.0;
    else
        return 1.0 - exp_helper(2.0 * (1.0 - t), exp_base) / 2.0;
}
static easing_type exp_in_out_resolution(easing_type v, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    if (v < 0.5)
        return exp_resolution_helper(2.0 * v, exp_base) / 2.0;
    else
        return 1.0 - exp_resolution_helper(2.0 * (1.0 - v), exp_base) / 2.0;
}

static easing_type exp_out_in(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    if (t < 0.5)
        return (1.0 - exp_helper(1.0 - 2.0 * t, exp_base)) / 2.0;
    else
        return (1.0 + exp_helper(2.0 * t - 1.0, exp_base)) / 2.0;
}
static easing_type exp_out_in_resolution(easing_type v, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, exp_base, 1024.0);
    if (v < 0.5)
        return (1.0 - exp_resolution_helper(1.0 - 2.0 * v, exp_base)) / 2.0;
    else
        return (1.0 + exp_resolution_helper(2.0 * v - 1.0, exp_base)) / 2.0;
}


// Bounce
static easing_type bounce_out_helper(easing_type t, easing_type c, easing_type a)
{
    if (t == 1.0)
        return c;
    if (t < 4.0 / 11.0)
        return c * (7.5625 * t * t);
    if (t < 8.0 / 11.0) {
        t -= 6.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.75)) + c;
    } else if (t < 10.0 / 11.0) {
        t -= 9.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.9375)) + c;
    } else {
        t -= (21.0 / 22.0);
        return -a * (1.0 - (7.5625 * t * t + 0.984375)) + c;
    }
}

static easing_type bounce_in(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, a, 1.70158);
    return 1.0 - bounce_out_helper(1.0 - t, 1.0, a);
}

static easing_type bounce_out(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, a, 1.70158);
    return bounce_out_helper(t, 1.0, a);
}


// Elastic
static easing_type elastic_in_helper(easing_type t, easing_type b, easing_type c, easing_type d, easing_type a, easing_type p)
{
    if (t == 0.0)
        return b;
    easing_type t_adj = t / d;
    if (t_adj == 1.0)
        return b + c;
    easing_type s = 0.0;
    if (a < fabs(c)) {
        a = c;
        s = p / 4.0;
    } else {
        s = p / (2.0 * M_PI) * asin(c / a);
    }
    t_adj -= 1.0;
    return -(a * exp2(10.0 * t_adj) * sin((t_adj * d - s) * (2.0 * M_PI) / p)) + b;
}

static easing_type elastic_in(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, amplitude, 0.1);
    DEFAULT_PARAMETER(1, period, 0.25);
    return elastic_in_helper(t, 0.0, 1.0, 1.0, amplitude, period);
}

static easing_type elastic_out_helper(easing_type t, easing_type b, easing_type c, easing_type d, easing_type a, easing_type p)
{
    if (t <= 0.0)
        return 0.0;
    if (t >= 1.0)
        return c;
    easing_type s = 0.0;
    if (a < c) {
        a = c;
        s = p / 4.0;
    } else {
        s = p / (2.0 * M_PI) * asin(c / a);
    }
    return a * exp2(-10.0 * t) * sin((t - s) * (2 * M_PI)/p ) + c;
}

static easing_type elastic_out(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, amplitude, 0.1);
    DEFAULT_PARAMETER(1, period, 0.25);
    return elastic_out_helper(t, 0.0, 1.0, 1.0, amplitude, period);
}


// Back
static easing_type back_in(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, s, 1.70158);
    return t * t * ((s + 1.0) * t - s);
}

static easing_type back_out(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, s, 1.70158);
    t -= 1.0;
    return t * t * ((s + 1.0) * t + s) + 1.0;
}

static easing_type back_in_out(easing_type t, int args_nb, const easing_type *args)
{
    DEFAULT_PARAMETER(0, s, 1.70158);
    t *= 2.0;
    if (t < 1.0) {
        s *= 1.525;
        return t * t * ((s + 1.0) * t - s) / 2.0;
    }
    t -= 2.0;
    s *= 1.525;
    return (t * t * ((s + 1.0) * t + s) + 2.0) / 2.0;
}

static easing_type back_out_in(easing_type t, int args_nb, const easing_type *args)
{
    if (t < 0.5)
        return back_out(2.0 * t, args_nb, args) / 2.0;
    return (back_in(2.0 * t - 1.0, args_nb, args) + 1.0) / 2.0;
}

static const struct {
    const char *name;
    easing_function function;
    easing_function resolution;
} easings[] = {
    {"linear",              linear,                 linear_resolution},

    {"quadratic_in",        quadratic_in,           quadratic_in_resolution},
    {"quadratic_out",       quadratic_out,          quadratic_out_resolution},
    {"quadratic_in_out",    quadratic_in_out,       quadratic_in_out_resolution},
    {"quadratic_out_in",    quadratic_out_in,       quadratic_out_in_resolution},

    {"cubic_in",            cubic_in,               cubic_in_resolution},
    {"cubic_out",           cubic_out,              cubic_out_resolution},
    {"cubic_in_out",        cubic_in_out,           cubic_in_out_resolution},
    {"cubic_out_in",        cubic_out_in,           cubic_out_in_resolution},

    {"quartic_in",          quartic_in,             quartic_in_resolution},
    {"quartic_out",         quartic_out,            quartic_out_resolution},
    {"quartic_in_out",      quartic_in_out,         quartic_in_out_resolution},
    {"quartic_out_in",      quartic_out_in,         quartic_out_in_resolution},

    {"quintic_in",          quintic_in,             quintic_in_resolution},
    {"quintic_out",         quintic_out,            quintic_out_resolution},
    {"quintic_in_out",      quintic_in_out,         quintic_in_out_resolution},
    {"quintic_out_in",      quintic_out_in,         quintic_out_in_resolution},

    {"sinus_in",            sinus_in,               sinus_in_resolution},
    {"sinus_out",           sinus_out,              sinus_out_resolution},
    {"sinus_in_out",        sinus_in_out,           sinus_in_out_resolution},
    {"sinus_out_in",        sinus_out_in,           sinus_out_in_resolution},

    {"exp_in",              exp_in,                 exp_in_resolution},
    {"exp_out",             exp_out,                exp_out_resolution},
    {"exp_in_out",          exp_in_out,             exp_in_out_resolution},
    {"exp_out_in",          exp_out_in,             exp_out_in_resolution},

    {"circular_in",         circular_in,            circular_in_resolution},
    {"circular_out",        circular_out,           circular_out_resolution},
    {"circular_in_out",     circular_in_out,        circular_in_out_resolution},
    {"circular_out_in",     circular_out_in,        circular_out_in_resolution},

    {"bounce_in",           bounce_in,              NULL},
    {"bounce_out",          bounce_out,             NULL},

    {"elastic_in",          elastic_in,             NULL},
    {"elastic_out",         elastic_out,            NULL},

    {"back_in",             back_in,                NULL},
    {"back_out",            back_out,               NULL},
    {"back_in_out",         back_in_out,            NULL},
    {"back_out_in",         back_out_in,            NULL},
};

static int get_easing_id(const char *interpolation_type)
{
    for (int i = 0; i < NGLI_ARRAY_NB(easings); i++)
        if (strncmp(interpolation_type, easings[i].name, strlen(easings[i].name)) == 0)
            return i;
    return -1;
}

static int animkeyframe_init(struct ngl_node *node)
{
    struct animkeyframe *s = node->priv_data;

    const int easing_id = get_easing_id(s->easing);
    const char *p = strchr(s->easing, ':');

    if (easing_id < 0) {
        LOG(ERROR, "easing '%.*s' not found",
            (int)strcspn(s->easing, ":"), s->easing);
        return -1;
    }

    if (node->class->id == NGL_NODE_ANIMKEYFRAMEVEC2)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f) for t=%f",
            node->class->name, easings[easing_id].name,
            s->value[0], s->value[1], s->time);
    else if (node->class->id == NGL_NODE_ANIMKEYFRAMEVEC3)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f,%f) for t=%f",
            node->class->name, easings[easing_id].name,
            s->value[0], s->value[1], s->value[2], s->time);
    else if (node->class->id == NGL_NODE_ANIMKEYFRAMEVEC4)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f,%f,%f) for t=%f",
            node->class->name, easings[easing_id].name,
            s->value[0], s->value[1], s->value[2], s->value[3], s->time);
    else if (node->class->id == NGL_NODE_ANIMKEYFRAMESCALAR)
        LOG(VERBOSE, "%s of type %s starting at %f for t=%f",
            node->class->name, easings[easing_id].name,
            s->scalar, s->time);
    else
        return -1;

    while (p && *p) {
        p++;
        if (s->nb_args >= ANIMKF_MAX_ARGS) {
            LOG(ERROR, "Maximum number of arguments reached");
            return -1;
        }
        s->args[s->nb_args++] = strtod(p, (char **)&p);
    }

    s->function   = easings[easing_id].function;
    s->resolution = easings[easing_id].resolution;
    return 0;
}

static int get_kf_id(struct ngl_node **animkf, int nb_animkf, int start, double t)
{
    int ret = -1;

    for (int i = start; i < nb_animkf; i++) {
        const struct animkeyframe *kf = animkf[i]->priv_data;
        if (kf->time >= t)
            break;
        ret = i;
    }
    return ret;
}

#define MIX(x, y, a) ((x)*(1.-(a)) + (y)*(a))

void ngli_animkf_interpolate(float *dst, struct ngl_node **animkf, int nb_animkf,
                             int *current_kf, double t)
{
    const int vec_lens[] = {
        [NGL_NODE_ANIMKEYFRAMEVEC4] = 4,
        [NGL_NODE_ANIMKEYFRAMEVEC3] = 3,
        [NGL_NODE_ANIMKEYFRAMEVEC2] = 2,
    };
    const int class_id = animkf[0]->class->id;
    int kf_id = get_kf_id(animkf, nb_animkf, *current_kf, t);
    if (kf_id < 0)
        kf_id = get_kf_id(animkf, nb_animkf, 0, t);
    if (kf_id >= 0 && kf_id < nb_animkf-1) {
        const struct animkeyframe *kf0 = animkf[kf_id  ]->priv_data;
        const struct animkeyframe *kf1 = animkf[kf_id+1]->priv_data;
        const double t0 = kf0->time;
        const double t1 = kf1->time;
        const double tnorm = (t - t0) / (t1 - t0);
        const double ratio = kf0->function(tnorm, kf0->nb_args, kf0->args);
        *current_kf = kf_id;
        if (class_id == NGL_NODE_ANIMKEYFRAMESCALAR)
            dst[0] = MIX(kf0->scalar, kf1->scalar, ratio);
        else
            for (int i = 0; i < vec_lens[class_id]; i++)
                dst[i] = MIX(kf0->value[i], kf1->value[i], ratio);
    } else {
        const struct animkeyframe *kf0 = animkf[          0]->priv_data;
        const struct animkeyframe *kfn = animkf[nb_animkf-1]->priv_data;
        const struct animkeyframe *kf  = t <= kf0->time ? kf0 : kfn;
        if (class_id == NGL_NODE_ANIMKEYFRAMEVEC3)
            memcpy(dst, kf->value, vec_lens[class_id] * sizeof(*dst));
        else
            dst[0] = kf->scalar;
    }
}

static char *animkeyframe_info_str_scalar(const struct ngl_node *node)
{
    const struct animkeyframe *s = node->priv_data;
    return ngli_asprintf("%s at %g with v=%g", s->easing, s->time, s->scalar);
}

static char *animkeyframe_info_str_vec2(const struct ngl_node *node)
{
    const struct animkeyframe *s = node->priv_data;
    return ngli_asprintf("%s at %g with (%g,%g)", s->easing, s->time,
                         s->value[0], s->value[1]);
}

static char *animkeyframe_info_str_vec3(const struct ngl_node *node)
{
    const struct animkeyframe *s = node->priv_data;
    return ngli_asprintf("%s at %g with (%g,%g,%g)", s->easing, s->time,
                         s->value[0], s->value[1], s->value[2]);
}

static char *animkeyframe_info_str_vec4(const struct ngl_node *node)
{
    const struct animkeyframe *s = node->priv_data;
    return ngli_asprintf("%s at %g with (%g,%g,%g,%g)", s->easing, s->time,
                         s->value[0], s->value[1], s->value[2], s->value[3]);
}

const struct node_class ngli_animkeyframescalar_class = {
    .id        = NGL_NODE_ANIMKEYFRAMESCALAR,
    .name      = "AnimKeyFrameScalar",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str_scalar,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframescalar_params,
};

const struct node_class ngli_animkeyframevec2_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEVEC2,
    .name      = "AnimKeyFrameVec2",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str_vec2,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframevec2_params,
};

const struct node_class ngli_animkeyframevec3_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEVEC3,
    .name      = "AnimKeyFrameVec3",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str_vec3,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframevec3_params,
};

const struct node_class ngli_animkeyframevec4_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEVEC4,
    .name      = "AnimKeyFrameVec4",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str_vec4,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframevec4_params,
};
