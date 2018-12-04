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

#include <stdio.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "utils.h"

struct x11_priv {
    Display *display;
    int own_display;
    Window window;
    int own_window;
    GLXContext handle;
    GLXFBConfig *fbconfigs;
    int nb_fbconfigs;
    GLXContext (*CreateContextAttribs)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    int swap_interval_ext;
    int swap_interval_mesa;
    int swap_interval_sgi;
    void (*SwapIntervalEXT)(Display*, GLXDrawable, int);
    int (*SwapIntervalMESA)(int);
    int (*SwapIntervalSGI)(int);
};

static int x11_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct x11_priv *x11 = ctx->priv_data;

    x11->display = (Display *)display;
    if (!x11->display) {
        x11->display = XOpenDisplay(NULL);
        if (!x11->display) {
            LOG(ERROR, "could not retrieve GLX display");
            return -1;
        }
        x11->own_display = 1;
    }

    const int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        GLX_SAMPLE_BUFFERS, ctx->samples > 0,
        GLX_SAMPLES, ctx->samples,
        None
    };

    x11->fbconfigs = glXChooseFBConfig(x11->display,
                                       DefaultScreen(x11->display),
                                       attribs,
                                       &x11->nb_fbconfigs);
    if (!x11->fbconfigs) {
        LOG(ERROR, "could not choose a valid framebuffer configuration");
        return -1;
    }

    x11->CreateContextAttribs = (void *)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
    if (!x11->CreateContextAttribs) {
        LOG(ERROR, "could not retrieve glXCreateContextAttribsARB()");
        return -1;
    }

    int screen = DefaultScreen(x11->display);
    GLXFBConfig *fbconfigs = x11->fbconfigs;

    const char *glx_extensions = glXQueryExtensionsString(x11->display, screen);
    if (!ngli_glcontext_check_extension("GLX_ARB_create_context", glx_extensions)) {
        LOG(ERROR, "context does not support GLX_ARB_create_context extension");
        return -1;
    }

    GLXContext shared_context = other ? (GLXContext)other : NULL;

    if (ctx->backend == NGL_BACKEND_OPENGLES) {
        if (!ngli_glcontext_check_extension("GLX_EXT_create_context_es2_profile", glx_extensions)) {
            LOG(ERROR, "context does not support GLX_EXT_create_context_es2_profile extension");
            return -1;
        }

        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_ES2_PROFILE_BIT_EXT,
            GLX_CONTEXT_FLAGS_ARB, 0,
            None
        };

        x11->handle = x11->CreateContextAttribs(x11->display,
                                                fbconfigs[0],
                                                shared_context,
                                                1,
                                                attribs);
    } else if (ctx->backend == NGL_BACKEND_OPENGL) {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };

        x11->handle = x11->CreateContextAttribs(x11->display,
                                                fbconfigs[0],
                                                shared_context,
                                                1,
                                                attribs);
    }

    if (!x11->handle) {
        LOG(ERROR, "could not create GLX context");
        return -1;
    }

    if (ctx->offscreen) {
        int attribs[] = {
            GLX_PBUFFER_WIDTH, ctx->width,
            GLX_PBUFFER_HEIGHT, ctx->height,
            None
        };

        x11->window = glXCreatePbuffer(x11->display, fbconfigs[0], attribs);
        if (!x11->window) {
            LOG(ERROR, "could not create offscreen pixel buffer");
            return -1;
        }
        x11->own_window = 1;
    } else {
        x11->window = (Window)window;
        if (!x11->window) {
            LOG(ERROR, "could not retrieve GLX window");
            return -1;
        }
    }

    if (ngli_glcontext_check_extension("GLX_EXT_swap_control", glx_extensions)) {
        x11->SwapIntervalEXT = (void *)glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");
        if (x11->SwapIntervalEXT)
            x11->swap_interval_ext = 1;
    } else if (ngli_glcontext_check_extension("GLX_MESA_swap_control", glx_extensions)) {
        x11->SwapIntervalMESA = (void *)glXGetProcAddress((const GLubyte *)"glXSwapIntervalMESA");
        if (x11->SwapIntervalMESA)
            x11->swap_interval_mesa = 1;
    } else if (ngli_glcontext_check_extension("GLX_SGI_swap_control", glx_extensions)) {
        x11->SwapIntervalSGI = (void *)glXGetProcAddress((const GLubyte *)"glXSwapIntervalSGI");
        if (x11->SwapIntervalSGI)
            x11->swap_interval_sgi = 1;
    } else {
        LOG(WARNING, "context does not support any swap interval extension");
    }

    return 0;
}

static void x11_uninit(struct glcontext *ctx)
{
    struct x11_priv *x11 = ctx->priv_data;

    if (x11->fbconfigs)
        XFree(x11->fbconfigs);

    if (x11->handle)
        glXDestroyContext(x11->display, x11->handle);

    if (x11->own_window)
        glXDestroyPbuffer(x11->display, x11->window);

    if (x11->own_display)
        XCloseDisplay(x11->display);
}

static int x11_make_current(struct glcontext *ctx, int current)
{
    int ret;
    struct x11_priv *x11 = ctx->priv_data;

    if (current) {
        ret = glXMakeCurrent(x11->display, x11->window, x11->handle);
    } else {
        ret = glXMakeCurrent(x11->display, None, NULL);
    }

    return ret - 1;
}

static void x11_swap_buffers(struct glcontext *ctx)
{
    struct x11_priv *x11 = ctx->priv_data;
    glXSwapBuffers(x11->display, x11->window);
}

static int x11_set_swap_interval(struct glcontext *ctx, int interval)
{
    struct x11_priv *x11 = ctx->priv_data;

    if (x11->swap_interval_ext) {
        x11->SwapIntervalEXT(x11->display, x11->window, interval);
    } else if (x11->swap_interval_mesa) {
        x11->SwapIntervalMESA(interval);
    } else if (x11->swap_interval_sgi) {
        x11->SwapIntervalSGI(interval);
    } else {
        LOG(WARNING, "context does not support swap interval operation");
    }

    return 0;
}

static void *x11_get_proc_address(struct glcontext *ctx, const char *name)
{
    return glXGetProcAddress((const GLubyte *)name);
}

const struct glcontext_class ngli_glcontext_x11_class = {
    .init = x11_init,
    .uninit = x11_uninit,
    .make_current = x11_make_current,
    .swap_buffers = x11_swap_buffers,
    .set_swap_interval = x11_set_swap_interval,
    .get_proc_address = x11_get_proc_address,
    .priv_size = sizeof(struct x11_priv),
};
