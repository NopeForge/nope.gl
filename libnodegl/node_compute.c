/*
 * Copyright 2017 GoPro Inc.
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
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "hmap.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "pass.h"
#include "utils.h"

struct compute_priv {
    int nb_group_x;
    int nb_group_y;
    int nb_group_z;
    struct ngl_node *program;
    struct hmap *resources;

    struct pass pass;
};

#define PROGRAMS_TYPES_LIST (const int[]){NGL_NODE_COMPUTEPROGRAM,  \
                                          -1}

#define DATA_TYPES_LIST     (const int[]){NGL_NODE_TEXTURE2D,       \
                                          NGL_NODE_BLOCK,           \
                                          NGL_NODE_UNIFORMFLOAT,    \
                                          NGL_NODE_UNIFORMVEC2,     \
                                          NGL_NODE_UNIFORMVEC3,     \
                                          NGL_NODE_UNIFORMVEC4,     \
                                          NGL_NODE_UNIFORMQUAT,     \
                                          NGL_NODE_UNIFORMINT,      \
                                          NGL_NODE_UNIFORMIVEC2,    \
                                          NGL_NODE_UNIFORMIVEC3,    \
                                          NGL_NODE_UNIFORMIVEC4,    \
                                          NGL_NODE_UNIFORMUINT,     \
                                          NGL_NODE_UNIFORMUIVEC2,   \
                                          NGL_NODE_UNIFORMUIVEC3,   \
                                          NGL_NODE_UNIFORMUIVEC4,   \
                                          NGL_NODE_UNIFORMMAT4,     \
                                          NGL_NODE_ANIMATEDFLOAT,   \
                                          NGL_NODE_ANIMATEDVEC2,    \
                                          NGL_NODE_ANIMATEDVEC3,    \
                                          NGL_NODE_ANIMATEDVEC4,    \
                                          NGL_NODE_ANIMATEDQUAT,    \
                                          NGL_NODE_STREAMEDINT,     \
                                          NGL_NODE_STREAMEDIVEC2,   \
                                          NGL_NODE_STREAMEDIVEC3,   \
                                          NGL_NODE_STREAMEDIVEC4,   \
                                          NGL_NODE_STREAMEDUINT,    \
                                          NGL_NODE_STREAMEDUIVEC2,  \
                                          NGL_NODE_STREAMEDUIVEC3,  \
                                          NGL_NODE_STREAMEDUIVEC4,  \
                                          NGL_NODE_STREAMEDFLOAT,   \
                                          NGL_NODE_STREAMEDVEC2,    \
                                          NGL_NODE_STREAMEDVEC3,    \
                                          NGL_NODE_STREAMEDVEC4,    \
                                          NGL_NODE_STREAMEDMAT4,    \
                                          -1}

#define OFFSET(x) offsetof(struct compute_priv, x)
static const struct node_param compute_params[] = {
    {"nb_group_x", PARAM_TYPE_INT,      OFFSET(nb_group_x), .flags=PARAM_FLAG_CONSTRUCTOR,
                   .desc=NGLI_DOCSTRING("number of work groups to be executed in the x dimension")},
    {"nb_group_y", PARAM_TYPE_INT,      OFFSET(nb_group_y), .flags=PARAM_FLAG_CONSTRUCTOR,
                   .desc=NGLI_DOCSTRING("number of work groups to be executed in the y dimension")},
    {"nb_group_z", PARAM_TYPE_INT,      OFFSET(nb_group_z), .flags=PARAM_FLAG_CONSTRUCTOR,
                   .desc=NGLI_DOCSTRING("number of work groups to be executed in the z dimension")},
    {"program",    PARAM_TYPE_NODE,     OFFSET(program),    .flags=PARAM_FLAG_CONSTRUCTOR, .node_types=PROGRAMS_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("compute program to be executed")},
    {"resources",  PARAM_TYPE_NODEDICT, OFFSET(resources),
                   .node_types=DATA_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("resources made accessible to the compute `program`")},
    {NULL}
};

static int compute_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct compute_priv *s = node->priv_data;
    struct pass_params params = {
        .label = node->label,
        .program = s->program,
        .compute_resources = s->resources,
        .nb_group_x = s->nb_group_x,
        .nb_group_y = s->nb_group_y,
        .nb_group_z = s->nb_group_z,
    };
    return ngli_pass_init(&s->pass, ctx, &params);
}

static int compute_prepare(struct ngl_node *node)
{
    struct compute_priv *s = node->priv_data;
    return ngli_pass_prepare(&s->pass);
}

static void compute_uninit(struct ngl_node *node)
{
    struct compute_priv *s = node->priv_data;
    ngli_pass_uninit(&s->pass);
}

static int compute_update(struct ngl_node *node, double t)
{
    struct compute_priv *s = node->priv_data;
    return ngli_pass_update(&s->pass, t);
}

static void compute_draw(struct ngl_node *node)
{
    struct compute_priv *s = node->priv_data;
    ngli_pass_exec(&s->pass);
}

const struct node_class ngli_compute_class = {
    .id        = NGL_NODE_COMPUTE,
    .name      = "Compute",
    .init      = compute_init,
    .prepare   = compute_prepare,
    .uninit    = compute_uninit,
    .update    = compute_update,
    .draw      = compute_draw,
    .priv_size = sizeof(struct compute_priv),
    .params    = compute_params,
    .file      = __FILE__,
};
