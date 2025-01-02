/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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

#ifndef NGPU_CTX_GL_H
#define NGPU_CTX_GL_H

#include "config.h"

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "cmd_buffer_gl.h"
#include "glstate.h"
#include "ngpu/ctx.h"
#include "ngpu/rendertarget.h"

struct ngl_ctx;
struct ngpu_rendertarget;

typedef void (*capture_func_type)(struct ngpu_ctx *s);

struct ngpu_ctx_gl {
    struct ngpu_ctx parent;
    struct glcontext *glcontext;
    struct glstate glstate;
    uint32_t nb_in_flight_frames;
    uint32_t current_frame_index;
    struct ngpu_cmd_buffer_gl **update_cmd_buffers;
    struct ngpu_cmd_buffer_gl **draw_cmd_buffers;
    struct ngpu_cmd_buffer_gl *cur_cmd_buffer;
    struct ngpu_rendertarget_layout default_rt_layout;
    /* Default rendertarget with load op set to clear */
    struct ngpu_rendertarget *default_rt;
    /* Default rendertarget with load op set to load, useful for resuming the
     * associated renderpass (without discarding its attachments) */
    struct ngpu_rendertarget *default_rt_load;
    /* Offscreen render target resources */
    struct ngpu_texture *color;
    struct ngpu_texture *ms_color;
    struct ngpu_texture *depth_stencil;
    /* Offscreen capture callback and resources */
    capture_func_type capture_func;
    struct ngpu_rendertarget *capture_rt;
    struct ngpu_texture *capture_texture;
#if defined(TARGET_IPHONE)
    CVPixelBufferRef capture_cvbuffer;
    CVOpenGLESTextureRef capture_cvtexture;
#endif
    /* Timer */
    GLuint queries[2];
    void (NGLI_GL_APIENTRY *glGenQueries)(GLsizei n, GLuint * ids);
    void (NGLI_GL_APIENTRY *glDeleteQueries)(GLsizei n, const GLuint *ids);
    void (NGLI_GL_APIENTRY *glBeginQuery)(GLenum target, GLuint id);
    void (NGLI_GL_APIENTRY *glEndQuery)(GLenum target);
    void (NGLI_GL_APIENTRY *glQueryCounter)(GLuint id, GLenum target);
    void (NGLI_GL_APIENTRY *glGetQueryObjectui64v)(GLuint id, GLenum pname, GLuint64 *params);
};

int ngpu_ctx_gl_make_current(struct ngpu_ctx *s);
int ngpu_ctx_gl_release_current(struct ngpu_ctx *s);
void ngpu_ctx_gl_reset_state(struct ngpu_ctx *s);
int ngpu_ctx_gl_wrap_framebuffer(struct ngpu_ctx *s, GLuint fbo);

#endif
