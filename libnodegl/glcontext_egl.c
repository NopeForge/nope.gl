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
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "utils.h"

#define EGL_PLATFORM_X11 0x31D5

struct egl_priv {
    EGLNativeDisplayType native_display;
    EGLNativeWindowType native_window;
    EGLDisplay display;
    int own_display;
    EGLSurface surface;
    int own_surface;
    EGLContext handle;
    int own_handle;
    EGLConfig config;
    EGLBoolean (*PresentationTimeANDROID)(EGLDisplay dpy, EGLSurface sur, khronos_stime_nanoseconds_t time);
    EGLDisplay (*GetPlatformDisplay)(EGLenum platform, void *native_display, const EGLint *attrib_list);
};

static int egl_probe_android_presentation_time_ext(struct egl_priv *egl)
{
#if defined(TARGET_ANDROID)
    char const *extensions = eglQueryString(egl->display, EGL_EXTENSIONS);
    if (!extensions)
        return 0;

    if (ngli_glcontext_check_extension("EGL_ANDROID_presentation_time", extensions)) {
        egl->PresentationTimeANDROID = (void *)eglGetProcAddress("eglPresentationTimeANDROID");
        if (!egl->PresentationTimeANDROID) {
            LOG(ERROR, "could not retrieve eglPresentationTimeANDROID()");
            return -1;
        }
        return 1;
    }
#endif

    return 0;
}

#if defined(TARGET_LINUX)
static int egl_probe_platform_x11_ext(struct egl_priv *egl)
{
    char const *extensions = eglQueryString(egl->display, EGL_EXTENSIONS);
    if (!extensions)
        return 0;

    if (ngli_glcontext_check_extension("EGL_KHR_platform_x11", extensions) ||
        ngli_glcontext_check_extension("EGL_EXT_platform_x11", extensions)) {
        egl->GetPlatformDisplay = (void *)eglGetProcAddress("eglGetPlatformDisplay");
        if (!egl->GetPlatformDisplay)
            egl->GetPlatformDisplay = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (!egl->GetPlatformDisplay) {
            LOG(ERROR, "could not retrieve eglGetPlatformDisplay()");
            return -1;
        }
        return 1;
    }

    return 0;
}
#endif

static EGLDisplay egl_get_display(struct egl_priv *egl)
{
#if defined(TARGET_ANDROID)
    return eglGetDisplay(egl->native_display);
#elif defined(TARGET_LINUX)
    /* XXX: only X11 is supported for now */
    int ret = egl_probe_platform_x11_ext(egl);
    if (ret <= 0)
        return EGL_NO_DISPLAY;
    return egl->GetPlatformDisplay(EGL_PLATFORM_X11, egl->native_display, NULL);
#else
    return EGL_NO_DISPLAY;
#endif
}

static int egl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct egl_priv *egl = ctx->priv_data;

    egl->native_display = display ? (EGLNativeDisplayType)display : EGL_DEFAULT_DISPLAY;

    if (!ctx->offscreen) {
        if (window) {
            egl->native_window = (EGLNativeWindowType)window;
            if (!egl->native_window) {
                LOG(ERROR, "could not retrieve EGL native window");
                return -1;
            }
        }
    }

    int ret;
    egl->display = egl_get_display(egl);
    if (!egl->display) {
        LOG(ERROR, "could not retrieve EGL display");
        return -1;
    }

    EGLint egl_minor;
    EGLint egl_major;
    ret = eglInitialize(egl->display, &egl_major, &egl_minor);
    if (!ret) {
        LOG(ERROR, "could initialize EGL: 0x%x", eglGetError());
        return -1;
    }
    egl->own_display = 1;

    if (egl_major < 1 || egl_minor < 4) {
        LOG(ERROR, "unsupported EGL version %d.%d, only 1.4+ is supported", egl_major, egl_minor);
        return -1;
    }

    int api = ctx->backend == NGL_BACKEND_OPENGL ? EGL_OPENGL_API : EGL_OPENGL_ES_API;
    if (!eglBindAPI(api)) {
        LOG(ERROR, "could not bind OpenGL%s API", ctx->backend == NGL_BACKEND_OPENGL ? "" : "ES");
        return -1;
    }

    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_SAMPLES, 0,
        EGL_NONE
    };

    if (ctx->backend == NGL_BACKEND_OPENGL) {
        config_attribs[1] = EGL_OPENGL_BIT;
    }

    if (ctx->samples > 0) {
        config_attribs[NGLI_ARRAY_NB(config_attribs) - 4] = 1;
        config_attribs[NGLI_ARRAY_NB(config_attribs) - 2] = ctx->samples;
    }

    EGLConfig config;
    EGLint nb_configs;
    ret = eglChooseConfig(egl->display, config_attribs, &config, 1, &nb_configs);
    if (!ret || !nb_configs) {
        LOG(ERROR, "could not choose a valid EGL configuration: 0x%x", eglGetError());
        return -1;
    }

    EGLContext shared_context = other ? (EGLContext)other : NULL;

    if (ctx->backend == NGL_BACKEND_OPENGL) {
        static const EGLint ctx_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_NONE
        };

        egl->handle = eglCreateContext(egl->display, config, shared_context, ctx_attribs);
    } else {
        static const EGLint ctx_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        egl->handle = eglCreateContext(egl->display, config, shared_context, ctx_attribs);
    }

    if (!egl->handle) {
        LOG(ERROR, "could not create EGL context: 0x%x", eglGetError());
        return -1;
    }
    egl->own_handle = 1;

    if (ctx->offscreen) {
        const EGLint attribs[] = {
            EGL_WIDTH, ctx->width,
            EGL_HEIGHT, ctx->height,
            EGL_NONE
        };

        egl->surface = eglCreatePbufferSurface(egl->display, config, attribs);
        if (!egl->surface) {
            LOG(ERROR, "could not create EGL window surface: 0x%x", eglGetError());
            return -1;
        }
    } else {
        egl->surface = eglCreateWindowSurface(egl->display, config, egl->native_window, NULL);
        if (!egl->surface) {
            LOG(ERROR, "could not create EGL window surface: 0x%x", eglGetError());
        }
    }
    egl->own_surface = 1;

    ret = egl_probe_android_presentation_time_ext(egl);
    if (ret < 0)
        return ret;

    return 0;
}

static void egl_uninit(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

    ngli_glcontext_make_current(ctx, 0);

    if (egl->own_surface)
        eglDestroySurface(egl->display, egl->surface);

    if (egl->own_handle)
        eglDestroyContext(egl->display, egl->handle);

    if (egl->own_display)
        eglTerminate(egl->display);
}

static int egl_make_current(struct glcontext *ctx, int current)
{
    int ret;
    struct egl_priv *egl = ctx->priv_data;

    if (current) {
        ret = eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->handle);
    } else {
        ret = eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    return ret - 1;
}

static void egl_swap_buffers(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;
    eglSwapBuffers(egl->display, egl->surface);
}

static int egl_set_swap_interval(struct glcontext *ctx, int interval)
{
    struct egl_priv *egl = ctx->priv_data;

    eglSwapInterval(egl->display, interval);

    return 0;
}

static void egl_set_surface_pts(struct glcontext *ctx, double t)
{
#if defined(TARGET_ANDROID)
    struct egl_priv *egl = ctx->priv_data;

    if (egl->PresentationTimeANDROID) {
        EGLnsecsANDROID pts = t * 1000000000LL;
        egl->PresentationTimeANDROID(egl->display, egl->surface, pts);
    }
#endif
}

static void *egl_get_proc_address(struct glcontext *ctx, const char *name)
{
    return eglGetProcAddress(name);
}

const struct glcontext_class ngli_glcontext_egl_class = {
    .init = egl_init,
    .uninit = egl_uninit,
    .make_current = egl_make_current,
    .swap_buffers = egl_swap_buffers,
    .set_swap_interval = egl_set_swap_interval,
    .set_surface_pts = egl_set_surface_pts,
    .get_proc_address = egl_get_proc_address,
    .priv_size = sizeof(struct egl_priv),
};
