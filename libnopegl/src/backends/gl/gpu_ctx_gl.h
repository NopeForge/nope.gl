/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef GPU_CTX_GL_H
#define GPU_CTX_GL_H

#include "config.h"

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "glstate.h"
#include "gpu_rendertarget.h"
#include "gpu_ctx.h"

struct ngl_ctx;
struct gpu_rendertarget;

typedef void (*capture_func_type)(struct gpu_ctx *s);

struct gpu_ctx_gl {
    struct gpu_ctx parent;
    struct glcontext *glcontext;
    struct glstate glstate;
    struct gpu_rendertarget_layout default_rt_layout;
    /* Default rendertarget with load op set to clear */
    struct gpu_rendertarget *default_rt;
    /* Default rendertarget with load op set to load, useful for resuming the
     * associated renderpass (without discarding its attachments) */
    struct gpu_rendertarget *default_rt_load;
    /* Offscreen render target resources */
    struct gpu_texture *color;
    struct gpu_texture *ms_color;
    struct gpu_texture *depth_stencil;
    /* Offscreen capture callback and resources */
    capture_func_type capture_func;
    struct gpu_rendertarget *capture_rt;
    struct gpu_texture *capture_texture;
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

int ngli_gpu_ctx_gl_make_current(struct gpu_ctx *s);
int ngli_gpu_ctx_gl_release_current(struct gpu_ctx *s);
void ngli_gpu_ctx_gl_reset_state(struct gpu_ctx *s);
int ngli_gpu_ctx_gl_wrap_framebuffer(struct gpu_ctx *s, GLuint fbo);

#endif
