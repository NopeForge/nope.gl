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

#include "glstate.h"
#include "glcontext.h"
#include "ngpu/graphics_state.h"
#include <stdlib.h>
#include <string.h>

static const GLenum gl_blend_factor_map[NGPU_BLEND_FACTOR_NB] = {
    [NGPU_BLEND_FACTOR_ZERO]                     = GL_ZERO,
    [NGPU_BLEND_FACTOR_ONE]                      = GL_ONE,
    [NGPU_BLEND_FACTOR_SRC_COLOR]                = GL_SRC_COLOR,
    [NGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR]      = GL_ONE_MINUS_SRC_COLOR,
    [NGPU_BLEND_FACTOR_DST_COLOR]                = GL_DST_COLOR,
    [NGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR]      = GL_ONE_MINUS_DST_COLOR,
    [NGPU_BLEND_FACTOR_SRC_ALPHA]                = GL_SRC_ALPHA,
    [NGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA]      = GL_ONE_MINUS_SRC_ALPHA,
    [NGPU_BLEND_FACTOR_DST_ALPHA]                = GL_DST_ALPHA,
    [NGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA]      = GL_ONE_MINUS_DST_ALPHA,
    [NGPU_BLEND_FACTOR_CONSTANT_COLOR]           = GL_CONSTANT_COLOR,
    [NGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR] = GL_ONE_MINUS_CONSTANT_COLOR,
    [NGPU_BLEND_FACTOR_CONSTANT_ALPHA]           = GL_CONSTANT_ALPHA,
    [NGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA] = GL_ONE_MINUS_CONSTANT_ALPHA,
    [NGPU_BLEND_FACTOR_SRC_ALPHA_SATURATE]       = GL_SRC_ALPHA_SATURATE,
};

static GLenum get_gl_blend_factor(enum ngpu_blend_factor blend_factor)
{
    return gl_blend_factor_map[blend_factor];
}

static const GLenum gl_blend_op_map[NGPU_BLEND_OP_NB] = {
    [NGPU_BLEND_OP_ADD]              = GL_FUNC_ADD,
    [NGPU_BLEND_OP_SUBTRACT]         = GL_FUNC_SUBTRACT,
    [NGPU_BLEND_OP_REVERSE_SUBTRACT] = GL_FUNC_REVERSE_SUBTRACT,
    [NGPU_BLEND_OP_MIN]              = GL_MIN,
    [NGPU_BLEND_OP_MAX]              = GL_MAX,
};

static GLenum get_gl_blend_op(enum ngpu_blend_op blend_op)
{
    return gl_blend_op_map[blend_op];
}

static const GLenum gl_compare_op_map[NGPU_COMPARE_OP_NB] = {
    [NGPU_COMPARE_OP_NEVER]            = GL_NEVER,
    [NGPU_COMPARE_OP_LESS]             = GL_LESS,
    [NGPU_COMPARE_OP_EQUAL]            = GL_EQUAL,
    [NGPU_COMPARE_OP_LESS_OR_EQUAL]    = GL_LEQUAL,
    [NGPU_COMPARE_OP_GREATER]          = GL_GREATER,
    [NGPU_COMPARE_OP_NOT_EQUAL]        = GL_NOTEQUAL,
    [NGPU_COMPARE_OP_GREATER_OR_EQUAL] = GL_GEQUAL,
    [NGPU_COMPARE_OP_ALWAYS]           = GL_ALWAYS,
};

static GLenum get_gl_compare_op(enum ngpu_compare_op compare_op)
{
    return gl_compare_op_map[compare_op];
}

static const GLenum gl_stencil_op_map[NGPU_STENCIL_OP_NB] = {
    [NGPU_STENCIL_OP_KEEP]                = GL_KEEP,
    [NGPU_STENCIL_OP_ZERO]                = GL_ZERO,
    [NGPU_STENCIL_OP_REPLACE]             = GL_REPLACE,
    [NGPU_STENCIL_OP_INCREMENT_AND_CLAMP] = GL_INCR,
    [NGPU_STENCIL_OP_DECREMENT_AND_CLAMP] = GL_DECR,
    [NGPU_STENCIL_OP_INVERT]              = GL_INVERT,
    [NGPU_STENCIL_OP_INCREMENT_AND_WRAP]  = GL_INCR_WRAP,
    [NGPU_STENCIL_OP_DECREMENT_AND_WRAP]  = GL_DECR_WRAP,
};

static GLenum get_gl_stencil_op(enum ngpu_stencil_op stencil_op)
{
    return gl_stencil_op_map[stencil_op];
}

static const GLenum gl_cull_mode_map[NGPU_CULL_MODE_NB] = {
    [NGPU_CULL_MODE_NONE]           = GL_BACK,
    [NGPU_CULL_MODE_FRONT_BIT]      = GL_FRONT,
    [NGPU_CULL_MODE_BACK_BIT]       = GL_BACK,
};

static GLenum get_gl_cull_mode(enum ngpu_cull_mode cull_mode)
{
    return gl_cull_mode_map[cull_mode];
}

static const GLenum gl_front_face_map[NGPU_FRONT_FACE_NB] = {
    [NGPU_FRONT_FACE_COUNTER_CLOCKWISE] = GL_CCW,
    [NGPU_FRONT_FACE_CLOCKWISE]         = GL_CW,
};

static GLenum get_gl_front_face(enum ngpu_front_face front_face)
{
    return gl_front_face_map[front_face];
}

void ngpu_glstate_reset(const struct glcontext *gl, struct ngpu_glstate *glstate)
{
    memset(glstate, 0, sizeof(*glstate));

    /* Blending */
    gl->funcs.Disable(GL_BLEND);
    glstate->blend = 0;

    gl->funcs.BlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glstate->blend_src_factor = GL_ONE;
    glstate->blend_dst_factor = GL_ZERO;
    glstate->blend_src_factor_a = GL_ONE;
    glstate->blend_dst_factor_a = GL_ZERO;

    gl->funcs.BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glstate->blend_op = GL_FUNC_ADD;
    glstate->blend_op_a = GL_FUNC_ADD;

    /* Color write mask */
    gl->funcs.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glstate->color_write_mask[0] = GL_TRUE;
    glstate->color_write_mask[1] = GL_TRUE;
    glstate->color_write_mask[2] = GL_TRUE;
    glstate->color_write_mask[3] = GL_TRUE;

    /* Depth */
    gl->funcs.Disable(GL_DEPTH_TEST);
    glstate->depth_test = 0;

    gl->funcs.DepthMask(GL_TRUE);
    glstate->depth_write_mask = GL_TRUE;

    gl->funcs.DepthFunc(GL_LESS);
    glstate->depth_func = GL_LESS;

    /* Stencil */
    gl->funcs.Disable(GL_STENCIL_TEST);
    glstate->stencil_test = 0;

    /* Use nope.gl's stencil read mask default (0xff) instead of OpenGL's ((GLuint)-1) */
    gl->funcs.StencilMaskSeparate(GL_FRONT, 0xff);
    glstate->stencil_front.write_mask = 0xff;

    gl->funcs.StencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xff);
    glstate->stencil_front.func = GL_ALWAYS;
    glstate->stencil_front.ref = 0;
    glstate->stencil_front.read_mask = 0xff;

    gl->funcs.StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
    glstate->stencil_front.fail = GL_KEEP;
    glstate->stencil_front.depth_fail = GL_KEEP;
    glstate->stencil_front.depth_pass = GL_KEEP;

    gl->funcs.StencilMaskSeparate(GL_BACK, 0xff);
    glstate->stencil_back.write_mask = 0xff;

    gl->funcs.StencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xff);
    glstate->stencil_back.func = GL_ALWAYS;
    glstate->stencil_back.ref = 0;
    glstate->stencil_back.read_mask = 0xff;

    gl->funcs.StencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
    glstate->stencil_back.fail = GL_KEEP;
    glstate->stencil_back.depth_fail = GL_KEEP;
    glstate->stencil_back.depth_pass = GL_KEEP;

    /* Face culling */
    gl->funcs.Disable(GL_CULL_FACE);
    glstate->cull_face = 0;

    gl->funcs.CullFace(GL_BACK);
    glstate->cull_face_mode = GL_BACK;

    gl->funcs.FrontFace(GL_CCW);
    glstate->front_face = GL_CCW;

    /* Scissor */
    gl->funcs.Disable(GL_SCISSOR_TEST);
    glstate->scissor_test = 0;

    /* Program */
    gl->funcs.UseProgram(0);
    glstate->program_id = 0;

    /* VAO */
    gl->funcs.BindVertexArray(0);
}

void ngpu_glstate_update(const struct glcontext *gl, struct ngpu_glstate *glstate, const struct ngpu_graphics_state *state)
{
    /* Blend */
    const int blend = state->blend;
    if (blend != glstate->blend) {
        if (blend)
            gl->funcs.Enable(GL_BLEND);
        else
            gl->funcs.Disable(GL_BLEND);
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
        gl->funcs.BlendFuncSeparate(blend_src_factor,
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
        gl->funcs.BlendEquationSeparate(blend_op, blend_op_a);
        glstate->blend_op   = blend_op;
        glstate->blend_op_a = blend_op_a;
    }

    /* Color */
    GLboolean color_write_mask[4];
    for (size_t i = 0; i < 4; i++)
        color_write_mask[i] = state->color_write_mask >> i & 1;
    if (memcmp(color_write_mask, glstate->color_write_mask, sizeof(glstate->color_write_mask))) {
        gl->funcs.ColorMask(color_write_mask[0],
                            color_write_mask[1],
                            color_write_mask[2],
                            color_write_mask[3]);
        memcpy(glstate->color_write_mask, color_write_mask, sizeof(glstate->color_write_mask));
    }

    /* Depth */
    const GLenum depth_test = state->depth_test;
    if (depth_test != glstate->depth_test) {
        if (depth_test)
            gl->funcs.Enable(GL_DEPTH_TEST);
        else
            gl->funcs.Disable(GL_DEPTH_TEST);
        glstate->depth_test = depth_test;
    }

    const GLboolean depth_write_mask = (GLboolean)state->depth_write_mask;
    if (depth_write_mask != glstate->depth_write_mask) {
        gl->funcs.DepthMask(depth_write_mask);
        glstate->depth_write_mask = depth_write_mask;
    }

    const GLenum depth_func = get_gl_compare_op(state->depth_func);
    if (depth_func != glstate->depth_func) {
        gl->funcs.DepthFunc(depth_func);
        glstate->depth_func = depth_func;
    }

    /* Stencil */
    const int stencil_test = state->stencil_test;
    if (stencil_test != glstate->stencil_test) {
        if (stencil_test)
            gl->funcs.Enable(GL_STENCIL_TEST);
        else
            gl->funcs.Disable(GL_STENCIL_TEST);
        glstate->stencil_test = stencil_test;
    }

    /* Stencil front operations */
    {
        const GLuint stencil_write_mask = state->stencil_front.write_mask;
        if (stencil_write_mask != glstate->stencil_front.write_mask) {
            gl->funcs.StencilMaskSeparate(GL_FRONT, stencil_write_mask);
            glstate->stencil_front.write_mask = stencil_write_mask;
        }

        const GLenum stencil_func = get_gl_compare_op(state->stencil_front.func);
        const GLint stencil_ref = (GLint)state->stencil_front.ref;
        const GLuint stencil_read_mask = state->stencil_front.read_mask;
        if (stencil_func != glstate->stencil_front.func ||
            stencil_ref != glstate->stencil_front.ref ||
            stencil_read_mask != glstate->stencil_front.read_mask) {
            gl->funcs.StencilFuncSeparate(GL_FRONT, stencil_func, stencil_ref, stencil_read_mask);
            glstate->stencil_front.func = stencil_func;
            glstate->stencil_front.ref = stencil_ref;
            glstate->stencil_front.read_mask = stencil_read_mask;
        }

        const GLenum stencil_fail = get_gl_stencil_op(state->stencil_front.fail);
        const GLenum stencil_depth_fail = get_gl_stencil_op(state->stencil_front.depth_fail);
        const GLenum stencil_depth_pass = get_gl_stencil_op(state->stencil_front.depth_pass);
        if (stencil_fail != glstate->stencil_front.fail ||
            stencil_depth_fail != glstate->stencil_front.depth_fail ||
            stencil_depth_pass != glstate->stencil_front.depth_pass) {
            gl->funcs.StencilOpSeparate(GL_FRONT, stencil_fail, stencil_depth_fail, stencil_depth_pass);
            glstate->stencil_front.fail = stencil_fail;
            glstate->stencil_front.depth_fail = stencil_depth_fail;
            glstate->stencil_front.depth_pass = stencil_depth_pass;
        }
    }

    /* Stencil back operations */
    {
        const GLuint stencil_write_mask = state->stencil_back.write_mask;
        if (stencil_write_mask != glstate->stencil_back.write_mask) {
            gl->funcs.StencilMaskSeparate(GL_BACK, stencil_write_mask);
            glstate->stencil_back.write_mask = stencil_write_mask;
        }

        const GLenum stencil_func = get_gl_compare_op(state->stencil_back.func);
        const GLint stencil_ref = (GLint)state->stencil_back.ref;
        const GLuint stencil_read_mask = state->stencil_back.read_mask;
        if (stencil_func != glstate->stencil_back.func ||
            stencil_ref != glstate->stencil_back.ref ||
            stencil_read_mask != glstate->stencil_back.read_mask) {
            gl->funcs.StencilFuncSeparate(GL_BACK, stencil_func, stencil_ref, stencil_read_mask);
            glstate->stencil_back.func = stencil_func;
            glstate->stencil_back.ref = stencil_ref;
            glstate->stencil_back.read_mask = stencil_read_mask;
        }

        const GLenum stencil_fail = get_gl_stencil_op(state->stencil_back.fail);
        const GLenum stencil_depth_fail = get_gl_stencil_op(state->stencil_back.depth_fail);
        const GLenum stencil_depth_pass = get_gl_stencil_op(state->stencil_back.depth_pass);
        if (stencil_fail != glstate->stencil_back.fail ||
            stencil_depth_fail != glstate->stencil_back.depth_fail ||
            stencil_depth_pass != glstate->stencil_back.depth_pass) {
            gl->funcs.StencilOpSeparate(GL_BACK, stencil_fail, stencil_depth_fail, stencil_depth_pass);
            glstate->stencil_back.fail = stencil_fail;
            glstate->stencil_back.depth_fail = stencil_depth_fail;
            glstate->stencil_back.depth_pass = stencil_depth_pass;
        }
    }

    /* Face Culling */
    const GLboolean cull_face = state->cull_mode != NGPU_CULL_MODE_NONE;
    if (cull_face != glstate->cull_face) {
        if (cull_face)
            gl->funcs.Enable(GL_CULL_FACE);
        else
            gl->funcs.Disable(GL_CULL_FACE);
        glstate->cull_face = cull_face;
    }

    const GLenum cull_face_mode = get_gl_cull_mode(state->cull_mode);
    if (cull_face_mode != glstate->cull_face_mode) {
        gl->funcs.CullFace(cull_face_mode);
        glstate->cull_face_mode = cull_face_mode;
    }

    /* Front Face */
    const GLenum front_face = get_gl_front_face(state->front_face);
    if (front_face != glstate->front_face) {
        gl->funcs.FrontFace(front_face);
        glstate->front_face = front_face;
    }
}

void ngpu_glstate_use_program(const struct glcontext *gl, struct ngpu_glstate *glstate, GLuint program_id)
{
    if (glstate->program_id != program_id) {
        gl->funcs.UseProgram(program_id);
        glstate->program_id = program_id;
    }
}

void ngpu_glstate_update_scissor(const struct glcontext *gl, struct ngpu_glstate *glstate, const struct ngpu_scissor *scissor)
{
    if (glstate->scissor.x == scissor->x &&
        glstate->scissor.y == scissor->y &&
        glstate->scissor.width == scissor->width &&
        glstate->scissor.height == scissor->height)
        return;
    glstate->scissor = *scissor;
    gl->funcs.Scissor(scissor->x, scissor->y, scissor->width, scissor->height);
}

void ngpu_glstate_update_viewport(const struct glcontext *gl, struct ngpu_glstate *glstate, const struct ngpu_viewport *viewport)
{
    if (glstate->viewport.x == viewport->x &&
        glstate->viewport.y == viewport->y &&
        glstate->viewport.width == viewport->width &&
        glstate->viewport.height == viewport->height)
        return;
    glstate->viewport = *viewport;
    gl->funcs.Viewport(viewport->x, viewport->y, viewport->width, viewport->height);
}

void ngpu_glstate_enable_scissor_test(const struct glcontext *gl, struct ngpu_glstate *glstate, int enable)
{
    if (glstate->scissor_test == enable)
        return;
    if (enable)
        gl->funcs.Enable(GL_SCISSOR_TEST);
    else
        gl->funcs.Disable(GL_SCISSOR_TEST);
    glstate->scissor_test = !!enable;
}
