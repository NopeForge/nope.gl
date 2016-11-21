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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct glstate, x)
static const struct node_param glstate_params[] = {
    {"capability", PARAM_TYPE_INT, OFFSET(capability), {.i64=GL_NONE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"enabled",    PARAM_TYPE_INT, OFFSET(enabled[0]), {.i64=GL_FALSE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static char *glstate_info_str(const struct ngl_node *node)
{
    const struct glstate *s = node->priv_data;
    return ngli_asprintf("0x%x enabled=%s",
                         s->capability, s->enabled[0] ? "yes" : "no");
}

const struct node_class ngli_glstate_class = {
    .id        = NGL_NODE_GLSTATE,
    .name      = "GLState",
    .info_str  = glstate_info_str,
    .priv_size = sizeof(struct glstate),
    .params    = glstate_params,
};
