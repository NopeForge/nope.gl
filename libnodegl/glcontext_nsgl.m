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

struct nsgl_priv {
    NSOpenGLContext *handle;
    NSView *view;
    CFBundleRef framework;

    GLuint framebuffer;
    GLuint colorbuffer;
    GLuint depthbuffer;
};

static int nsgl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
    if (!framework) {
        LOG(ERROR, "could not retrieve OpenGL framework");
        return -1;
    }

    nsgl->framework = (CFBundleRef)CFRetain(framework);
    if (!nsgl->framework) {
        LOG(ERROR, "could not retain OpenGL framework object");
        return -1;
    }

    const NSOpenGLPixelFormatAttribute pixelAttrs[] = {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAClosestPolicy,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        NSOpenGLPFASampleBuffers, ctx->offscreen ? 0 : (ctx->samples > 0),
        NSOpenGLPFASamples, ctx->offscreen ? 0 : ctx->samples,
        0,
    };

    NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttrs];
    if (!pixelFormat) {
        LOG(ERROR, "could not allocate pixel format");
        return -1;
    }

    NSOpenGLContext *shared_context = other ? (NSOpenGLContext *)other : NULL;
    nsgl->handle = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:shared_context];
    if (!nsgl->handle) {
        LOG(ERROR, "could not create NSGL context");
        return -1;
    }

    if (ctx->offscreen) {
        ngli_glcontext_make_current(ctx, 1);

        glGenFramebuffers(1, &nsgl->framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, nsgl->framebuffer);

        if (ctx->samples > 0) {
            glGenRenderbuffers(1, &nsgl->colorbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, nsgl->colorbuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, ctx->samples, GL_RGBA8, ctx->width, ctx->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, nsgl->colorbuffer);

            glGenRenderbuffers(1, &nsgl->depthbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, nsgl->depthbuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, ctx->samples, GL_DEPTH24_STENCIL8, ctx->width, ctx->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, nsgl->depthbuffer);
        } else {
            glGenRenderbuffers(1, &nsgl->colorbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, nsgl->colorbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, ctx->width, ctx->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, nsgl->colorbuffer);

            glGenRenderbuffers(1, &nsgl->depthbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, nsgl->depthbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, ctx->width, ctx->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, nsgl->depthbuffer);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER) ;
        if(status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }

        glViewport(0, 0, ctx->width, ctx->height);
    } else {
        nsgl->view = (NSView *)window;
        if (!nsgl->view) {
            LOG(ERROR, "could not retrieve NS view");
            return -1;
        }
        [nsgl->handle setView:nsgl->view];
    }

    return 0;
}

static int nsgl_resize(struct glcontext *ctx, int width, int height)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    [nsgl->handle update];

    NSRect bounds = [nsgl->view bounds];
    ctx->width = bounds.size.width;
    ctx->height = bounds.size.height;

    return 0;
}

static int nsgl_make_current(struct glcontext *ctx, int current)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    if (current) {
        [nsgl->handle makeCurrentContext];
        if (ctx->offscreen) {
            glBindFramebuffer(GL_FRAMEBUFFER, nsgl->framebuffer);
        }
    } else {
        [NSOpenGLContext clearCurrentContext];
    }

    return 0;
}

static void nsgl_swap_buffers(struct glcontext *ctx)
{
    struct nsgl_priv *nsgl = ctx->priv_data;
    [nsgl->handle flushBuffer];
}

static int nsgl_set_swap_interval(struct glcontext *ctx, int interval)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    [nsgl->handle setValues:&interval forParameter:NSOpenGLCPSwapInterval];

    return 0;
}

static void *nsgl_get_proc_address(struct glcontext *ctx, const char *name)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(nsgl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

static void nsgl_uninit(struct glcontext *ctx)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    if (nsgl->framebuffer > 0)
        glDeleteFramebuffers(1, &nsgl->framebuffer);

    if (nsgl->colorbuffer > 0)
        glDeleteRenderbuffers(1, &nsgl->colorbuffer);

    if (nsgl->depthbuffer > 0)
        glDeleteRenderbuffers(1, &nsgl->depthbuffer);

    if (nsgl->framework)
        CFRelease(nsgl->framework);

    if (nsgl->handle)
        CFRelease(nsgl->handle);
}

const struct glcontext_class ngli_glcontext_nsgl_class = {
    .init = nsgl_init,
    .uninit = nsgl_uninit,
    .resize = nsgl_resize,
    .make_current = nsgl_make_current,
    .swap_buffers = nsgl_swap_buffers,
    .set_swap_interval = nsgl_set_swap_interval,
    .get_proc_address = nsgl_get_proc_address,
    .priv_size = sizeof(struct nsgl_priv),
};
