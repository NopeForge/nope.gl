/*
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

#include "bindgroup.h"
#include "nopegl.h"
#include "glstate.h"
#include "graphics_state.h"
#include "rendertarget.h"
#include "pgcache.h"
#include "pipeline.h"
#include "gpu_ctx.h"

struct ngl_ctx;
struct rendertarget;

typedef void (*capture_func_type)(struct gpu_ctx *s);

struct gpu_ctx_gl {
    struct gpu_ctx parent;
    struct glcontext *glcontext;
    struct glstate glstate;
    struct rendertarget_layout default_rt_layout;
    /* Default rendertarget with load op set to clear */
    struct rendertarget *default_rt;
    /* Default rendertarget with load op set to load, useful for resuming the
     * associated renderpass (without discarding its attachments) */
    struct rendertarget *default_rt_load;
    /* Offscreen render target resources */
    struct texture *color;
    struct texture *ms_color;
    struct texture *depth_stencil;
    /* Offscreen capture callback and resources */
    capture_func_type capture_func;
#if defined(TARGET_IPHONE)
    CVPixelBufferRef capture_cvbuffer;
    CVOpenGLESTextureRef capture_cvtexture;
#endif
    /* Timer */
    GLuint queries[2];
    void (*glGenQueries)(const struct glcontext *gl, GLsizei n, GLuint * ids);
    void (*glDeleteQueries)(const struct glcontext *gl, GLsizei n, const GLuint *ids);
    void (*glBeginQuery)(const struct glcontext *gl, GLenum target, GLuint id);
    void (*glEndQuery)(const struct glcontext *gl, GLenum target);
    void (*glQueryCounter)(const struct glcontext *gl, GLuint id, GLenum target);
    void (*glGetQueryObjectui64v)(const struct glcontext *gl, GLuint id, GLenum pname, GLuint64 *params);
};

int ngli_gpu_ctx_gl_make_current(struct gpu_ctx *s);
int ngli_gpu_ctx_gl_release_current(struct gpu_ctx *s);
void ngli_gpu_ctx_gl_reset_state(struct gpu_ctx *s);
int ngli_gpu_ctx_gl_wrap_framebuffer(struct gpu_ctx *s, GLuint fbo);

#endif
