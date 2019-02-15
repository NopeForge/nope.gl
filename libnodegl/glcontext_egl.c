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

#if defined(TARGET_LINUX)
#include <X11/Xlib.h>
#endif

#include "egl.h"
#include "fbo.h"
#include "format.h"
#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "utils.h"

#define EGL_PLATFORM_X11 0x31D5

struct egl_priv {
    EGLNativeDisplayType native_display;
    int own_native_display;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext handle;
    EGLConfig config;
    const char *extensions;
    EGLBoolean (*PresentationTimeANDROID)(EGLDisplay dpy, EGLSurface sur, khronos_stime_nanoseconds_t time);
    EGLDisplay (*GetPlatformDisplay)(EGLenum platform, void *native_display, const EGLint *attrib_list);
    EGLAPIENTRY EGLImageKHR (*CreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
    EGLAPIENTRY EGLBoolean (*DestroyImageKHR)(EGLDisplay, EGLImageKHR);
    EGLAPIENTRY void (*EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
    struct fbo fbo;
    struct texture fbo_color;
    struct texture fbo_depth;
};

EGLImageKHR ngli_eglCreateImageKHR(struct glcontext *gl, EGLConfig context, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
    struct egl_priv *egl = gl->priv_data;
    return egl->CreateImageKHR(egl->display, context, target, buffer, attrib_list);
}

EGLBoolean ngli_eglDestroyImageKHR(struct glcontext *gl, EGLImageKHR image)
{
    struct egl_priv *egl = gl->priv_data;
    return egl->DestroyImageKHR(egl->display, image);
}

static int egl_probe_extensions(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

#if defined(TARGET_ANDROID)
    if (ngli_glcontext_check_extension("EGL_ANDROID_presentation_time", egl->extensions)) {
        egl->PresentationTimeANDROID = (void *)eglGetProcAddress("eglPresentationTimeANDROID");
        if (!egl->PresentationTimeANDROID) {
            LOG(ERROR, "could not retrieve eglPresentationTimeANDROID()");
            return -1;
        }
    }
#endif

    if (ngli_glcontext_check_extension("EGL_KHR_image_base", egl->extensions)) {
        egl->CreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
        egl->DestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
        if (!egl->CreateImageKHR || !egl->DestroyImageKHR) {
            LOG(ERROR, "could not retrieve egl{Create,Destroy}ImageKHR()");
            return -1;
        }
        ctx->features |= NGLI_FEATURE_EGL_IMAGE_BASE_KHR;
    }

    if (ngli_glcontext_check_extension("EGL_EXT_image_dma_buf_import", egl->extensions)) {
        ctx->features |= NGLI_FEATURE_EGL_EXT_IMAGE_DMA_BUF_IMPORT;
    }

    return 0;
}

#if defined(TARGET_LINUX)
static int egl_probe_platform_x11_ext(struct egl_priv *egl)
{
    const char *client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!client_extensions) {
        LOG(ERROR, "could not retrieve EGL client extensions");
        return -1;
    }

    if (ngli_glcontext_check_extension("EGL_KHR_platform_x11", client_extensions) ||
        ngli_glcontext_check_extension("EGL_EXT_platform_x11", client_extensions)) {
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

static int egl_set_native_display(struct egl_priv *egl, uintptr_t native_display)
{
    if (native_display) {
        egl->native_display = (EGLNativeDisplayType)native_display;
        return 0;
    }
#if defined(TARGET_LINUX)
    egl->native_display = XOpenDisplay(NULL);
    if (!egl->native_display) {
        LOG(ERROR, "could not retrieve X11 display");
        return -1;
    }
    egl->own_native_display = 1;
#else
    egl->native_display = EGL_DEFAULT_DISPLAY;
#endif
    return 0;
}

static EGLDisplay egl_get_display(struct egl_priv *egl, EGLNativeDisplayType native_display)
{
#if defined(TARGET_ANDROID)
    return eglGetDisplay(native_display);
#elif defined(TARGET_LINUX)
    /* XXX: only X11 is supported for now */
    int ret = egl_probe_platform_x11_ext(egl);
    if (ret <= 0)
        return EGL_NO_DISPLAY;
    return egl->GetPlatformDisplay(EGL_PLATFORM_X11, native_display, NULL);
#else
    return EGL_NO_DISPLAY;
#endif
}

static int egl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct egl_priv *egl = ctx->priv_data;

    int ret = egl_set_native_display(egl, display);
    if (ret < 0) {
        LOG(ERROR, "could not set native display");
        return -1;
    }

    egl->display = egl_get_display(egl, egl->native_display);
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

    egl->extensions = eglQueryString(egl->display, EGL_EXTENSIONS);
    if (!egl->extensions) {
        LOG(ERROR, "could not retrieve EGL extensions");
        return -1;
    }

    if (egl_major < 1 || egl_minor < 4) {
        LOG(ERROR, "unsupported EGL version %d.%d, only 1.4+ is supported", egl_major, egl_minor);
        return -1;
    }

    int api = ctx->backend == NGL_BACKEND_OPENGL ? EGL_OPENGL_API : EGL_OPENGL_ES_API;
    if (!eglBindAPI(api)) {
        LOG(ERROR, "could not bind OpenGL%s API", ctx->backend == NGL_BACKEND_OPENGL ? "" : "ES");
        return -1;
    }

    const EGLint type = ctx->backend == NGL_BACKEND_OPENGL ? EGL_OPENGL_BIT : EGL_OPENGL_ES2_BIT;
    const EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, type,
        EGL_SURFACE_TYPE, ctx->offscreen ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, ctx->offscreen ? 0 : (ctx->samples > 0),
        EGL_SAMPLES, ctx->offscreen ? 0 : ctx->samples,
        EGL_NONE
    };

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

    if (ctx->offscreen) {
        const EGLint attribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };

        egl->surface = eglCreatePbufferSurface(egl->display, config, attribs);
        if (!egl->surface) {
            LOG(ERROR, "could not create EGL window surface: 0x%x", eglGetError());
            return -1;
        }
    } else {
        EGLNativeWindowType native_window = (EGLNativeWindowType)window;
        if (!native_window) {
            LOG(ERROR, "could not retrieve EGL native window");
            return -1;
        }
        egl->surface = eglCreateWindowSurface(egl->display, config, native_window, NULL);
        if (!egl->surface) {
            LOG(ERROR, "could not create EGL window surface: 0x%x", eglGetError());
            return -1;
        }
    }

    ret = egl_probe_extensions(ctx);
    if (ret < 0)
        return ret;

    return 0;
}

static int egl_init_framebuffer(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

    if (!ctx->offscreen)
        return 0;

    if (!(ctx->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) && ctx->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, multisample anti-aliasing will be disabled");
        ctx->samples = 0;
    }

    struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    attachment_params.width = ctx->width;
    attachment_params.height = ctx->height;
    attachment_params.samples = ctx->samples;
    attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
    int ret = ngli_texture_init(&egl->fbo_color, ctx, &attachment_params);
    if (ret < 0)
        return ret;

    attachment_params.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    ret = ngli_texture_init(&egl->fbo_depth, ctx, &attachment_params);
    if (ret < 0)
        return ret;

    const struct texture *attachments[] = {&egl->fbo_color, &egl->fbo_depth};
    struct fbo_params fbo_params = {
        .width = ctx->width,
        .height = ctx->height,
        .nb_attachments = NGLI_ARRAY_NB(attachments),
        .attachments = attachments,
    };
    ret = ngli_fbo_init(&egl->fbo, ctx, &fbo_params);
    if (ret < 0)
        return ret;

    ret = ngli_fbo_bind(&egl->fbo);
    if (ret < 0)
        return ret;

    ngli_glViewport(ctx, 0, 0, ctx->width, ctx->height);

    return 0;
}

static void egl_uninit(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

    ngli_fbo_reset(&egl->fbo);
    ngli_texture_reset(&egl->fbo_color);
    ngli_texture_reset(&egl->fbo_depth);

    ngli_glcontext_make_current(ctx, 0);

    if (egl->surface)
        eglDestroySurface(egl->display, egl->surface);

    if (egl->handle)
        eglDestroyContext(egl->display, egl->handle);

    if (egl->display)
        eglTerminate(egl->display);

#if defined(TARGET_LINUX)
    if (egl->own_native_display) {
        XCloseDisplay(egl->native_display);
    }
#endif
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

    if (!ctx->offscreen)
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

static uintptr_t get_display(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;
    return (uintptr_t)egl->native_display;
}

static uintptr_t egl_get_handle(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;
    return (uintptr_t)egl->handle;
}

const struct glcontext_class ngli_glcontext_egl_class = {
    .init = egl_init,
    .init_framebuffer = egl_init_framebuffer,
    .uninit = egl_uninit,
    .make_current = egl_make_current,
    .swap_buffers = egl_swap_buffers,
    .set_swap_interval = egl_set_swap_interval,
    .set_surface_pts = egl_set_surface_pts,
    .get_proc_address = egl_get_proc_address,
    .get_handle = egl_get_handle,
    .get_display = get_display,
    .priv_size = sizeof(struct egl_priv),
};
