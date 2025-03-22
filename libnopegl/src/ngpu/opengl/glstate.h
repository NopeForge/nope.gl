/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2017-2022 GoPro Inc.
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

#ifndef NGPU_GLSTATE_H
#define NGPU_GLSTATE_H

#include "glcontext.h"
#include "ngpu/ctx.h"

struct ngpu_graphics_state;

struct ngpu_glstate_stencil_op {
    GLuint write_mask;
    GLenum func;
    GLint  ref;
    GLuint read_mask;
    GLenum fail;
    GLenum depth_fail;
    GLenum depth_pass;
};

struct ngpu_glstate {
    /* Graphics state */
    GLenum blend;
    GLenum blend_dst_factor;
    GLenum blend_src_factor;
    GLenum blend_dst_factor_a;
    GLenum blend_src_factor_a;
    GLenum blend_op;
    GLenum blend_op_a;

    GLboolean color_write_mask[4];

    GLenum    depth_test;
    GLboolean depth_write_mask;
    GLenum    depth_func;

    GLenum stencil_test;
    struct ngpu_glstate_stencil_op stencil_front;
    struct ngpu_glstate_stencil_op stencil_back;

    GLboolean cull_face;
    GLenum cull_face_mode;

    GLenum front_face;

    GLboolean scissor_test;

    /* Dynamic graphics state */
    struct ngpu_scissor scissor;
    struct ngpu_viewport viewport;

    /* Common state */
    GLuint program_id;
};

void ngpu_glstate_reset(const struct glcontext *gl,
                        struct ngpu_glstate *glstate);

void ngpu_glstate_update(const struct glcontext *gl,
                         struct ngpu_glstate *glstate,
                         const struct ngpu_graphics_state *state);

void ngpu_glstate_use_program(const struct glcontext *gl,
                              struct ngpu_glstate *glstate,
                              GLuint program_id);

void ngpu_glstate_update_scissor(const struct glcontext *gl,
                                 struct ngpu_glstate *glstate,
                                 const struct ngpu_scissor *scissor);

void ngpu_glstate_update_viewport(const struct glcontext *gl,
                                  struct ngpu_glstate *glstate,
                                  const struct ngpu_viewport *viewport);

void ngpu_glstate_enable_scissor_test(const struct glcontext *gl,
                                      struct ngpu_glstate *glstate,
                                      int enable);

#endif
