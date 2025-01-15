/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2019-2022 GoPro Inc.
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

#ifndef GPU_GRAPHICS_STATE_H
#define GPU_GRAPHICS_STATE_H

#include "utils.h"

enum {
    NGLI_GPU_BLEND_FACTOR_ZERO,
    NGLI_GPU_BLEND_FACTOR_ONE,
    NGLI_GPU_BLEND_FACTOR_SRC_COLOR,
    NGLI_GPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    NGLI_GPU_BLEND_FACTOR_DST_COLOR,
    NGLI_GPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    NGLI_GPU_BLEND_FACTOR_SRC_ALPHA,
    NGLI_GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    NGLI_GPU_BLEND_FACTOR_DST_ALPHA,
    NGLI_GPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    NGLI_GPU_BLEND_FACTOR_NB
};

enum {
    NGLI_GPU_BLEND_OP_ADD,
    NGLI_GPU_BLEND_OP_SUBTRACT,
    NGLI_GPU_BLEND_OP_REVERSE_SUBTRACT,
    NGLI_GPU_BLEND_OP_MIN,
    NGLI_GPU_BLEND_OP_MAX,
    NGLI_GPU_BLEND_OP_NB
};

enum {
    NGLI_GPU_COMPARE_OP_NEVER,
    NGLI_GPU_COMPARE_OP_LESS,
    NGLI_GPU_COMPARE_OP_EQUAL,
    NGLI_GPU_COMPARE_OP_LESS_OR_EQUAL,
    NGLI_GPU_COMPARE_OP_GREATER,
    NGLI_GPU_COMPARE_OP_NOT_EQUAL,
    NGLI_GPU_COMPARE_OP_GREATER_OR_EQUAL,
    NGLI_GPU_COMPARE_OP_ALWAYS,
    NGLI_GPU_COMPARE_OP_NB
};

enum {
    NGLI_GPU_STENCIL_OP_KEEP,
    NGLI_GPU_STENCIL_OP_ZERO,
    NGLI_GPU_STENCIL_OP_REPLACE,
    NGLI_GPU_STENCIL_OP_INCREMENT_AND_CLAMP,
    NGLI_GPU_STENCIL_OP_DECREMENT_AND_CLAMP,
    NGLI_GPU_STENCIL_OP_INVERT,
    NGLI_GPU_STENCIL_OP_INCREMENT_AND_WRAP,
    NGLI_GPU_STENCIL_OP_DECREMENT_AND_WRAP,
    NGLI_GPU_STENCIL_OP_NB
};

enum {
    NGLI_GPU_CULL_MODE_NONE,
    NGLI_GPU_CULL_MODE_FRONT_BIT,
    NGLI_GPU_CULL_MODE_BACK_BIT,
    NGLI_GPU_CULL_MODE_NB
};

enum {
    NGLI_GPU_COLOR_COMPONENT_R_BIT = 1 << 0,
    NGLI_GPU_COLOR_COMPONENT_G_BIT = 1 << 1,
    NGLI_GPU_COLOR_COMPONENT_B_BIT = 1 << 2,
    NGLI_GPU_COLOR_COMPONENT_A_BIT = 1 << 3,
};

struct gpu_stencil_op_state {
    int write_mask;
    int func;
    int ref;
    int read_mask;
    int fail;
    int depth_fail;
    int depth_pass;
};

struct gpu_graphics_state {
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
    struct gpu_stencil_op_state stencil_front;
    struct gpu_stencil_op_state stencil_back;

    int cull_mode;
};

/* Make sure to keep this in sync with the blending documentation */
#define NGLI_GPU_GRAPHICS_STATE_DEFAULTS (struct gpu_graphics_state) { \
    .blend              = 0,                                           \
    .blend_src_factor   = NGLI_GPU_BLEND_FACTOR_ONE,                   \
    .blend_dst_factor   = NGLI_GPU_BLEND_FACTOR_ZERO,                  \
    .blend_src_factor_a = NGLI_GPU_BLEND_FACTOR_ONE,                   \
    .blend_dst_factor_a = NGLI_GPU_BLEND_FACTOR_ZERO,                  \
    .blend_op           = NGLI_GPU_BLEND_OP_ADD,                       \
    .blend_op_a         = NGLI_GPU_BLEND_OP_ADD,                       \
    .color_write_mask   = NGLI_GPU_COLOR_COMPONENT_R_BIT               \
                        | NGLI_GPU_COLOR_COMPONENT_G_BIT               \
                        | NGLI_GPU_COLOR_COMPONENT_B_BIT               \
                        | NGLI_GPU_COLOR_COMPONENT_A_BIT,              \
    .depth_test         = 0,                                           \
    .depth_write_mask   = 1,                                           \
    .depth_func         = NGLI_GPU_COMPARE_OP_LESS,                    \
    .stencil_test       = 0,                                           \
    .stencil_front      = {                                            \
        .write_mask = 0xff,                                            \
        .func       = NGLI_GPU_COMPARE_OP_ALWAYS,                      \
        .ref        = 0,                                               \
        .read_mask  = 0xff,                                            \
        .fail       = NGLI_GPU_STENCIL_OP_KEEP,                        \
        .depth_fail = NGLI_GPU_STENCIL_OP_KEEP,                        \
        .depth_pass = NGLI_GPU_STENCIL_OP_KEEP,                        \
    },                                                                 \
    .stencil_back = {                                                  \
         .write_mask = 0xff,                                           \
         .func       = NGLI_GPU_COMPARE_OP_ALWAYS,                     \
         .ref        = 0,                                              \
         .read_mask  = 0xff,                                           \
         .fail       = NGLI_GPU_STENCIL_OP_KEEP,                       \
         .depth_fail = NGLI_GPU_STENCIL_OP_KEEP,                       \
         .depth_pass = NGLI_GPU_STENCIL_OP_KEEP,                       \
     },                                                                \
    .cull_mode          = NGLI_GPU_CULL_MODE_NONE,                     \
}                                                                      \

#endif
