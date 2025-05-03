/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2016-2022 GoPro Inc.
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

#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#include <stdlib.h>

#include "feature_gl.h"
#include "format_gl.h"
#include "glfunctions.h"
#include "ngpu/limits.h"
#include "nopegl.h"

struct glcontext_class;

struct glcontext_params {
    enum ngl_platform_type platform;
    enum ngl_backend_type backend;
    int external;
    uintptr_t display;
    uintptr_t window;
    uintptr_t shared_ctx;
    int swap_interval;
    int offscreen;
    int32_t width;
    int32_t height;
    int32_t samples;
    int debug;
};

struct glcontext {
    /* GL context */
    const struct glcontext_class *cls;
    void *priv_data;

    /* User options */
    enum ngl_platform_type platform;
    enum ngl_backend_type backend;
    int external;
    int offscreen;
    int32_t width;
    int32_t height;
    int32_t samples;
    int debug;

    /* GL api */
    int version;

    /* GLSL version */
    int glsl_version;

    /* GL features */
    uint64_t features;

    /* GL limits */
    struct ngpu_limits limits;

    /* GL functions */
    struct glfunctions funcs;

    /* GL formats */
    struct ngpu_format_gl formats[NGPU_FORMAT_NB];

    /*
     * Workaround a radeonsi sync issue between fbo writes and compute reads
     * using 2D samplers.
     *
     * See: https://gitlab.freedesktop.org/mesa/mesa/-/issues/8906
     */
    int workaround_radeonsi_sync;
};

struct glcontext_class {
    int (*init)(struct glcontext *glcontext, uintptr_t display, uintptr_t window, uintptr_t handle);
    int (*resize)(struct glcontext *glcontext, int32_t width, int32_t height);
    int (*make_current)(struct glcontext *glcontext, int current);
    void (*swap_buffers)(struct glcontext *glcontext);
    int (*set_swap_interval)(struct glcontext *glcontext, int interval);
    void (*set_surface_pts)(struct glcontext *glcontext, double t);
    void* (*get_texture_cache)(struct glcontext *glcontext);
    void* (*get_proc_address)(struct glcontext *glcontext, const char *name);
    uintptr_t (*get_display)(struct glcontext *glcontext);
    uintptr_t (*get_handle)(struct glcontext *glcontext);
    GLuint (*get_default_framebuffer)(struct glcontext *glcontext);
    void (*uninit)(struct glcontext *glcontext);
    size_t priv_size;
};

struct glcontext *ngli_glcontext_create(const struct glcontext_params *params);
int ngli_glcontext_make_current(struct glcontext *glcontext, int current);
void ngli_glcontext_swap_buffers(struct glcontext *glcontext);
int ngli_glcontext_set_swap_interval(struct glcontext *glcontext, int interval);
void ngli_glcontext_set_surface_pts(struct glcontext *glcontext, double t);
int ngli_glcontext_resize(struct glcontext *glcontext, int32_t width, int32_t height);
void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name);
void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext);
uintptr_t ngli_glcontext_get_display(struct glcontext *glcontext);
uintptr_t ngli_glcontext_get_handle(struct glcontext *glcontext);
GLuint ngli_glcontext_get_default_framebuffer(struct glcontext *glcontext);
void ngli_glcontext_freep(struct glcontext **glcontext);
int ngli_glcontext_check_extension(const char *extension, const char *extensions);
int ngli_glcontext_check_gl_error(const struct glcontext *glcontext, const char *context);

#endif /* GLCONTEXT_H */
