/*
 * Copyright 2020 GoPro Inc.
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
#include "precision.h"

#define OFFSET(x) offsetof(struct resourceprops_priv, x)
static const struct node_param resourceprops_params[] = {
    {"precision", NGLI_PARAM_TYPE_SELECT, OFFSET(precision), {.i64=NGLI_PRECISION_AUTO},
                  .choices=&ngli_precision_choices,
                  .desc=NGLI_DOCSTRING("precision qualifier for the shader")},
    {"as_image", NGLI_PARAM_TYPE_BOOL, OFFSET(as_image),
                 .desc=NGLI_DOCSTRING("flag this resource for image accessing (only applies to texture nodes)")},
    {"writable", NGLI_PARAM_TYPE_BOOL, OFFSET(writable),
                 .desc=NGLI_DOCSTRING("flag this resource as writable in the shader")},
    {"variadic", NGLI_PARAM_TYPE_BOOL, OFFSET(variadic),
                 .desc=NGLI_DOCSTRING("flag this resource as variadic (only applies to block nodes)")},
    {NULL}
};

const struct node_class ngli_resourceprops_class = {
    .id        = NGL_NODE_RESOURCEPROPS,
    .name      = "ResourceProps",
    .priv_size = sizeof(struct resourceprops_priv),
    .params    = resourceprops_params,
    .file      = __FILE__,
};
