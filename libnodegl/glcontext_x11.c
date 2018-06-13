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

struct glcontext_x11 {
    Display *display;
    int own_display;
    Window window;
    int own_window;
    GLXContext handle;
    int own_handle;
    GLXFBConfig *fbconfigs;
    int nb_fbconfigs;
    int swap_interval_ext;
    int swap_interval_mesa;
    int swap_interval_sgi;
    void (*glXSwapIntervalEXT)(Display*, GLXDrawable, int);
    int (*glXSwapIntervalMESA)(int);
    int (*glXSwapIntervalSGI)(int);
};

typedef GLXContext (*glXCreateContextAttribsFunc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int glcontext_x11_init(struct glcontext *glcontext, uintptr_t display, uintptr_t window, uintptr_t handle)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    if (glcontext->wrapped) {
        glcontext_x11->display = display ? (Display *)display : glXGetCurrentDisplay();
        glcontext_x11->window  = window  ? (Window)window     : glXGetCurrentDrawable();
        glcontext_x11->handle  = handle  ? (GLXContext)handle : glXGetCurrentContext();
        if (!glcontext_x11->display || !glcontext_x11->window || !glcontext_x11->handle) {
            LOG(ERROR,
                "could not retrieve GLX display (%p), window (0x%lx) and context (%p)",
                glcontext_x11->display,
                glcontext_x11->window,
                glcontext_x11->handle);
            return -1;
        }
    } else {
        if (display)
            glcontext_x11->display = (Display *)display;
        if (!glcontext_x11->display) {
            glcontext_x11->own_display = 1;
            glcontext_x11->display = XOpenDisplay(NULL);
            if (!glcontext_x11->display) {
                LOG(ERROR, "could not retrieve GLX display");
                return -1;
            }
        }

        if (!glcontext->offscreen) {
            if (window) {
                glcontext_x11->window  = (Window)window;
                if (!glcontext_x11->window) {
                    LOG(ERROR, "could not retrieve GLX window");
                    return -1;
                }
            }
        }
    }

    int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        GLX_SAMPLE_BUFFERS, 0,
        GLX_SAMPLES, 0,
        None
    };

    if (glcontext->samples > 0) {
        attribs[NGLI_ARRAY_NB(attribs) - 4] = 1;
        attribs[NGLI_ARRAY_NB(attribs) - 2] = glcontext->samples;
    }

    glcontext_x11->fbconfigs = glXChooseFBConfig(glcontext_x11->display,
                                                 DefaultScreen(glcontext_x11->display),
                                                 attribs,
                                                 &glcontext_x11->nb_fbconfigs);
    if (!glcontext_x11->fbconfigs) {
        LOG(ERROR, "could not choose a valid framebuffer configuration");
        return -1;
    }

    return 0;
}

static void glcontext_x11_uninit(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    if (glcontext_x11->fbconfigs)
        XFree(glcontext_x11->fbconfigs);

    if (glcontext_x11->own_handle)
        glXDestroyContext(glcontext_x11->display, glcontext_x11->handle);

    if (glcontext_x11->own_window)
        glXDestroyPbuffer(glcontext_x11->display, glcontext_x11->window);

    if (glcontext_x11->own_display)
        XCloseDisplay(glcontext_x11->display);
}

static int glcontext_x11_create(struct glcontext *glcontext, uintptr_t other)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    glXCreateContextAttribsFunc glXCreateContextAttribs =
        (glXCreateContextAttribsFunc)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
    if (!glXCreateContextAttribs) {
        LOG(ERROR, "could not retrieve glXCreateContextAttribsARB()");
        return -1;
    }

    Display *display = glcontext_x11->display;
    int screen = DefaultScreen(display);
    GLXFBConfig *fbconfigs = glcontext_x11->fbconfigs;

    const char *glx_extensions = glXQueryExtensionsString(display, screen);
    if (!ngli_glcontext_check_extension("GLX_ARB_create_context", glx_extensions)) {
        LOG(ERROR, "context does not support GLX_ARB_create_context extension");
        return -1;
    }

    GLXContext shared_context = other ? (GLXContext)other : NULL;

    glcontext_x11->own_handle = 1;
    if (glcontext->api == NGL_GLAPI_OPENGLES) {
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

        glcontext_x11->handle = glXCreateContextAttribs(display,
                                                        fbconfigs[0],
                                                        shared_context,
                                                        1,
                                                        attribs);
    } else if (glcontext->api == NGL_GLAPI_OPENGL) {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };

        glcontext_x11->handle = glXCreateContextAttribs(display,
                                                        fbconfigs[0],
                                                        shared_context,
                                                        1,
                                                        attribs);
    }

    if (!glcontext_x11->handle) {
        LOG(ERROR, "could not create GLX context");
        return -1;
    }

    if (glcontext->offscreen) {
        int attribs[] = {
            GLX_PBUFFER_WIDTH, glcontext->width,
            GLX_PBUFFER_HEIGHT, glcontext->height,
            None
        };

        glcontext_x11->own_window = 1;
        glcontext_x11->window = glXCreatePbuffer(display, fbconfigs[0], attribs);
        if (!glcontext_x11->window) {
            LOG(ERROR, "could not create offscreen pixel buffer");
            return -1;
        }
    }

    if (ngli_glcontext_check_extension("GLX_EXT_swap_control", glx_extensions)) {
        glcontext_x11->glXSwapIntervalEXT = (void *)glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");
        if (glcontext_x11->glXSwapIntervalEXT)
            glcontext_x11->swap_interval_ext = 1;
    } else if (ngli_glcontext_check_extension("GLX_MESA_swap_control", glx_extensions)) {
        glcontext_x11->glXSwapIntervalMESA = (void *)glXGetProcAddress((const GLubyte *)"glXSwapIntervalMESA");
        if (glcontext_x11->glXSwapIntervalMESA)
            glcontext_x11->swap_interval_mesa = 1;
    } else if (ngli_glcontext_check_extension("GLX_SGI_swap_control", glx_extensions)) {
        glcontext_x11->glXSwapIntervalSGI = (void *)glXGetProcAddress((const GLubyte *)"glXSwapIntervalSGI");
        if (glcontext_x11->glXSwapIntervalSGI)
            glcontext_x11->swap_interval_sgi = 1;
    } else {
        LOG(WARNING, "context does not support any swap interval extension");
    }

    return 0;
}

static int glcontext_x11_make_current(struct glcontext *glcontext, int current)
{
    int ret;
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    if (current) {
        ret = glXMakeCurrent(glcontext_x11->display, glcontext_x11->window, glcontext_x11->handle);
    } else {
        ret = glXMakeCurrent(glcontext_x11->display, None, NULL);
    }

    return ret - 1;
}

static void glcontext_x11_swap_buffers(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;
    glXSwapBuffers(glcontext_x11->display, glcontext_x11->window);
}

static int glcontext_x11_set_swap_interval(struct glcontext *glcontext, int interval)
{
    struct glcontext_x11 *x11 = glcontext->priv_data;

    if (x11->swap_interval_ext) {
        x11->glXSwapIntervalEXT(x11->display, x11->window, interval);
    } else if (x11->swap_interval_mesa) {
        x11->glXSwapIntervalMESA(interval);
    } else if (x11->swap_interval_sgi) {
        x11->glXSwapIntervalSGI(interval);
    } else {
        LOG(WARNING, "context does not support swap interval operation");
    }

    return 0;
}

static void *glcontext_x11_get_proc_address(struct glcontext *glcontext, const char *name)
{
    return glXGetProcAddress((const GLubyte *)name);
}

const struct glcontext_class ngli_glcontext_x11_class = {
    .init = glcontext_x11_init,
    .uninit = glcontext_x11_uninit,
    .create = glcontext_x11_create,
    .make_current = glcontext_x11_make_current,
    .swap_buffers = glcontext_x11_swap_buffers,
    .set_swap_interval = glcontext_x11_set_swap_interval,
    .get_proc_address = glcontext_x11_get_proc_address,
    .priv_size = sizeof(struct glcontext_x11),
};
