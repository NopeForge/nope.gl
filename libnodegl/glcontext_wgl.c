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

struct wgl_priv {
    HGLRC handle;
    HDC window;
    HMODULE module;
};

static int wgl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t handle)
{
    struct wgl_priv *wgl = ctx->priv_data;

    wgl->handle = wglGetCurrentContext();
    wgl->window = wglGetCurrentDC();

    if (!wgl->window || !wgl->handle)
        return -1;

    wgl->module = LoadLibrary("opengl32.dll");
    if (!wgl->module)
        return -1;

    return 0;
}

static void wgl_uninit(struct glcontext *ctx)
{
    struct wgl_priv *wgl = ctx->priv_data;

    if (wgl->module)
        FreeLibrary(wgl->module);
}

static int wgl_create(struct glcontext *ctx, uintptr_t other)
{
    return -1;
}

static int wgl_make_current(struct glcontext *ctx, int current)
{
    int ret;
    struct wgl_priv *wgl = ctx->priv_data;

    if (current) {
        ret = wglMakeCurrent(wgl->window, wgl->handle);
    } else {
        ret = wglMakeCurrent(wgl->window, NULL);
    }

    return ret - 1;
}

static void *wgl_get_proc_address(struct glcontext *ctx, const char *name)
{
    struct wgl_priv *wgl = ctx->priv_data;

    void *addr = GetProcAddress(wgl->module, name);
    if (!addr)
        addr = wglGetProcAddress((LPCSTR)name);

    return addr;
}

const struct glcontext_class ngli_glcontext_wgl_class = {
    .init = wgl_init,
    .uninit = wgl_uninit,
    .create = wgl_create,
    .make_current = wgl_make_current,
    .get_proc_address = wgl_get_proc_address,
    .priv_size = sizeof(struct wgl_priv),
};
