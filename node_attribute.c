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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct attribute, x)
static const struct node_param attributevec2_params[] = {
    {"id",     PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static const struct node_param attributevec3_params[] = {
    {"id",     PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static const struct node_param attributevec4_params[] = {
    {"id",     PARAM_TYPE_STR,  OFFSET(name),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static char *attribute_info_str(const struct ngl_node *node)
{
    const struct attribute *s = node->priv_data;
    return ngli_strdup(s->name);
}

const struct node_class ngli_attributevec2_class = {
    .id        = NGL_NODE_ATTRIBUTEVEC2,
    .name      = "AttributeVec2",
    .info_str  = attribute_info_str,
    .priv_size = sizeof(struct attribute),
    .params    = attributevec2_params,
};

const struct node_class ngli_attributevec3_class = {
    .id        = NGL_NODE_ATTRIBUTEVEC3,
    .name      = "AttributeVec3",
    .info_str  = attribute_info_str,
    .priv_size = sizeof(struct attribute),
    .params    = attributevec3_params,
};

const struct node_class ngli_attributevec4_class = {
    .id        = NGL_NODE_ATTRIBUTEVEC4,
    .name      = "AttributeVec4",
    .info_str  = attribute_info_str,
    .priv_size = sizeof(struct attribute),
    .params    = attributevec4_params,
};
