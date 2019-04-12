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

#ifndef GRAPHICCONFIG_H
#define GRAPHICCONFIG_H

#include "utils.h"

enum {
    NGLI_BLEND_FACTOR_ZERO,
    NGLI_BLEND_FACTOR_ONE,
    NGLI_BLEND_FACTOR_SRC_COLOR,
    NGLI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    NGLI_BLEND_FACTOR_DST_COLOR,
    NGLI_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    NGLI_BLEND_FACTOR_SRC_ALPHA,
    NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    NGLI_BLEND_FACTOR_DST_ALPHA,
    NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    NGLI_BLEND_FACTOR_NB
};

enum {
    NGLI_BLEND_OP_ADD,
    NGLI_BLEND_OP_SUBTRACT,
    NGLI_BLEND_OP_REVERSE_SUBTRACT,
    NGLI_BLEND_OP_MIN,
    NGLI_BLEND_OP_MAX,
    NGLI_BLEND_OP_NB
};

enum {
    NGLI_COMPARE_OP_NEVER,
    NGLI_COMPARE_OP_LESS,
    NGLI_COMPARE_OP_EQUAL,
    NGLI_COMPARE_OP_LESS_OR_EQUAL,
    NGLI_COMPARE_OP_GREATER,
    NGLI_COMPARE_OP_NOT_EQUAL,
    NGLI_COMPARE_OP_GREATER_OR_EQUAL,
    NGLI_COMPARE_OP_ALWAYS,
    NGLI_COMPARE_OP_NB
};

enum {
    NGLI_STENCIL_OP_KEEP,
    NGLI_STENCIL_OP_ZERO,
    NGLI_STENCIL_OP_REPLACE,
    NGLI_STENCIL_OP_INCREMENT_AND_CLAMP,
    NGLI_STENCIL_OP_DECREMENT_AND_CLAMP,
    NGLI_STENCIL_OP_INVERT,
    NGLI_STENCIL_OP_INCREMENT_AND_WRAP,
    NGLI_STENCIL_OP_DECREMENT_AND_WRAP,
    NGLI_STENCIL_OP_NB
};

enum {
    NGLI_CULL_MODE_NONE,
    NGLI_CULL_MODE_FRONT_BIT,
    NGLI_CULL_MODE_BACK_BIT,
    NGLI_CULL_MODE_FRONT_AND_BACK,
    NGLI_CULL_MODE_NB
};

NGLI_STATIC_ASSERT(cull_mode, (NGLI_CULL_MODE_FRONT_BIT | NGLI_CULL_MODE_BACK_BIT) == NGLI_CULL_MODE_FRONT_AND_BACK);

enum {
    NGLI_COLOR_COMPONENT_R_BIT = 1 << 0,
    NGLI_COLOR_COMPONENT_G_BIT = 1 << 1,
    NGLI_COLOR_COMPONENT_B_BIT = 1 << 2,
    NGLI_COLOR_COMPONENT_A_BIT = 1 << 3,
};

struct graphicconfig {
    int blend;
    int blend_dst_factor;
    int blend_src_factor;
    int blend_dst_factor_a;
    int blend_src_factor_a;
    int blend_op;
    int blend_op_a;

    int color_write_mask;

    int depth_test;
    int depth_write_mask;
    int depth_func;

    int stencil_test;
    int stencil_write_mask;
    int stencil_func;
    int stencil_ref;
    int stencil_read_mask;
    int stencil_fail;
    int stencil_depth_fail;
    int stencil_depth_pass;

    int cull_face;
    int cull_face_mode;

    int scissor_test;
    int scissor[4];
};

void ngli_graphicconfig_init(struct graphicconfig *s);

#endif
