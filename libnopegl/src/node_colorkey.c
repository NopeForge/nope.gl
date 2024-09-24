/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Forge
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

#include "internal.h"
#include "node_colorkey.h"
#include "nopegl.h"

#define OFFSET(x) offsetof(struct colorkey_opts, x)
static const struct node_param colorkey_params[] = {
    {"position", NGLI_PARAM_TYPE_F32, OFFSET(position_node), {.f32=0.f},
                .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                .desc=NGLI_DOCSTRING("position of the gradient point on the axis (within [0,1])")},
    {"color",   NGLI_PARAM_TYPE_VEC3, OFFSET(color_node), {.vec={1.f, 1.f, 1.f}},
                .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                .desc=NGLI_DOCSTRING("color at this specific position")},
    {"opacity", NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=1.f},
                .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                .desc=NGLI_DOCSTRING("opacity at this specific position")},
    {NULL}
};

const struct node_class ngli_colorkey_class = {
    .id        = NGL_NODE_COLORKEY,
    .name      = "ColorKey",
    .update    = ngli_node_update_children,
    .opts_size = sizeof(struct colorkey_opts),
    .params    = colorkey_params,
    .file      = __FILE__,
};
