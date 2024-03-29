/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef GLSTATE_H
#define GLSTATE_H

#include "glcontext.h"
#include "gpu_ctx.h"

struct graphics_state;

struct glstate {
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
    GLuint stencil_write_mask;
    GLenum stencil_func;
    GLint  stencil_ref;
    GLuint stencil_read_mask;
    GLenum stencil_fail;
    GLenum stencil_depth_fail;
    GLenum stencil_depth_pass;

    GLboolean cull_face;
    GLenum cull_face_mode;

    GLboolean scissor_test;

    /* Dynamic graphics state */
    struct scissor scissor;
    struct viewport viewport;

    /* Common state */
    GLuint program_id;
};

void ngli_glstate_reset(const struct glcontext *gl,
                        struct glstate *glstate);

void ngli_glstate_update(const struct glcontext *gl,
                         struct glstate *glstate,
                         const struct graphics_state *state);

void ngli_glstate_use_program(const struct glcontext *gl,
                              struct glstate *glstate,
                              GLuint program_id);

void ngli_glstate_update_scissor(const struct glcontext *gl,
                                 struct glstate *glstate,
                                 const struct scissor *scissor);

void ngli_glstate_update_viewport(const struct glcontext *gl,
                                  struct glstate *glstate,
                                  const struct viewport *viewport);

#endif
