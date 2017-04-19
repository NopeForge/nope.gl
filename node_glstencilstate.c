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
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct glstate, x)
static const struct node_param glstencilstate_params[] = {
    {"enabled",    PARAM_TYPE_INT, OFFSET(enabled[0]),    {.i64=GL_FALSE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"writemask",  PARAM_TYPE_INT, OFFSET(writemask[0]),  {.i64=0xFF}},
    {"func",       PARAM_TYPE_INT, OFFSET(func[0]),       {.i64=GL_ALWAYS}},
    {"func_ref",   PARAM_TYPE_INT, OFFSET(func_ref[0]),   {.i64=0}},
    {"func_mask",  PARAM_TYPE_INT, OFFSET(func_mask[0]),  {.i64=0xFF}},
    {"op_sfail",   PARAM_TYPE_INT, OFFSET(op_sfail[0]),   {.i64=GL_KEEP}},
    {"op_dpfail",  PARAM_TYPE_INT, OFFSET(op_dpfail[0]),  {.i64=GL_KEEP}},
    {"op_dppass",  PARAM_TYPE_INT, OFFSET(op_dppass[0]),  {.i64=GL_KEEP}},
    {NULL}
};

static char *glstencilstate_info_str(const struct ngl_node *node)
{
    char *info_str;
    const struct glstate *s = node->priv_data;
    if (s->enabled[0]) {
        info_str = ngli_asprintf("STENCIL_TEST enabled");
    } else {
        info_str = ngli_asprintf("STENCIL_TEST disabled");
    }
    return info_str;
}

static int glstencilstate_init(struct ngl_node *node)
{
    struct glstate *s = node->priv_data;

    s->capability = GL_STENCIL_TEST;

    return 0;
}

const struct node_class ngli_glstencilstate_class = {
    .id        = NGL_NODE_GLSTENCILSTATE,
    .name      = "GLStencilState",
    .info_str  = glstencilstate_info_str,
    .init      = glstencilstate_init,
    .priv_size = sizeof(struct glstate),
    .params    = glstencilstate_params,
};
