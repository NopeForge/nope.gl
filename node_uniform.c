/*
 * Copyright 2016 Clément Bœsch <cboesch@gopro.com>
 * Copyright 2016 Matthieu Bouron <mbouron@gopro.com>
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

#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct uniform, x)
static const struct node_param uniformscalar_params[] = {
    {"name",   PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_DBL,  OFFSET(scalar)},
    {"animkf", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMESCALAR, -1}},
    {NULL}
};

static const struct node_param uniformvec2_params[] = {
    {"name",   PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC2, OFFSET(vector)},
    {"animkf", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC2, -1}},
    {NULL}
};

static const struct node_param uniformvec3_params[] = {
    {"name",   PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC3, OFFSET(vector)},
    {"animkf", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1}},
    {NULL}
};

static const struct node_param uniformvec4_params[] = {
    {"name",   PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_VEC4, OFFSET(vector)},
    {"animkf", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC4, -1}},
    {NULL}
};

static const struct node_param uniformint_params[] = {
    {"name",   PARAM_TYPE_STR, OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"value",  PARAM_TYPE_INT, OFFSET(ival)},
    {NULL}
};

static const struct node_param uniformmat4_params[] = {
    {"name",   PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static const struct node_param uniformsampler_params[] = {
    {"name",   PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"type",   PARAM_TYPE_INT,  OFFSET(type),   {.i64=NGL_UNIFORM_SAMPLER_2D}},
    {NULL}
};

static void uniform_scalar_update(struct ngl_node *node, double t)
{
    struct uniform *s = node->priv_data;
    if (s->nb_animkf) {
        float scalar;
        ngli_animkf_interpolate(&scalar, s->animkf, s->nb_animkf, &s->current_kf, t);
        s->scalar = scalar;
    }
}

static void uniform_vec_update(struct ngl_node *node, double t)
{
    struct uniform *s = node->priv_data;
    if (s->nb_animkf)
        ngli_animkf_interpolate(s->vector, s->animkf, s->nb_animkf, &s->current_kf, t);
}

static int uniform_init(struct ngl_node *node)
{
    struct uniform *s = node->priv_data;
    for (int i = 0; i < s->nb_animkf; i++)
        ngli_node_init(s->animkf[i]);

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
    .update    = uniform_scalar_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformscalar_params,
};

const struct node_class ngli_uniformvec2_class = {
    .id        = NGL_NODE_UNIFORMVEC2,
    .name      = "UniformVec2",
    .init      = uniform_init,
    .update    = uniform_vec_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec2_params,
};

const struct node_class ngli_uniformvec3_class = {
    .id        = NGL_NODE_UNIFORMVEC3,
    .name      = "UniformVec3",
    .init      = uniform_init,
    .update    = uniform_vec_update,
    .priv_size = sizeof(struct uniform),
    .params    = uniformvec3_params,
};

const struct node_class ngli_uniformvec4_class = {
    .id        = NGL_NODE_UNIFORMVEC4,
    .name      = "UniformVec4",
    .init      = uniform_init,
    .update    = uniform_vec_update,
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
    .priv_size = sizeof(struct uniform),
    .params    = uniformmat4_params,
};

const struct node_class ngli_uniformsampler_class = {
    .id        = NGL_NODE_UNIFORMSAMPLER,
    .name      = "UniformSampler",
    .init      = uniform_init,
    .priv_size = sizeof(struct uniform),
    .params    = uniformsampler_params,
};
