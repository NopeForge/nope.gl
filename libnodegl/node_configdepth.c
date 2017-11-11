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
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct configdepth, x)
static const struct node_param configdepth_params[] = {
    {"enabled",   PARAM_TYPE_INT, OFFSET(enabled[0]),   {.i64=GL_FALSE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"writemask", PARAM_TYPE_INT, OFFSET(writemask[0]), {.i64=GL_TRUE}},
    {"func",      PARAM_TYPE_INT, OFFSET(func[0]),      {.i64=GL_LESS}},
    {NULL}
};

static char *configdepth_info_str(const struct ngl_node *node)
{
    const struct configdepth *s = node->priv_data;
    return ngli_asprintf("DEPTH_TEST enabled=%s writemask=0x%x func=0x%x",
                         s->enabled[0] ? "yes" : "no",
                         s->writemask[0],
                         s->func[0]);
}

static int configdepth_init(struct ngl_node *node)
{
    struct configdepth *s = node->priv_data;

    s->capability = GL_DEPTH_TEST;

    return 0;
}

const struct node_class ngli_configdepth_class = {
    .id        = NGL_NODE_CONFIGDEPTH,
    .name      = "ConfigDepth",
    .init      = configdepth_init,
    .info_str  = configdepth_info_str,
    .priv_size = sizeof(struct configdepth),
    .params    = configdepth_params,
};
