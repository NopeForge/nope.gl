/*
 * Copyright 2019 GoPro Inc.
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

#include "graphicconfig.h"

static const struct graphicconfig default_graphicconfig = {
    .blend              = 0,
    .blend_src_factor   = NGLI_BLEND_FACTOR_ONE,
    .blend_dst_factor   = NGLI_BLEND_FACTOR_ZERO,
    .blend_src_factor_a = NGLI_BLEND_FACTOR_ONE,
    .blend_dst_factor_a = NGLI_BLEND_FACTOR_ZERO,
    .blend_op           = NGLI_BLEND_OP_ADD,
    .blend_op_a         = NGLI_BLEND_OP_ADD,
    .color_write_mask   = NGLI_COLOR_COMPONENT_R_BIT
                        | NGLI_COLOR_COMPONENT_G_BIT
                        | NGLI_COLOR_COMPONENT_B_BIT
                        | NGLI_COLOR_COMPONENT_A_BIT,
    .depth_test         = 0,
    .depth_write_mask   = 1,
    .depth_func         = NGLI_COMPARE_OP_LESS,
    .stencil_test       = 0,
    .stencil_write_mask = 1,
    .stencil_func       = NGLI_COMPARE_OP_ALWAYS,
    .stencil_ref        = 0,
    .stencil_read_mask  = 1,
    .stencil_fail       = NGLI_STENCIL_OP_KEEP,
    .stencil_depth_fail = NGLI_STENCIL_OP_KEEP,
    .stencil_depth_pass = NGLI_STENCIL_OP_KEEP,
    .cull_face          = 0,
    .cull_face_mode     = NGLI_CULL_MODE_BACK_BIT,
};

void ngli_graphicconfig_init(struct graphicconfig *s)
{
    *s = default_graphicconfig;
}
