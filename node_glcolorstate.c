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
static const struct node_param glcolorstate_params[] = {
    {"enabled", PARAM_TYPE_INT, OFFSET(enabled[0]), {.i64=GL_TRUE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"red",     PARAM_TYPE_INT, OFFSET(rgba[0][0]), {.i64=GL_TRUE}},
    {"green",   PARAM_TYPE_INT, OFFSET(rgba[0][1]), {.i64=GL_TRUE}},
    {"blue",    PARAM_TYPE_INT, OFFSET(rgba[0][2]), {.i64=GL_TRUE}},
    {"alpha",   PARAM_TYPE_INT, OFFSET(rgba[0][3]), {.i64=GL_TRUE}},
    {NULL}
};

static char *glcolorstate_info_str(const struct ngl_node *node)
{
    const struct glstate *s = node->priv_data;

    return ngli_asprintf("COLOR_WRITEMASK red=%d, green=%d, blue=%d, alpha=%d",
                         s->rgba[0][1], s->rgba[0][1], s->rgba[0][2], s->rgba[0][3]);
}

static int glcolorstate_init(struct ngl_node *node)
{
    return 0;
}

const struct node_class ngli_glcolorstate_class = {
    .id        = NGL_NODE_GLCOLORSTATE,
    .name      = "GLColorState",
    .info_str  = glcolorstate_info_str,
    .init      = glcolorstate_init,
    .priv_size = sizeof(struct glstate),
    .params    = glcolorstate_params,
};
