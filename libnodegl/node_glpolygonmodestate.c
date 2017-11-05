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
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct glstate, x)
static const struct node_param glpolygonmodestate_params[] = {
    {"mode", PARAM_TYPE_INT, OFFSET(mode[0]), {.i64=GL_FILL}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static char *glpolygonmodestate_info_str(const struct ngl_node *node)
{
    const struct glstate *s = node->priv_data;
    return ngli_asprintf("face=0x%x mode=0x%x", GL_FRONT_AND_BACK, s->mode[0]);
}

const struct node_class ngli_glpolygonmodestate_class = {
    .id        = NGL_NODE_GLPOLYGONMODESTATE,
    .name      = "GLPolygonModeState",
    .info_str  = glpolygonmodestate_info_str,
    .priv_size = sizeof(struct glstate),
    .params    = glpolygonmodestate_params,
};
