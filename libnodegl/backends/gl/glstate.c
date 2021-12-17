/*
 * Copyright 2017 GoPro Inc.
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

#include <stdlib.h>
#include <string.h>
#include "gpu_ctx_gl.h"
#include "glcontext.h"
#include "glincludes.h"
#include "glstate.h"
#include "graphicstate.h"
#include "internal.h"

static const GLenum gl_blend_factor_map[NGLI_BLEND_FACTOR_NB] = {
    [NGLI_BLEND_FACTOR_ZERO]                = GL_ZERO,
    [NGLI_BLEND_FACTOR_ONE]                 = GL_ONE,
    [NGLI_BLEND_FACTOR_SRC_COLOR]           = GL_SRC_COLOR,
    [NGLI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR] = GL_ONE_MINUS_SRC_COLOR,
    [NGLI_BLEND_FACTOR_DST_COLOR]           = GL_DST_COLOR,
    [NGLI_BLEND_FACTOR_ONE_MINUS_DST_COLOR] = GL_ONE_MINUS_DST_COLOR,
    [NGLI_BLEND_FACTOR_SRC_ALPHA]           = GL_SRC_ALPHA,
    [NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA] = GL_ONE_MINUS_SRC_ALPHA,
    [NGLI_BLEND_FACTOR_DST_ALPHA]           = GL_DST_ALPHA,
    [NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA] = GL_ONE_MINUS_DST_ALPHA,
};

static GLenum get_gl_blend_factor(int blend_factor)
{
    return gl_blend_factor_map[blend_factor];
}

static const GLenum gl_blend_op_map[NGLI_BLEND_OP_NB] = {
    [NGLI_BLEND_OP_ADD]              = GL_FUNC_ADD,
    [NGLI_BLEND_OP_SUBTRACT]         = GL_FUNC_SUBTRACT,
    [NGLI_BLEND_OP_REVERSE_SUBTRACT] = GL_FUNC_REVERSE_SUBTRACT,
    [NGLI_BLEND_OP_MIN]              = GL_MIN,
    [NGLI_BLEND_OP_MAX]              = GL_MAX,
};

static GLenum get_gl_blend_op(int blend_op)
{
    return gl_blend_op_map[blend_op];
}

static const GLenum gl_compare_op_map[NGLI_COMPARE_OP_NB] = {
    [NGLI_COMPARE_OP_NEVER]            = GL_NEVER,
    [NGLI_COMPARE_OP_LESS]             = GL_LESS,
    [NGLI_COMPARE_OP_EQUAL]            = GL_EQUAL,
    [NGLI_COMPARE_OP_LESS_OR_EQUAL]    = GL_LEQUAL,
    [NGLI_COMPARE_OP_GREATER]          = GL_GREATER,
    [NGLI_COMPARE_OP_NOT_EQUAL]        = GL_NOTEQUAL,
    [NGLI_COMPARE_OP_GREATER_OR_EQUAL] = GL_GEQUAL,
    [NGLI_COMPARE_OP_ALWAYS]           = GL_ALWAYS,
};

static GLenum get_gl_compare_op(int compare_op)
{
    return gl_compare_op_map[compare_op];
}

static const GLenum gl_stencil_op_map[NGLI_STENCIL_OP_NB] = {
    [NGLI_STENCIL_OP_KEEP]                = GL_KEEP,
    [NGLI_STENCIL_OP_ZERO]                = GL_ZERO,
    [NGLI_STENCIL_OP_REPLACE]             = GL_REPLACE,
    [NGLI_STENCIL_OP_INCREMENT_AND_CLAMP] = GL_INCR,
    [NGLI_STENCIL_OP_DECREMENT_AND_CLAMP] = GL_DECR,
    [NGLI_STENCIL_OP_INVERT]              = GL_INVERT,
    [NGLI_STENCIL_OP_INCREMENT_AND_WRAP]  = GL_INCR_WRAP,
    [NGLI_STENCIL_OP_DECREMENT_AND_WRAP]  = GL_DECR_WRAP,
};

static GLenum get_gl_stencil_op(int stencil_op)
{
    return gl_stencil_op_map[stencil_op];
}

static const GLenum gl_cull_mode_map[NGLI_CULL_MODE_NB] = {
    [NGLI_CULL_MODE_NONE]           = GL_BACK,
    [NGLI_CULL_MODE_FRONT_BIT]      = GL_FRONT,
    [NGLI_CULL_MODE_BACK_BIT]       = GL_BACK,
};

static GLenum get_gl_cull_mode(int cull_mode)
{
    return gl_cull_mode_map[cull_mode];
}

void ngli_glstate_reset(const struct glcontext *gl, struct glstate *glstate)
{
    memset(glstate, 0, sizeof(*glstate));

    /* Blending */
    ngli_glDisable(gl, GL_BLEND);
    glstate->blend = 0;

    ngli_glBlendFuncSeparate(gl, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glstate->blend_src_factor = GL_ONE;
    glstate->blend_dst_factor = GL_ZERO;
    glstate->blend_src_factor_a = GL_ONE;
    glstate->blend_dst_factor_a = GL_ZERO;

    ngli_glBlendEquationSeparate(gl, GL_FUNC_ADD, GL_FUNC_ADD);
    glstate->blend_op = GL_FUNC_ADD;
    glstate->blend_op_a = GL_FUNC_ADD;

    /* Color write mask */
    ngli_glColorMask(gl, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glstate->color_write_mask[0] = GL_TRUE;
    glstate->color_write_mask[1] = GL_TRUE;
    glstate->color_write_mask[2] = GL_TRUE;
    glstate->color_write_mask[3] = GL_TRUE;

    /* Depth */
    ngli_glDisable(gl, GL_DEPTH_TEST);
    glstate->depth_test = 0;

    ngli_glDepthMask(gl, GL_TRUE);
    glstate->depth_write_mask = GL_TRUE;

    ngli_glDepthFunc(gl, GL_LESS);
    glstate->depth_func = GL_LESS;

    /* Stencil */
    ngli_glDisable(gl, GL_STENCIL_TEST);
    glstate->stencil_test = 0;

    /* Use node.gl's stencil read mask default (0xff) instead of OpenGL's ((GLuint)-1) */
    ngli_glStencilMask(gl, 0xff);
    glstate->stencil_write_mask = 0xff;

    /* Use node.gl's stencil write mask default (0xff) instead of OpenGL's ((GLuint)-1) */
    ngli_glStencilFunc(gl, GL_ALWAYS, 0, 0xff);
    glstate->stencil_func = GL_ALWAYS;
    glstate->stencil_ref = 0;
    glstate->stencil_read_mask = 0xff;

    ngli_glStencilOp(gl, GL_KEEP, GL_KEEP, GL_KEEP);
    glstate->stencil_fail = GL_KEEP;
    glstate->stencil_depth_fail = GL_KEEP;
    glstate->stencil_depth_pass = GL_KEEP;

    /* Face culling */
    ngli_glDisable(gl, GL_CULL_FACE);
    glstate->cull_face = 0;

    ngli_glCullFace(gl, GL_BACK);
    glstate->cull_face_mode = GL_BACK;

    /* Scissor */
    ngli_glDisable(gl, GL_SCISSOR_TEST);
    glstate->scissor_test = 0;

    /* Program */
    ngli_glUseProgram(gl, 0);
    glstate->program_id = 0;

    /* VAO */
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glBindVertexArray(gl, 0);
}

void ngli_glstate_update(const struct glcontext *gl, struct glstate *glstate, const struct graphicstate *state)
{
    /* Blend */
    const int blend = state->blend;
    if (blend != glstate->blend) {
        if (blend)
            ngli_glEnable(gl, GL_BLEND);
        else
            ngli_glDisable(gl, GL_BLEND);
        glstate->blend = blend;
    }

    const GLenum blend_dst_factor   = get_gl_blend_factor(state->blend_dst_factor);
    const GLenum blend_src_factor   = get_gl_blend_factor(state->blend_src_factor);
    const GLenum blend_dst_factor_a = get_gl_blend_factor(state->blend_dst_factor_a);
    const GLenum blend_src_factor_a = get_gl_blend_factor(state->blend_src_factor_a);
    if (blend_dst_factor   != glstate->blend_dst_factor   ||
        blend_src_factor   != glstate->blend_src_factor   ||
        blend_dst_factor_a != glstate->blend_dst_factor_a ||
        blend_src_factor_a != glstate->blend_src_factor_a) {
        ngli_glBlendFuncSeparate(gl,
                                 blend_src_factor,
                                 blend_dst_factor,
                                 blend_src_factor_a,
                                 blend_dst_factor_a);
        glstate->blend_dst_factor = blend_dst_factor;
        glstate->blend_src_factor = blend_src_factor;
        glstate->blend_dst_factor_a = blend_dst_factor_a;
        glstate->blend_src_factor_a = blend_src_factor_a;
    }

    const GLenum blend_op   = get_gl_blend_op(state->blend_op);
    const GLenum blend_op_a = get_gl_blend_op(state->blend_op_a);
    if (blend_op   != glstate->blend_op ||
        blend_op_a != glstate->blend_op_a) {
        ngli_glBlendEquationSeparate(gl,
                                     blend_op,
                                     blend_op_a);
        glstate->blend_op   = blend_op;
        glstate->blend_op_a = blend_op_a;
    }

    /* Color */
    GLboolean color_write_mask[4];
    for (int i = 0; i < 4; i++)
        color_write_mask[i] = state->color_write_mask >> i & 1;
    if (memcmp(color_write_mask, glstate->color_write_mask, sizeof(glstate->color_write_mask))) {
        ngli_glColorMask(gl,
                         color_write_mask[0],
                         color_write_mask[1],
                         color_write_mask[2],
                         color_write_mask[3]);
        memcpy(glstate->color_write_mask, color_write_mask, sizeof(glstate->color_write_mask));
    }

    /* Depth */
    const GLenum depth_test = state->depth_test;
    if (depth_test != glstate->depth_test) {
        if (depth_test)
            ngli_glEnable(gl, GL_DEPTH_TEST);
        else
            ngli_glDisable(gl, GL_DEPTH_TEST);
        glstate->depth_test = depth_test;
    }

    const GLboolean depth_write_mask = state->depth_write_mask;
    if (depth_write_mask != glstate->depth_write_mask) {
        ngli_glDepthMask(gl, depth_write_mask);
        glstate->depth_write_mask = depth_write_mask;
    }

    const GLenum depth_func = get_gl_compare_op(state->depth_func);
    if (depth_func != glstate->depth_func) {
        ngli_glDepthFunc(gl, depth_func);
        glstate->depth_func = depth_func;
    }

    /* Stencil */
    const int stencil_test = state->stencil_test;
    if (stencil_test != glstate->stencil_test) {
        if (stencil_test)
            ngli_glEnable(gl, GL_STENCIL_TEST);
        else
            ngli_glDisable(gl, GL_STENCIL_TEST);
        glstate->stencil_test = stencil_test;
    }

    const GLuint stencil_write_mask = state->stencil_write_mask;
    if (stencil_write_mask != glstate->stencil_write_mask) {
        ngli_glStencilMask(gl, stencil_write_mask);
        glstate->stencil_write_mask = stencil_write_mask;
    }

    const GLenum stencil_func       = get_gl_compare_op(state->stencil_func);
    const GLint stencil_ref         = state->stencil_ref;
    const GLuint stencil_read_mask  = state->stencil_read_mask;
    if (stencil_func      != glstate->stencil_func ||
        stencil_ref       != glstate->stencil_ref  ||
        stencil_read_mask != glstate->stencil_read_mask) {
        ngli_glStencilFunc(gl,
                           stencil_func,
                           stencil_ref,
                           stencil_read_mask);
        glstate->stencil_func = stencil_func;
        glstate->stencil_ref = stencil_ref;
        glstate->stencil_read_mask = stencil_read_mask;
    }

    const GLenum stencil_fail       = get_gl_stencil_op(state->stencil_fail);
    const GLenum stencil_depth_fail = get_gl_stencil_op(state->stencil_depth_fail);
    const GLenum stencil_depth_pass = get_gl_stencil_op(state->stencil_depth_pass);
    if (stencil_fail       != glstate->stencil_fail       ||
        stencil_depth_fail != glstate->stencil_depth_fail ||
        stencil_depth_pass != glstate->stencil_depth_pass) {
        ngli_glStencilOp(gl,
                         stencil_fail,
                         stencil_depth_fail,
                         stencil_depth_pass);
        glstate->stencil_fail = stencil_fail;
        glstate->stencil_depth_fail = stencil_depth_fail;
        glstate->stencil_depth_pass = stencil_depth_pass;
    }

    /* Face Culling */
    const int cull_face = state->cull_mode != NGLI_CULL_MODE_NONE;
    if (cull_face != glstate->cull_face) {
        if (cull_face)
            ngli_glEnable(gl, GL_CULL_FACE);
        else
            ngli_glDisable(gl, GL_CULL_FACE);
        glstate->cull_face = cull_face;
    }

    const GLenum cull_face_mode = get_gl_cull_mode(state->cull_mode);
    if (cull_face_mode != glstate->cull_face_mode) {
        ngli_glCullFace(gl, cull_face_mode);
        glstate->cull_face_mode = cull_face_mode;
    }

    /* Scissor */
    const int scissor_test = state->scissor_test;
    if (scissor_test != glstate->scissor_test) {
        if (scissor_test)
            ngli_glEnable(gl, GL_SCISSOR_TEST);
        else
            ngli_glDisable(gl, GL_SCISSOR_TEST);
        glstate->scissor_test = scissor_test;
    }
}

void ngli_glstate_use_program(const struct glcontext *gl, struct glstate *glstate, GLuint program_id)
{
    if (glstate->program_id != program_id) {
        ngli_glUseProgram(gl, program_id);
        glstate->program_id = program_id;
    }
}

void ngli_glstate_update_scissor(const struct glcontext *gl, struct glstate *glstate, const int *scissor)
{
    if (!memcmp(glstate->scissor, scissor, sizeof(glstate->scissor)))
        return;
    memcpy(glstate->scissor, scissor, sizeof(glstate->scissor));
    ngli_glScissor(gl, scissor[0], scissor[1], scissor[2], scissor[3]);
}

void ngli_glstate_update_viewport(const struct glcontext *gl, struct glstate *glstate, const int *viewport)
{
    if (!memcmp(glstate->viewport, viewport, sizeof(glstate->viewport)))
        return;
    memcpy(glstate->viewport, viewport, sizeof(glstate->viewport));
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
}
