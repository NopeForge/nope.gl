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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct shapeprimitive, x)
static const struct node_param shapeprimitive_params[] = {
    {"coordinates",         PARAM_TYPE_VEC3, OFFSET(coordinates), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"texture_coordinates", PARAM_TYPE_VEC2, OFFSET(texture_coordinates), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"normals",             PARAM_TYPE_VEC3, OFFSET(normals)},
    {NULL}
};

const struct node_class ngli_shapeprimitive_class = {
    .id        = NGL_NODE_SHAPEPRIMITIVE,
    .name      = "ShapePrimitive",
    .priv_size = sizeof(struct shapeprimitive),
    .params    = shapeprimitive_params,
};
