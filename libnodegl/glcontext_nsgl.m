/*
 * Copyright 2018 GoPro Inc.
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
#include <stdlib.h>
#include <string.h>
#include <OpenGL/OpenGL.h>
#include <CoreFoundation/CFBundle.h>
#include <Cocoa/Cocoa.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"

struct glcontext_nsgl {
    NSOpenGLContext *handle;
    NSView *view;
    CFBundleRef framework;

    GLuint framebuffer;
    GLuint colorbuffer;
    GLuint depthbuffer;
};

static int glcontext_nsgl_init(struct glcontext *glcontext, void *display, void *window, void *handle)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;

    glcontext_nsgl->view = window ? *(NSView **)window : nil;
    glcontext_nsgl->handle = handle ? *(NSOpenGLContext **)handle : [NSOpenGLContext currentContext];

    if (glcontext->wrapped && !glcontext_nsgl->handle)
        return -1;

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
    if (!framework)
        return -1;

    glcontext_nsgl->framework = (CFBundleRef)CFRetain(framework);
    if (!glcontext_nsgl->framework)
        return -1;

    return 0;
}

static int glcontext_nsgl_create(struct glcontext *glcontext, void *other)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;

    NSOpenGLPixelFormatAttribute pixelAttrs[] = {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAClosestPolicy,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        NSOpenGLPFASampleBuffers, 0,
        NSOpenGLPFASamples, 0,
        0,
    };

    if (!glcontext->offscreen && glcontext->samples > 0) {
        pixelAttrs[NGLI_ARRAY_NB(pixelAttrs) - 4] = 1;
        pixelAttrs[NGLI_ARRAY_NB(pixelAttrs) - 2] = glcontext->samples;
    }

    NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttrs];
    if (!pixelFormat)
        return -1;

    NSOpenGLContext *shared_context = other ? *(NSOpenGLContext **)other : NULL;
    glcontext_nsgl->handle = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:shared_context];
    if (!glcontext_nsgl->handle)
        return -1;

    if (glcontext->offscreen) {
        ngli_glcontext_make_current(glcontext, 1);

        glGenFramebuffers(1, &glcontext_nsgl->framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, glcontext_nsgl->framebuffer);

        if (glcontext->samples > 0) {
            glGenRenderbuffers(1, &glcontext_nsgl->colorbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, glcontext_nsgl->colorbuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, glcontext->samples, GL_RGBA8, glcontext->width, glcontext->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glcontext_nsgl->colorbuffer);

            glGenRenderbuffers(1, &glcontext_nsgl->depthbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, glcontext_nsgl->depthbuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, glcontext->samples, GL_DEPTH24_STENCIL8, glcontext->width, glcontext->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glcontext_nsgl->depthbuffer);
        } else {
            glGenRenderbuffers(1, &glcontext_nsgl->colorbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, glcontext_nsgl->colorbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, glcontext->width, glcontext->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glcontext_nsgl->colorbuffer);

            glGenRenderbuffers(1, &glcontext_nsgl->depthbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, glcontext_nsgl->depthbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, glcontext->width, glcontext->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glcontext_nsgl->depthbuffer);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER) ;
        if(status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }

        glViewport(0, 0, glcontext->width, glcontext->height);
    } else {
        [glcontext_nsgl->handle setView:glcontext_nsgl->view];
    }

    return 0;
}

static int glcontext_nsgl_make_current(struct glcontext *glcontext, int current)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;

    if (current) {
        [glcontext_nsgl->handle makeCurrentContext];
        if (glcontext->offscreen) {
            glBindFramebuffer(GL_FRAMEBUFFER, glcontext_nsgl->framebuffer);
        }
    } else {
        [NSOpenGLContext clearCurrentContext];
    }

    return 0;
}

static void glcontext_nsgl_swap_buffers(struct glcontext *glcontext)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;
    [glcontext_nsgl->handle flushBuffer];
}

static void *glcontext_nsgl_get_handle(struct glcontext *glcontext)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;
    return &glcontext_nsgl->handle;
}

static void *glcontext_nsgl_get_proc_address(struct glcontext *glcontext, const char *name)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(glcontext_nsgl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

static void glcontext_nsgl_uninit(struct glcontext *glcontext)
{
    struct glcontext_nsgl *glcontext_nsgl = glcontext->priv_data;

    if (glcontext_nsgl->framebuffer > 0)
        glDeleteFramebuffers(1, &glcontext_nsgl->framebuffer);

    if (glcontext_nsgl->colorbuffer > 0)
        glDeleteRenderbuffers(1, &glcontext_nsgl->colorbuffer);

    if (glcontext_nsgl->depthbuffer > 0)
        glDeleteRenderbuffers(1, &glcontext_nsgl->depthbuffer);

    if (glcontext_nsgl->framework)
        CFRelease(glcontext_nsgl->framework);

    if (!glcontext->wrapped)
        CFRelease(glcontext_nsgl->handle);
}

const struct glcontext_class ngli_glcontext_nsgl_class = {
    .init = glcontext_nsgl_init,
    .create = glcontext_nsgl_create,
    .uninit = glcontext_nsgl_uninit,
    .make_current = glcontext_nsgl_make_current,
    .swap_buffers = glcontext_nsgl_swap_buffers,
    .get_handle = glcontext_nsgl_get_handle,
    .get_proc_address = glcontext_nsgl_get_proc_address,
    .priv_size = sizeof(struct glcontext_nsgl),
};
