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

#include <stdio.h>
#include <windows.h>

#include <GL/glcorearb.h>
#include <GL/wglext.h>

#include "glcontext.h"
#include "nodegl.h"

struct glcontext_wgl {
    HGLRC handle;
    HDC window;
    HMODULE module;
};

static int glcontext_wgl_init(struct glcontext *glcontext, void *display, void *window, void *handle)
{
    struct glcontext_wgl *glcontext_wgl = glcontext->priv_data;

    glcontext_wgl->handle = handle ? *(HGLRC *)handle : wglGetCurrentContext();
    glcontext_wgl->window = window ? *(HDC *)window   : wglGetCurrentDC();

    if (!glcontext_wgl->window || !glcontext_wgl->handle)
        return -1;

    glcontext_wgl->module = LoadLibrary("opengl32.dll");
    if (!glcontext_wgl->module)
        return -1;

    return 0;
}

static void glcontext_wgl_uninit(struct glcontext *glcontext)
{
    struct glcontext_wgl *glcontext_wgl = glcontext->priv_data;

    if (glcontext_wgl->module)
        FreeLibrary(glcontext_wgl->module);
}

static int glcontext_wgl_create(struct glcontext *glcontext, struct glcontext *other)
{
    return -1;
}

static int glcontext_wgl_make_current(struct glcontext *glcontext, int current)
{
    int ret;
    struct glcontext_wgl *glcontext_wgl = glcontext->priv_data;

    if (current) {
        ret = wglMakeCurrent(glcontext_wgl->window, glcontext_wgl->handle);
    } else {
        ret = wglMakeCurrent(glcontext_wgl->window, NULL);
    }

    return ret - 1;
}

static void *glcontext_wgl_get_display(struct glcontext *glcontext)
{
    return NULL;
}

static void *glcontext_wgl_get_window(struct glcontext *glcontext)
{
    struct glcontext_wgl *glcontext_wgl = glcontext->priv_data;
    return &glcontext_wgl->window;
}

static void *glcontext_wgl_get_handle(struct glcontext *glcontext)
{
    struct glcontext_wgl *glcontext_wgl = glcontext->priv_data;
    return &glcontext_wgl->handle;
}

static void *glcontext_wgl_get_proc_address(struct glcontext *glcontext, const char *name)
{
    struct glcontext_wgl *glcontext_wgl = glcontext->priv_data;

    void *addr = GetProcAddress(glcontext_wgl->module, name);
    if (!addr)
        addr = wglGetProcAddress((LPCSTR)name);

    return addr;
}

const struct glcontext_class ngli_glcontext_wgl_class = {
    .init = glcontext_wgl_init,
    .uninit = glcontext_wgl_uninit,
    .create = glcontext_wgl_create,
    .make_current = glcontext_wgl_make_current,
    .get_display = glcontext_wgl_get_display,
    .get_window = glcontext_wgl_get_window,
    .get_handle = glcontext_wgl_get_handle,
    .get_proc_address = glcontext_wgl_get_proc_address,
    .priv_size = sizeof(struct glcontext_wgl),
};
