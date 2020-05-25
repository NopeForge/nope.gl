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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "bstr.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "pgcache.h"
#include "program.h"

#define OFFSET(x) offsetof(struct program_priv, x)
static const struct node_param program_params[] = {
    {"vertex",   PARAM_TYPE_STR, OFFSET(vertex),   {.str=NULL},
                 .desc=NGLI_DOCSTRING("vertex shader")},
    {"fragment", PARAM_TYPE_STR, OFFSET(fragment), {.str=NULL},
                 .desc=NGLI_DOCSTRING("fragment shader")},
    {NULL}
};

static int program_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct program_priv *s = node->priv_data;

    if (!s->vertex || !s->fragment) {
        LOG(ERROR, "both vertex and fragment shaders must be set");
        return NGL_ERROR_INVALID_USAGE;
    }

    return ngli_pgcache_get_graphics_program(&ctx->pgcache, &s->program, s->vertex, s->fragment);
}

static void program_uninit(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;
    ngli_pgcache_release_program(&s->program);
}

const struct node_class ngli_program_class = {
    .id        = NGL_NODE_PROGRAM,
    .name      = "Program",
    .init      = program_init,
    .uninit    = program_uninit,
    .priv_size = sizeof(struct program_priv),
    .params    = program_params,
    .file      = __FILE__,
};
