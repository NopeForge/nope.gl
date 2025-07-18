/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "ngpu/limits.h"
#include "node_program.h"
#include "nopegl.h"
#include "pass.h"
#include "utils/hmap.h"
#include "utils/utils.h"

struct compute_opts {
    uint32_t workgroup_count[3];
    struct ngl_node *program;
    struct hmap *resources;
};

struct compute_priv {
    struct pass pass;
};

#define PROGRAMS_TYPES_LIST (const uint32_t[]){NGL_NODE_COMPUTEPROGRAM,  \
                                               NGLI_NODE_NONE}

#define DATA_TYPES_LIST     (const uint32_t[]){NGL_NODE_TEXTURE2D,       \
                                               NGL_NODE_TEXTURE2DARRAY,  \
                                               NGL_NODE_TEXTURE3D,       \
                                               NGL_NODE_TEXTURECUBE,     \
                                               NGL_NODE_BLOCK,           \
                                               NGL_NODE_COLORSTATS,      \
                                               NGL_NODE_UNIFORMFLOAT,    \
                                               NGL_NODE_UNIFORMVEC2,     \
                                               NGL_NODE_UNIFORMVEC3,     \
                                               NGL_NODE_UNIFORMVEC4,     \
                                               NGL_NODE_UNIFORMCOLOR,    \
                                               NGL_NODE_UNIFORMQUAT,     \
                                               NGL_NODE_UNIFORMBOOL,     \
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
                                               NGL_NODE_ANIMATEDCOLOR,   \
                                               NGL_NODE_NOISEFLOAT,      \
                                               NGL_NODE_NOISEVEC2,       \
                                               NGL_NODE_NOISEVEC3,       \
                                               NGL_NODE_NOISEVEC4,       \
                                               NGL_NODE_EVALFLOAT,       \
                                               NGL_NODE_EVALVEC2,        \
                                               NGL_NODE_EVALVEC3,        \
                                               NGL_NODE_EVALVEC4,        \
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
                                               NGL_NODE_TIME,            \
                                               NGL_NODE_VELOCITYFLOAT,   \
                                               NGL_NODE_VELOCITYVEC2,    \
                                               NGL_NODE_VELOCITYVEC3,    \
                                               NGL_NODE_VELOCITYVEC4,    \
                                               NGLI_NODE_NONE}

#define OFFSET(x) offsetof(struct compute_opts, x)
static const struct node_param compute_params[] = {
    {"workgroup_count", NGLI_PARAM_TYPE_UVEC3,      OFFSET(workgroup_count),
                        .desc=NGLI_DOCSTRING("number of work groups to be executed")},
    {"program",    NGLI_PARAM_TYPE_NODE,     OFFSET(program),    .flags=NGLI_PARAM_FLAG_NON_NULL, .node_types=PROGRAMS_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("compute program to be executed")},
    {"resources",  NGLI_PARAM_TYPE_NODEDICT, OFFSET(resources),
                   .node_types=DATA_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("resources made accessible to the compute `program`")},
    {NULL}
};

static int check_params(const struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    const struct compute_opts *o = node->opts;

    if (o->workgroup_count[0] <= 0 || o->workgroup_count[1] <= 0 || o->workgroup_count[2] <= 0) {
        LOG(ERROR, "number of group must be > 0 for x, y and z");
        return NGL_ERROR_INVALID_ARG;
    }
    const struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct ngpu_limits *limits = &gpu_ctx->limits;

    if (o->workgroup_count[0] > limits->max_compute_work_group_count[0] ||
        o->workgroup_count[1] > limits->max_compute_work_group_count[1] ||
        o->workgroup_count[2] > limits->max_compute_work_group_count[2]) {
        LOG(ERROR,
            "compute work group counts (%u, %u, %u) exceed device limits (%u, %u, %u)",
            NGLI_ARG_VEC3(o->workgroup_count),
            NGLI_ARG_VEC3(limits->max_compute_work_group_count));

        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }

    return 0;
}

static int compute_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct compute_priv *s = node->priv_data;
    const struct compute_opts *o = node->opts;

    int ret = check_params(node);
    if (ret < 0)
        return ret;

    const struct program_opts *program = o->program->opts;
    struct pass_params params = {
        .label = node->label,
        .program_label = o->program->label,
        .comp_base = program->compute,
        .compute_resources = o->resources,
        .properties = program->properties,
        .workgroup_count = {NGLI_ARG_VEC3(o->workgroup_count)},
        .workgroup_size = {NGLI_ARG_VEC3(program->workgroup_size)},
    };
    return ngli_pass_init(&s->pass, ctx, &params);
}

static int compute_prepare(struct ngl_node *node)
{
    struct compute_priv *s = node->priv_data;

    int ret = ngli_node_prepare_children(node);
    if (ret < 0)
        return ret;

    return ngli_pass_prepare(&s->pass);
}

static void compute_uninit(struct ngl_node *node)
{
    struct compute_priv *s = node->priv_data;
    ngli_pass_uninit(&s->pass);
}

static void compute_draw(struct ngl_node *node)
{
    struct compute_priv *s = node->priv_data;
    ngli_node_draw_children(node);
    ngli_pass_exec(&s->pass);
}

const struct node_class ngli_compute_class = {
    .id        = NGL_NODE_COMPUTE,
    .name      = "Compute",
    .init      = compute_init,
    .prepare   = compute_prepare,
    .uninit    = compute_uninit,
    .update    = ngli_node_update_children,
    .draw      = compute_draw,
    .opts_size = sizeof(struct compute_opts),
    .priv_size = sizeof(struct compute_priv),
    .params    = compute_params,
    .file      = __FILE__,
};
