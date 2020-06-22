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
#include "nodegl.h"
#include "nodes.h"
#include "pgcache.h"

#define OFFSET(x) offsetof(struct program_priv, x)
static const struct node_param computeprogram_params[] = {
    {"compute", PARAM_TYPE_STR, OFFSET(compute), .flags=PARAM_FLAG_CONSTRUCTOR,
                .desc=NGLI_DOCSTRING("compute shader")},
    {NULL}
};

static int computeprogram_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct program_priv *s = node->priv_data;

    return ngli_pgcache_get_compute_program(&ctx->pgcache, &s->program, s->compute);
}

static void computeprogram_uninit(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;
    ngli_pgcache_release_program(&s->program);
}

const struct node_class ngli_computeprogram_class = {
    .id        = NGL_NODE_COMPUTEPROGRAM,
    .name      = "ComputeProgram",
    .init      = computeprogram_init,
    .uninit    = computeprogram_uninit,
    .priv_size = sizeof(struct program_priv),
    .params    = computeprogram_params,
    .file      = __FILE__,
};
