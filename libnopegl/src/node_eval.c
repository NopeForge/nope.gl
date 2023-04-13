/*
 * Copyright 2021-2022 GoPro Inc.
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
#include <stdlib.h>

#include "eval.h"
#include "hmap.h"
#include "internal.h"
#include "log.h"
#include "nopegl.h"
#include "type.h"

struct eval_opts {
    char *expr[4];
    struct hmap *resources;
};

struct eval_priv {
    struct variable_info var;
    float vector[4];
    size_t nb_expr;
    struct hmap *vars;
    struct eval *eval[4];
};

#define INPUT_TYPES_LIST (const uint32_t[]){NGL_NODE_NOISEFLOAT,      \
                                            NGL_NODE_EVALFLOAT,       \
                                            NGL_NODE_UNIFORMFLOAT,    \
                                            NGL_NODE_ANIMATEDFLOAT,   \
                                            NGL_NODE_STREAMEDFLOAT,   \
                                            NGL_NODE_TIME,            \
                                            NGL_NODE_VELOCITYFLOAT,   \
                                            NGLI_NODE_NONE}

#define OFFSET(x) offsetof(struct eval_opts, x)

static const struct node_param eval_float_params[] = {
    {"expr0",     NGLI_PARAM_TYPE_STR, OFFSET(expr[0]), {.str="0"},
                  .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .desc=NGLI_DOCSTRING("expression to evaluate")},
    {"resources", NGLI_PARAM_TYPE_NODEDICT, OFFSET(resources),
                  .node_types=INPUT_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("resources made accessible to the `expr0`")},
    {NULL}
};

static const struct node_param eval_vec2_params[] = {
    {"expr0",     NGLI_PARAM_TYPE_STR, OFFSET(expr[0]), {.str="0"},
                  .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 1st component")},
    {"expr1",     NGLI_PARAM_TYPE_STR, OFFSET(expr[1]),
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 2nd component")},
    {"resources", NGLI_PARAM_TYPE_NODEDICT, OFFSET(resources),
                  .node_types=INPUT_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("resources made accessible to the `expr0` and `expr1`")},
    {NULL}
};

static const struct node_param eval_vec3_params[] = {
    {"expr0",     NGLI_PARAM_TYPE_STR, OFFSET(expr[0]), {.str="0"},
                  .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 1st component")},
    {"expr1",     NGLI_PARAM_TYPE_STR, OFFSET(expr[1]),
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 2nd component")},
    {"expr2",     NGLI_PARAM_TYPE_STR, OFFSET(expr[2]),
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 3rd component")},
    {"resources", NGLI_PARAM_TYPE_NODEDICT, OFFSET(resources),
                  .node_types=INPUT_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("resources made accessible to the `expr0`, `expr1` and `expr2`")},
    {NULL}
};

static const struct node_param eval_vec4_params[] = {
    {"expr0",     NGLI_PARAM_TYPE_STR, OFFSET(expr[0]), {.str="0"},
                  .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 1st component")},
    {"expr1",     NGLI_PARAM_TYPE_STR, OFFSET(expr[1]),
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 2nd component")},
    {"expr2",     NGLI_PARAM_TYPE_STR, OFFSET(expr[2]),
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 3rd component")},
    {"expr3",     NGLI_PARAM_TYPE_STR, OFFSET(expr[3]),
                  .desc=NGLI_DOCSTRING("expression to evaluate to define 4th component")},
    {"resources", NGLI_PARAM_TYPE_NODEDICT, OFFSET(resources),
                  .node_types=INPUT_TYPES_LIST,
                  .desc=NGLI_DOCSTRING("resources made accessible to the `expr0`, `expr1`, `expr2` and `expr3`")},
    {NULL}
};

NGLI_STATIC_ASSERT(variable_info_is_first, offsetof(struct eval_priv, var) == 0);

static int eval_init(struct ngl_node *node)
{
    struct eval_priv *s = node->priv_data;
    const struct eval_opts *o = node->opts;

    s->vars = ngli_hmap_create();
    if (!s->vars)
        return NGL_ERROR_MEMORY;

    if (o->resources) {
        struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(o->resources, entry))) {
            struct ngl_node *res = entry->data;
            struct variable_info *var = res->priv_data;
            ngli_assert(var->data_type == NGLI_TYPE_F32);
            int ret = ngli_hmap_set(s->vars, entry->key, var->data);
            if (ret < 0)
                return ret;
        }
    }

    for (size_t i = 0; i < s->nb_expr; i++) {
        if (!o->expr[i])
            continue;
        s->eval[i] = ngli_eval_create();
        if (!s->eval[i])
            return NGL_ERROR_MEMORY;
        int ret = ngli_eval_init(s->eval[i], o->expr[i], s->vars);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int eval_update(struct ngl_node *node, double t)
{
    struct eval_priv *s = node->priv_data;
    const struct eval_opts *o = node->opts;

    if (o->resources) {
        struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(o->resources, entry))) {
            int ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    for (size_t i = 0; i < s->nb_expr; i++) {
        if (!s->eval[i]) {
            s->vector[i] = s->vector[i - 1];
            continue;
        }
        int ret = ngli_eval_run(s->eval[i], &s->vector[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void eval_uninit(struct ngl_node *node)
{
    struct eval_priv *s = node->priv_data;

    for (size_t i = 0; i < s->nb_expr; i++)
        ngli_eval_freep(&s->eval[i]);
    ngli_hmap_freep(&s->vars);
}

#define DEFINE_EVAL_CLASS(class_id, class_name, type, dtype, count) \
static int eval##type##_init(struct ngl_node *node)                 \
{                                                                   \
    struct eval_priv *s = node->priv_data;                          \
    s->nb_expr = count;                                             \
    s->var.data = s->vector;                                        \
    s->var.data_size = count * sizeof(float);                       \
    s->var.data_type = dtype;                                       \
    s->var.dynamic = 1;                                             \
    return eval_init(node);                                         \
}                                                                   \
                                                                    \
const struct node_class ngli_eval##type##_class = {                 \
    .id        = class_id,                                          \
    .category  = NGLI_NODE_CATEGORY_VARIABLE,                       \
    .name      = class_name,                                        \
    .init      = eval##type##_init,                                 \
    .update    = eval_update,                                       \
    .uninit    = eval_uninit,                                       \
    .opts_size = sizeof(struct eval_opts),                          \
    .priv_size = sizeof(struct eval_priv),                          \
    .params    = eval_##type##_params,                              \
    .file      = __FILE__,                                          \
};

DEFINE_EVAL_CLASS(NGL_NODE_EVALFLOAT, "EvalFloat", float, NGLI_TYPE_F32,   1)
DEFINE_EVAL_CLASS(NGL_NODE_EVALVEC2,  "EvalVec2",  vec2,  NGLI_TYPE_VEC2,  2)
DEFINE_EVAL_CLASS(NGL_NODE_EVALVEC3,  "EvalVec3",  vec3,  NGLI_TYPE_VEC3,  3)
DEFINE_EVAL_CLASS(NGL_NODE_EVALVEC4,  "EvalVec4",  vec4,  NGLI_TYPE_VEC4,  4)
