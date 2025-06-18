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
#include <string.h>

#include "eval.h"
#include "internal.h"
#include "log.h"
#include "ngpu/type.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "utils/hmap.h"

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
                                            NGL_NODE_NOISEVEC2,       \
                                            NGL_NODE_NOISEVEC3,       \
                                            NGL_NODE_NOISEVEC4,       \
                                            NGL_NODE_EVALFLOAT,       \
                                            NGL_NODE_EVALVEC2,        \
                                            NGL_NODE_EVALVEC3,        \
                                            NGL_NODE_EVALVEC4,        \
                                            NGL_NODE_UNIFORMFLOAT,    \
                                            NGL_NODE_UNIFORMVEC2,     \
                                            NGL_NODE_UNIFORMVEC3,     \
                                            NGL_NODE_UNIFORMVEC4,     \
                                            NGL_NODE_UNIFORMCOLOR,    \
                                            NGL_NODE_ANIMATEDFLOAT,   \
                                            NGL_NODE_ANIMATEDVEC2,    \
                                            NGL_NODE_ANIMATEDVEC3,    \
                                            NGL_NODE_ANIMATEDVEC4,    \
                                            NGL_NODE_STREAMEDFLOAT,   \
                                            NGL_NODE_STREAMEDVEC2,    \
                                            NGL_NODE_STREAMEDVEC3,    \
                                            NGL_NODE_STREAMEDVEC4,    \
                                            NGL_NODE_TIME,            \
                                            NGL_NODE_VELOCITYFLOAT,   \
                                            NGL_NODE_VELOCITYVEC2,    \
                                            NGL_NODE_VELOCITYVEC3,    \
                                            NGL_NODE_VELOCITYVEC4,    \
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

NGLI_STATIC_ASSERT(offsetof(struct eval_priv, var) == 0, "variable_info is first");

static const char * const comp_selectors[] = {"0123", "rgba", "xyzw", "stpq"};

static int register_component_names(struct hmap *vars, const char *base_name, size_t nb_components, float *ptr)
{
    ngli_assert(nb_components > 1);

    char name[256];
    for (size_t j = 0; j < NGLI_ARRAY_NB(comp_selectors); j++) {
        const char *selectors = comp_selectors[j];
        for (size_t i = 0; i < nb_components; i++) {
            const char component = selectors[i];
            int len = snprintf(name, sizeof(name), "%s.%c", base_name, component);
            if (len >= sizeof(name)) {
                LOG(ERROR, "resource name \"%s\" is too long", base_name);
                return NGL_ERROR_LIMIT_EXCEEDED;
            }
            int ret = ngli_hmap_set_str(vars, name, ptr + i);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int eval_init(struct ngl_node *node)
{
    struct eval_priv *s = node->priv_data;
    const struct eval_opts *o = node->opts;

    s->vars = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!s->vars)
        return NGL_ERROR_MEMORY;

    if (o->resources) {
        struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(o->resources, entry))) {
            struct ngl_node *res = entry->data;
            struct variable_info *var = res->priv_data;
            int ret;
            if (var->data_type == NGPU_TYPE_F32) {
                ret = ngli_hmap_set_str(s->vars, entry->key.str, var->data);
            } else if (var->data_type == NGPU_TYPE_VEC2) {
                ret = register_component_names(s->vars, entry->key.str, 2, var->data);
            } else if (var->data_type == NGPU_TYPE_VEC3) {
                ret = register_component_names(s->vars, entry->key.str, 3, var->data);
            } else if (var->data_type == NGPU_TYPE_VEC4) {
                ret = register_component_names(s->vars, entry->key.str, 4, var->data);
            } else {
                ngli_assert(0);
            }
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
            // expr0 is always mandatory (NGLI_PARAM_FLAG_NON_NULL) while the
            // other expr* are optional
            ngli_assert(i > 0);

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

DEFINE_EVAL_CLASS(NGL_NODE_EVALFLOAT, "EvalFloat", float, NGPU_TYPE_F32,   1)
DEFINE_EVAL_CLASS(NGL_NODE_EVALVEC2,  "EvalVec2",  vec2,  NGPU_TYPE_VEC2,  2)
DEFINE_EVAL_CLASS(NGL_NODE_EVALVEC3,  "EvalVec3",  vec3,  NGPU_TYPE_VEC3,  3)
DEFINE_EVAL_CLASS(NGL_NODE_EVALVEC4,  "EvalVec4",  vec4,  NGPU_TYPE_VEC4,  4)
