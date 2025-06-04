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

#ifndef NGPU_GRAPHICS_STATE_H
#define NGPU_GRAPHICS_STATE_H

#include "utils/utils.h"

enum ngpu_blend_factor {
    NGPU_BLEND_FACTOR_ZERO,
    NGPU_BLEND_FACTOR_ONE,
    NGPU_BLEND_FACTOR_SRC_COLOR,
    NGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    NGPU_BLEND_FACTOR_DST_COLOR,
    NGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    NGPU_BLEND_FACTOR_SRC_ALPHA,
    NGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    NGPU_BLEND_FACTOR_DST_ALPHA,
    NGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    NGPU_BLEND_FACTOR_CONSTANT_COLOR,
    NGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    NGPU_BLEND_FACTOR_CONSTANT_ALPHA,
    NGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    NGPU_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    NGPU_BLEND_FACTOR_NB
};

enum ngpu_blend_op {
    NGPU_BLEND_OP_ADD,
    NGPU_BLEND_OP_SUBTRACT,
    NGPU_BLEND_OP_REVERSE_SUBTRACT,
    NGPU_BLEND_OP_MIN,
    NGPU_BLEND_OP_MAX,
    NGPU_BLEND_OP_NB
};

enum ngpu_compare_op {
    NGPU_COMPARE_OP_NEVER,
    NGPU_COMPARE_OP_LESS,
    NGPU_COMPARE_OP_EQUAL,
    NGPU_COMPARE_OP_LESS_OR_EQUAL,
    NGPU_COMPARE_OP_GREATER,
    NGPU_COMPARE_OP_NOT_EQUAL,
    NGPU_COMPARE_OP_GREATER_OR_EQUAL,
    NGPU_COMPARE_OP_ALWAYS,
    NGPU_COMPARE_OP_NB
};

enum ngpu_stencil_op {
    NGPU_STENCIL_OP_KEEP,
    NGPU_STENCIL_OP_ZERO,
    NGPU_STENCIL_OP_REPLACE,
    NGPU_STENCIL_OP_INCREMENT_AND_CLAMP,
    NGPU_STENCIL_OP_DECREMENT_AND_CLAMP,
    NGPU_STENCIL_OP_INVERT,
    NGPU_STENCIL_OP_INCREMENT_AND_WRAP,
    NGPU_STENCIL_OP_DECREMENT_AND_WRAP,
    NGPU_STENCIL_OP_NB
};

enum ngpu_cull_mode {
    NGPU_CULL_MODE_NONE,
    NGPU_CULL_MODE_FRONT_BIT,
    NGPU_CULL_MODE_BACK_BIT,
    NGPU_CULL_MODE_NB
};

enum {
    NGPU_COLOR_COMPONENT_R_BIT = 1 << 0,
    NGPU_COLOR_COMPONENT_G_BIT = 1 << 1,
    NGPU_COLOR_COMPONENT_B_BIT = 1 << 2,
    NGPU_COLOR_COMPONENT_A_BIT = 1 << 3,
};

enum ngpu_front_face {
    NGPU_FRONT_FACE_COUNTER_CLOCKWISE,
    NGPU_FRONT_FACE_CLOCKWISE,
    NGPU_FRONT_FACE_NB
};

struct ngpu_stencil_op_state {
    int write_mask;
    enum ngpu_compare_op func;
    int ref;
    int read_mask;
    enum ngpu_stencil_op fail;
    enum ngpu_stencil_op depth_fail;
    enum ngpu_stencil_op depth_pass;
};

struct ngpu_graphics_state {
    int blend;
    enum ngpu_blend_factor blend_dst_factor;
    enum ngpu_blend_factor blend_src_factor;
    enum ngpu_blend_factor blend_dst_factor_a;
    enum ngpu_blend_factor blend_src_factor_a;
    enum ngpu_blend_op blend_op;
    enum ngpu_blend_op blend_op_a;

    int color_write_mask;

    int depth_test;
    int depth_write_mask;
    enum ngpu_compare_op depth_func;

    int stencil_test;
    struct ngpu_stencil_op_state stencil_front;
    struct ngpu_stencil_op_state stencil_back;

    enum ngpu_cull_mode cull_mode;
    enum ngpu_front_face front_face;
};

/* Make sure to keep this in sync with the blending documentation */
#define NGPU_GRAPHICS_STATE_DEFAULTS (struct ngpu_graphics_state) {    \
    .blend              = 0,                                           \
    .blend_src_factor   = NGPU_BLEND_FACTOR_ONE,                       \
    .blend_dst_factor   = NGPU_BLEND_FACTOR_ZERO,                      \
    .blend_src_factor_a = NGPU_BLEND_FACTOR_ONE,                       \
    .blend_dst_factor_a = NGPU_BLEND_FACTOR_ZERO,                      \
    .blend_op           = NGPU_BLEND_OP_ADD,                           \
    .blend_op_a         = NGPU_BLEND_OP_ADD,                           \
    .color_write_mask   = NGPU_COLOR_COMPONENT_R_BIT                   \
                        | NGPU_COLOR_COMPONENT_G_BIT                   \
                        | NGPU_COLOR_COMPONENT_B_BIT                   \
                        | NGPU_COLOR_COMPONENT_A_BIT,                  \
    .depth_test         = 0,                                           \
    .depth_write_mask   = 1,                                           \
    .depth_func         = NGPU_COMPARE_OP_LESS,                        \
    .stencil_test       = 0,                                           \
    .stencil_front      = {                                            \
        .write_mask = 0xff,                                            \
        .func       = NGPU_COMPARE_OP_ALWAYS,                          \
        .ref        = 0,                                               \
        .read_mask  = 0xff,                                            \
        .fail       = NGPU_STENCIL_OP_KEEP,                            \
        .depth_fail = NGPU_STENCIL_OP_KEEP,                            \
        .depth_pass = NGPU_STENCIL_OP_KEEP,                            \
    },                                                                 \
    .stencil_back = {                                                  \
         .write_mask = 0xff,                                           \
         .func       = NGPU_COMPARE_OP_ALWAYS,                         \
         .ref        = 0,                                              \
         .read_mask  = 0xff,                                           \
         .fail       = NGPU_STENCIL_OP_KEEP,                           \
         .depth_fail = NGPU_STENCIL_OP_KEEP,                           \
         .depth_pass = NGPU_STENCIL_OP_KEEP,                           \
     },                                                                \
    .cull_mode          = NGPU_CULL_MODE_NONE,                         \
    .front_face         = NGPU_FRONT_FACE_COUNTER_CLOCKWISE            \
}                                                                      \

#endif
