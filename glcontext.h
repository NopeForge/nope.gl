/*
 * Copyright 2016 GoPro Inc.
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

#include "glincludes.h"

#define NGLI_GL_FUNC(flags, ret, name, ...) ret (*name)(__VA_ARGS__);

struct glfunctions {
#include "glfunctions.h"
};

#undef NGLI_GL_FUNC

struct glcontext_class;

struct glcontext {
    /* GL context */
    const struct glcontext_class *class;
    int platform;
    int api;
    int wrapped;
    void *priv_data;

    /* GL api */
    int loaded;
    int major_version;
    int minor_version;
    int has_es2_compatibility;
    int has_vao_compatibility;
    int max_texture_image_units;

    struct glfunctions funcs;
};

struct glcontext_class {
    int (*init)(struct glcontext *glcontext, void *display, void *window, void *handle);
    int (*create)(struct glcontext *glcontext, struct glcontext *other);
    int (*make_current)(struct glcontext *glcontext, int current);
    void (*swap_buffers)(struct glcontext *glcontext);
    void* (*get_display)(struct glcontext *glcontext);
    void* (*get_window)(struct glcontext *glcontext);
    void* (*get_handle)(struct glcontext *glcontext);
    void* (*get_proc_address)(struct glcontext *glcontext, const char *name);
    void (*uninit)(struct glcontext *glcontext);
    size_t priv_size;
};

struct glcontext *ngli_glcontext_new_wrapped(void *display, void *window, void *handle, int platform, int api);
struct glcontext *ngli_glcontext_new_shared(struct glcontext *other);
int ngli_glcontext_load_extensions(struct glcontext *glcontext);
int ngli_glcontext_make_current(struct glcontext *glcontext, int current);
void ngli_glcontext_swap_buffers(struct glcontext *glcontext);
void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name);
void *ngli_glcontext_get_handle(struct glcontext *glcontext);
void ngli_glcontext_freep(struct glcontext **glcontext);
int ngli_glcontext_check_extension(const char *extension, const char *extensions);
int ngli_glcontext_check_gl_error(struct glcontext *glcontext);

#endif /* GLCONTEXT_H */
