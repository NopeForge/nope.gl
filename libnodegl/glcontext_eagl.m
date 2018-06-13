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

#include <CoreVideo/CoreVideo.h>
#include <UIKit/UIKit.h>
#include <OpenGLES/EAGL.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"

struct glcontext_eagl {
    EAGLContext *handle;
    UIView *view;
    CAEAGLLayer *layer;
    CFBundleRef framework;
    CVOpenGLESTextureCacheRef texture_cache;
    int width;
    int height;
    GLuint framebuffer;
    GLuint framebuffer_ms;
    GLuint colorbuffer;
    GLuint colorbuffer_ms;
    GLuint depthbuffer;
    void (*RenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
    void (*BlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
};

static int glcontext_eagl_setup_layer(struct glcontext *glcontext)
{
    if (![NSThread isMainThread]) {
        __block int ret;

        dispatch_sync(dispatch_get_main_queue(), ^{
            ret = glcontext_eagl_setup_layer(glcontext);
        });

        return ret;
    }

    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    glcontext_eagl->layer = (CAEAGLLayer *)[glcontext_eagl->view layer];
    if (!glcontext_eagl->layer) {
        LOG(ERROR, "could not retrieve EAGL layer");
        return -1;
    }

    NSDictionary *properties = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
        nil
    ];

    [glcontext_eagl->layer setOpaque:YES];
    [glcontext_eagl->layer setDrawableProperties:properties];

    return 0;
}

static int glcontext_eagl_init(struct glcontext *glcontext, uintptr_t display, uintptr_t window, uintptr_t handle)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengles"));
    if (!framework) {
        LOG(ERROR, "could not retrieve OpenGLES framework");
        return -1;
    }

    glcontext_eagl->framework = (CFBundleRef)CFRetain(framework);
    if (!glcontext_eagl->framework) {
        LOG(ERROR, "could not retain OpenGL framework object");
        return -1;
    }

    if (glcontext->wrapped) {
        glcontext_eagl->handle = handle ? (EAGLContext *)handle : [EAGLContext currentContext];
        if (!glcontext_eagl->handle) {
            LOG(ERROR, "could not retrieve EAGL context");
            return -1;
        }

        CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                                    NULL,
                                                    glcontext_eagl->handle,
                                                    NULL,
                                                    &glcontext_eagl->texture_cache);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture cache: 0x%x", err);
            return -1;
        }
    } else  {
        if (!glcontext->offscreen) {
            if (window)
                glcontext_eagl->view = (UIView *)window;
            if (!glcontext_eagl->view) {
                LOG(ERROR, "could not retrieve UI view");
                return -1;
            }

            int ret = glcontext_eagl_setup_layer(glcontext);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static void glcontext_eagl_uninit(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    if (glcontext_eagl->framebuffer > 0)
        glDeleteFramebuffers(1, &glcontext_eagl->framebuffer);

    if (glcontext_eagl->colorbuffer > 0)
        glDeleteRenderbuffers(1, &glcontext_eagl->colorbuffer);

    if (glcontext_eagl->depthbuffer > 0)
        glDeleteRenderbuffers(1, &glcontext_eagl->depthbuffer);

    if (glcontext_eagl->framework)
        CFRelease(glcontext_eagl->framework);

    if (glcontext_eagl->texture_cache)
        CFRelease(glcontext_eagl->texture_cache);

    if (!glcontext->wrapped) {
        if (glcontext_eagl->handle)
            CFRelease(glcontext_eagl->handle);
    }
}

static int glcontext_eagl_safe_create(struct glcontext *glcontext, uintptr_t other)
{
    if (![NSThread isMainThread]) {
        __block int ret;

        dispatch_sync(dispatch_get_main_queue(), ^{
            ret = glcontext_eagl_safe_create(glcontext, other);
        });

        return ret;
    }

    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    glcontext_eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!glcontext_eagl->handle) {
        glcontext_eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        if (!glcontext_eagl->handle) {
            LOG(ERROR, "could not create EAGL context");
            return -1;
        }
        if (glcontext->samples > 0) {
            LOG(WARNING, "multisample anti-aliasing is not supported with OpenGLES 2.0 context");
            glcontext->samples = 0;
        }
    }

    CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                                NULL,
                                                glcontext_eagl->handle,
                                                NULL,
                                                &glcontext_eagl->texture_cache);
    if (err != noErr) {
        LOG(ERROR, "could not create CoreVideo texture cache: 0x%x", err);
        return -1;
    }

    ngli_glcontext_make_current(glcontext, 1);

    if (glcontext->samples > 0) {
        glcontext_eagl->RenderbufferStorageMultisample = ngli_glcontext_get_proc_address(glcontext, "glRenderbufferStorageMultisample");
        glcontext_eagl->BlitFramebuffer = ngli_glcontext_get_proc_address(glcontext, "glBlitFramebuffer");

        if (!glcontext_eagl->RenderbufferStorageMultisample ||
            !glcontext_eagl->BlitFramebuffer) {
            LOG(ERROR, "could not retrieve glRenderbufferStorage() and glBlitFramebuffer()");
            return -1;
        }

        glGenFramebuffers(1, &glcontext_eagl->framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, glcontext_eagl->framebuffer);

        glGenRenderbuffers(1, &glcontext_eagl->colorbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, glcontext_eagl->colorbuffer);
        if (!glcontext->offscreen)
            [glcontext_eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:glcontext_eagl->layer];
        else
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, glcontext->width, glcontext->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glcontext_eagl->colorbuffer);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &glcontext_eagl->width);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &glcontext_eagl->height);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }

        glGenFramebuffers(1, &glcontext_eagl->framebuffer_ms);
        glBindFramebuffer(GL_FRAMEBUFFER, glcontext_eagl->framebuffer_ms);

        glGenRenderbuffers(1, &glcontext_eagl->colorbuffer_ms);
        glBindRenderbuffer(GL_RENDERBUFFER, glcontext_eagl->colorbuffer_ms);
        glcontext_eagl->RenderbufferStorageMultisample(GL_RENDERBUFFER, glcontext->samples, GL_RGBA8, glcontext_eagl->width, glcontext_eagl->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glcontext_eagl->colorbuffer_ms);

        glGenRenderbuffers(1, &glcontext_eagl->depthbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, glcontext_eagl->depthbuffer);
        glcontext_eagl->RenderbufferStorageMultisample(GL_RENDERBUFFER, glcontext->samples, GL_DEPTH24_STENCIL8_OES, glcontext_eagl->width, glcontext_eagl->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glcontext_eagl->depthbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glcontext_eagl->depthbuffer);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }
    } else {
        glGenFramebuffers(1, &glcontext_eagl->framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, glcontext_eagl->framebuffer);

        glGenRenderbuffers(1, &glcontext_eagl->colorbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, glcontext_eagl->colorbuffer);
        if (!glcontext->offscreen)
            [glcontext_eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:glcontext_eagl->layer];
        else
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, glcontext->width, glcontext->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glcontext_eagl->colorbuffer);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &glcontext_eagl->width);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &glcontext_eagl->height);

        glGenRenderbuffers(1, &glcontext_eagl->depthbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, glcontext_eagl->depthbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, glcontext_eagl->width, glcontext_eagl->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glcontext_eagl->depthbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glcontext_eagl->depthbuffer);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }
    }

    ngli_glcontext_make_current(glcontext, 0);

    return 0;
}

static int glcontext_eagl_create(struct glcontext *glcontext, uintptr_t other)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    int ret = glcontext_eagl_safe_create(glcontext, other);
    if (ret < 0)
        return ret;

    ngli_glcontext_make_current(glcontext, 1);

    if (glcontext->samples > 0)
        glBindFramebuffer(GL_FRAMEBUFFER, glcontext_eagl->framebuffer_ms);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, glcontext_eagl->framebuffer);
    glViewport(0, 0, glcontext_eagl->width, glcontext_eagl->height);

    return 0;
}

static int glcontext_eagl_make_current(struct glcontext *glcontext, int current)
{
    int ret;
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    if (current) {
        ret = [EAGLContext setCurrentContext:glcontext_eagl->handle];
    } else {
        ret = [EAGLContext setCurrentContext:nil];
    }

    return ret;
}

static void glcontext_eagl_swap_buffers(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    if (glcontext->samples > 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, glcontext_eagl->framebuffer_ms);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glcontext_eagl->framebuffer);
        glcontext_eagl->BlitFramebuffer(0, 0, glcontext_eagl->width, glcontext_eagl->height, 0, 0, glcontext_eagl->width, glcontext_eagl->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glcontext_eagl->framebuffer_ms);
    }

    if (!glcontext->offscreen) {
        glBindRenderbuffer(GL_RENDERBUFFER, glcontext_eagl->colorbuffer);
        [glcontext_eagl->handle presentRenderbuffer: GL_RENDERBUFFER];
    }
}

static void *glcontext_eagl_get_texture_cache(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;
    return &glcontext_eagl->texture_cache;
}

static void *glcontext_eagl_get_proc_address(struct glcontext *glcontext, const char *name)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(glcontext_eagl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

const struct glcontext_class ngli_glcontext_eagl_class = {
    .init = glcontext_eagl_init,
    .uninit = glcontext_eagl_uninit,
    .create = glcontext_eagl_create,
    .make_current = glcontext_eagl_make_current,
    .swap_buffers = glcontext_eagl_swap_buffers,
    .get_texture_cache = glcontext_eagl_get_texture_cache,
    .get_proc_address = glcontext_eagl_get_proc_address,
    .priv_size = sizeof(struct glcontext_eagl),
};
