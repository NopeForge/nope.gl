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

#include "bstr.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "params.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct animkeyframe, x)
static const struct node_param animkeyframefloat_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_DBL,  OFFSET(scalar), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {"easing_args", PARAM_TYPE_DBLLIST, OFFSET(args)},
    {NULL}
};

static const struct node_param animkeyframevec2_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC2, OFFSET(value),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {"easing_args", PARAM_TYPE_DBLLIST, OFFSET(args)},
    {NULL}
};

static const struct node_param animkeyframevec3_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC3, OFFSET(value),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {"easing_args", PARAM_TYPE_DBLLIST, OFFSET(args)},
    {NULL}
};

static const struct node_param animkeyframevec4_params[] = {
    {"time",   PARAM_TYPE_DBL,  OFFSET(time),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC4, OFFSET(value),  .flags=PARAM_FLAG_CONSTRUCTOR},
    {"easing", PARAM_TYPE_STR,  OFFSET(easing), {.str="linear"}},
    {"easing_args", PARAM_TYPE_DBLLIST, OFFSET(args)},
    {NULL}
};

static const struct node_param animkeyframebuffer_params[] = {
    {"time",        PARAM_TYPE_DBL,     OFFSET(time), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"data",        PARAM_TYPE_DATA,    OFFSET(data)},
    {"easing",      PARAM_TYPE_STR,     OFFSET(easing), {.str="linear"}},
    {"easing_args", PARAM_TYPE_DBLLIST, OFFSET(args)},
    {NULL}
};

#ifdef __ANDROID__
#define log2(x)  (log(x) / log(2))
#endif

#define TRANSFORM_IN(function) function(x)
#define TRANSFORM_OUT(function) (1.0 - function(1.0 - x))
#define TRANSFORM_IN_OUT(function) (x < 0.5 ? function(2.0 * x) / 2.0 \
                                            : 1.0 - function(2.0 * (1.0 - x)) / 2.0)
#define TRANSFORM_OUT_IN(function) (x < 0.5 ? (1.0 - function(1.0 - 2.0 * x)) / 2.0 \
                                            : (1.0 + function(2.0 * x - 1.0)) / 2.0)

#define DECLARE_EASING(base_name, name, transform)                            \
static easing_type name(easing_type x, int args_nb, const easing_type *args)  \
{                                                                             \
    return transform(base_name##_helper);                                     \
}

#define DECLARE_HELPER(base_name, formula)                              \
static inline easing_type base_name##_helper(easing_type x)             \
{                                                                       \
    return formula;                                                     \
}

#define DECLARE_EASINGS(base_name, suffix, formula) \
DECLARE_HELPER(base_name##suffix, formula) \
DECLARE_EASING(base_name##suffix, base_name##_in##suffix,       TRANSFORM_IN)       \
DECLARE_EASING(base_name##suffix, base_name##_out##suffix,      TRANSFORM_OUT)      \
DECLARE_EASING(base_name##suffix, base_name##_in_out##suffix,   TRANSFORM_IN_OUT)   \
DECLARE_EASING(base_name##suffix, base_name##_out_in##suffix,   TRANSFORM_OUT_IN)

#define DECLARE_EASINGS_WITH_RESOLUTIONS(base_name, direct_function, resolution_function)   \
DECLARE_EASINGS(base_name,            , direct_function)                                    \
DECLARE_EASINGS(base_name, _resolution, resolution_function)

#define DEFAULT_PARAMETER(index, default_value) args_nb > index ? args[index] : default_value


/* Linear */

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


/* Exponential */

static inline easing_type exp_helper(easing_type x, easing_type exp_base)
{
    return (pow(exp_base, x) - 1.0) / (exp_base - 1.0);
}

static inline easing_type exp_resolution_helper(easing_type x, easing_type exp_base)
{
    return log2(x * (exp_base - 1.0) + 1.0) / log2(exp_base);
}

static easing_type exp_in(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    return exp_helper(t, exp_base);
}

static easing_type exp_in_resolution(easing_type v, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    return exp_resolution_helper(v, exp_base);
}

static easing_type exp_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    return 1.0 - exp_helper(1.0 - t, exp_base);
}

static easing_type exp_out_resolution(easing_type v, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    return 1.0 - exp_resolution_helper(1.0 - v, exp_base);
}

static easing_type exp_in_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    if (t < 0.5)
        return exp_helper(2.0 * t, exp_base) / 2.0;
    else
        return 1.0 - exp_helper(2.0 * (1.0 - t), exp_base) / 2.0;
}

static easing_type exp_in_out_resolution(easing_type v, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    if (v < 0.5)
        return exp_resolution_helper(2.0 * v, exp_base) / 2.0;
    else
        return 1.0 - exp_resolution_helper(2.0 * (1.0 - v), exp_base) / 2.0;
}

static easing_type exp_out_in(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    if (t < 0.5)
        return (1.0 - exp_helper(1.0 - 2.0 * t, exp_base)) / 2.0;
    else
        return (1.0 + exp_helper(2.0 * t - 1.0, exp_base)) / 2.0;
}

static easing_type exp_out_in_resolution(easing_type v, int args_nb, const easing_type *args)
{
    const easing_type exp_base = DEFAULT_PARAMETER(0, 1024.0);
    if (v < 0.5)
        return (1.0 - exp_resolution_helper(1.0 - 2.0 * v, exp_base)) / 2.0;
    else
        return (1.0 + exp_resolution_helper(2.0 * v - 1.0, exp_base)) / 2.0;
}


/* Bounce */

static easing_type bounce_helper(easing_type t, easing_type c, easing_type a)
{
    if (t == 1.0) {
        return c;
    } else if (t < 4.0 / 11.0) {
        return c * (7.5625 * t * t);
    } else if (t < 8.0 / 11.0) {
        t -= 6.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.75)) + c;
    } else if (t < 10.0 / 11.0) {
        t -= 9.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.9375)) + c;
    } else {
        t -= 21.0 / 22.0;
        return -a * (1.0 - (7.5625 * t * t + 0.984375)) + c;
    }
}

static easing_type bounce_in(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type a = DEFAULT_PARAMETER(0, 1.70158);
    return 1.0 - bounce_helper(1.0 - t, 1.0, a);
}

static easing_type bounce_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type a = DEFAULT_PARAMETER(0, 1.70158);
    return bounce_helper(t, 1.0, a);
}


/* Elastic */

static easing_type elastic_in_helper(easing_type t, easing_type b, easing_type c, easing_type d, easing_type a, easing_type p)
{
    if (t == 0.0)
        return b;
    easing_type t_adj = t / d;
    if (t_adj == 1.0)
        return b + c;
    easing_type s;
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
    const easing_type amplitude = DEFAULT_PARAMETER(0, 0.1);
    const easing_type period    = DEFAULT_PARAMETER(1, 0.25);
    return elastic_in_helper(t, 0.0, 1.0, 1.0, amplitude, period);
}

static easing_type elastic_out_helper(easing_type t, easing_type b, easing_type c, easing_type d, easing_type a, easing_type p)
{
    if (t <= 0.0)
        return 0.0;
    if (t >= 1.0)
        return c;
    easing_type s;
    if (a < c) {
        a = c;
        s = p / 4.0;
    } else {
        s = p / (2.0 * M_PI) * asin(c / a);
    }
    return a * exp2(-10.0 * t) * sin((t - s) * (2 * M_PI) / p) + c;
}

static easing_type elastic_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type amplitude = DEFAULT_PARAMETER(0, 0.1);
    const easing_type period    = DEFAULT_PARAMETER(1, 0.25);
    return elastic_out_helper(t, 0.0, 1.0, 1.0, amplitude, period);
}


/* Back */

static easing_type back_in(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type s = DEFAULT_PARAMETER(0, 1.70158);
    return t * t * ((s + 1.0) * t - s);
}

static easing_type back_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type s = DEFAULT_PARAMETER(0, 1.70158);
    t -= 1.0;
    return t * t * ((s + 1.0) * t + s) + 1.0;
}

static easing_type back_in_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type s = DEFAULT_PARAMETER(0, 1.70158) * 1.525;
    t *= 2.0;
    if (t < 1.0)
        return t * t * ((s + 1.0) * t - s) / 2.0;
    t -= 2.0;
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
        if (strcmp(interpolation_type, easings[i].name) == 0)
            return i;
    return -1;
}

static int animkeyframe_init(struct ngl_node *node)
{
    struct animkeyframe *s = node->priv_data;

    const int easing_id = get_easing_id(s->easing);
    if (easing_id < 0) {
        LOG(ERROR, "easing '%s' not found", s->easing);
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
    else if (node->class->id == NGL_NODE_ANIMKEYFRAMEFLOAT)
        LOG(VERBOSE, "%s of type %s starting at %f for t=%f",
            node->class->name, easings[easing_id].name,
            s->scalar, s->time);
    else if (node->class->id == NGL_NODE_ANIMKEYFRAMEBUFFER)
        LOG(VERBOSE, "%s of type %s starting with t=%f (data size: %d)",
            node->class->name, easings[easing_id].name,
            s->time, s->data_size);
    else
        return -1;

    s->function   = easings[easing_id].function;
    s->resolution = easings[easing_id].resolution;
    return 0;
}

static char *animkeyframe_info_str(const struct ngl_node *node)
{
    const struct animkeyframe *s = node->priv_data;
    const struct node_param *params = node->class->params;
    struct bstr *b = ngli_bstr_create();

    if (!b)
        return NULL;

    ngli_bstr_print(b, "%s @ t=%g ", s->easing, s->time);
    if (s->nb_args) {
        const struct node_param *easing_args_par = ngli_params_find(params, "easing_args");
        ngli_assert(easing_args_par);
        ngli_bstr_print(b, "(args: ");
        ngli_params_bstr_print_val(b, node->priv_data, easing_args_par);
        ngli_bstr_print(b, ") ");
    }

    if (node->class->id == NGL_NODE_ANIMKEYFRAMEBUFFER) {
        ngli_bstr_print(b, "with data size of %dB", s->data_size);
    } else {
        ngli_bstr_print(b, "with v=");
        const struct node_param *val_par = ngli_params_find(params, "value");
        ngli_assert(val_par);
        ngli_params_bstr_print_val(b, node->priv_data, val_par);
    }

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

const struct node_class ngli_animkeyframefloat_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEFLOAT,
    .name      = "AnimKeyFrameFloat",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframefloat_params,
};

const struct node_class ngli_animkeyframevec2_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEVEC2,
    .name      = "AnimKeyFrameVec2",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframevec2_params,
};

const struct node_class ngli_animkeyframevec3_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEVEC3,
    .name      = "AnimKeyFrameVec3",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframevec3_params,
};

const struct node_class ngli_animkeyframevec4_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEVEC4,
    .name      = "AnimKeyFrameVec4",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframevec4_params,
};

const struct node_class ngli_animkeyframebuffer_class = {
    .id        = NGL_NODE_ANIMKEYFRAMEBUFFER,
    .name      = "AnimKeyFrameBuffer",
    .init      = animkeyframe_init,
    .info_str  = animkeyframe_info_str,
    .priv_size = sizeof(struct animkeyframe),
    .params    = animkeyframebuffer_params,
};
