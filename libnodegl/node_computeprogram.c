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

#include "gpu_ctx.h"
#include "gpu_limits.h"
#include "log.h"
#include "nodegl.h"
#include "internal.h"

#define OFFSET(x) offsetof(struct program_priv, x)
static const struct node_param computeprogram_params[] = {
    {"compute", NGLI_PARAM_TYPE_STR, OFFSET(compute), .flags=NGLI_PARAM_FLAG_NON_NULL,
                .desc=NGLI_DOCSTRING("compute shader")},
    {"workgroup_size", NGLI_PARAM_TYPE_IVEC3, OFFSET(workgroup_size),
                       .desc=NGLI_DOCSTRING("number of local compute instances in a work group")},
    {"properties", NGLI_PARAM_TYPE_NODEDICT, OFFSET(properties),
                   .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                   .node_types=(const int[]){NGL_NODE_RESOURCEPROPS, -1},
                   .desc=NGLI_DOCSTRING("resource properties")},
    {NULL}
};

static int computeprogram_init(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;
    struct ngl_ctx *ctx = node->ctx;

    if (s->workgroup_size[0] <= 0 || s->workgroup_size[1] <= 0 || s->workgroup_size[2] <= 0) {
        LOG(ERROR, "work group size must be > 0 for x, y and z");
        return NGL_ERROR_INVALID_ARG;
    }

    const struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct gpu_limits *limits = &gpu_ctx->limits;

    if (s->workgroup_size[0] > limits->max_compute_work_group_size[0] ||
        s->workgroup_size[1] > limits->max_compute_work_group_size[1] ||
        s->workgroup_size[2] > limits->max_compute_work_group_size[2]) {
        LOG(ERROR,
            "compute work group size (%d, %d, %d) exceeds device limits (%d, %d, %d)",
            NGLI_ARG_VEC3(s->workgroup_size),
            NGLI_ARG_VEC3(limits->max_compute_work_group_size));
        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }

    const uint32_t nb_invocations = s->workgroup_size[0] * s->workgroup_size[1] * s->workgroup_size[2];
    const uint32_t max_invocations = limits->max_compute_work_group_invocations;
    if (nb_invocations > max_invocations) {
        LOG(ERROR, "compute number of invocations (%u) exceeds device limits (%u)",
            nb_invocations, max_invocations);
        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }

    return 0;
}

const struct node_class ngli_computeprogram_class = {
    .id        = NGL_NODE_COMPUTEPROGRAM,
    .name      = "ComputeProgram",
    .priv_size = sizeof(struct program_priv),
    .params    = computeprogram_params,
    .init      = computeprogram_init,
    .file      = __FILE__,
};
