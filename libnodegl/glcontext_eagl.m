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

struct eagl_priv {
    EAGLContext *handle;
    UIView *view;
    CAEAGLLayer *layer;
    CFBundleRef framework;
    CVPixelBufferRef pixel_buffer;
    CVOpenGLESTextureRef texture;
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

static int eagl_setup_layer(struct glcontext *ctx)
{
    if (![NSThread isMainThread]) {
        __block int ret;

        dispatch_sync(dispatch_get_main_queue(), ^{
            ret = eagl_setup_layer(ctx);
        });

        return ret;
    }

    struct eagl_priv *eagl = ctx->priv_data;

    eagl->layer = (CAEAGLLayer *)[eagl->view layer];
    if (!eagl->layer) {
        LOG(ERROR, "could not retrieve EAGL layer");
        return -1;
    }

    NSDictionary *properties = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
        nil
    ];

    [eagl->layer setOpaque:YES];
    [eagl->layer setDrawableProperties:properties];

    return 0;
}

static int eagl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t handle)
{
    struct eagl_priv *eagl = ctx->priv_data;

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengles"));
    if (!framework) {
        LOG(ERROR, "could not retrieve OpenGLES framework");
        return -1;
    }

    eagl->framework = (CFBundleRef)CFRetain(framework);
    if (!eagl->framework) {
        LOG(ERROR, "could not retain OpenGL framework object");
        return -1;
    }

    if (ctx->offscreen) {
        if (window) {
            CVPixelBufferRef pixel_buffer = (CVPixelBufferRef)window;
            eagl->pixel_buffer = (CVPixelBufferRef)CFRetain(pixel_buffer);
        }
    } else {
        if (window)
            eagl->view = (UIView *)window;
        if (!eagl->view) {
            LOG(ERROR, "could not retrieve UI view");
            return -1;
        }

        int ret = eagl_setup_layer(ctx);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void eagl_uninit(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    if (eagl->framebuffer > 0)
        glDeleteFramebuffers(1, &eagl->framebuffer);

    if (eagl->colorbuffer > 0)
        glDeleteRenderbuffers(1, &eagl->colorbuffer);

    if (eagl->depthbuffer > 0)
        glDeleteRenderbuffers(1, &eagl->depthbuffer);

    if (eagl->framework)
        CFRelease(eagl->framework);

    if (eagl->pixel_buffer)
        CFRelease(eagl->pixel_buffer);

    if (eagl->texture)
        CFRelease(eagl->texture);

    if (eagl->texture_cache)
        CFRelease(eagl->texture_cache);

    if (eagl->handle)
        CFRelease(eagl->handle);
}

static int eagl_safe_create(struct glcontext *ctx, uintptr_t other)
{
    if (![NSThread isMainThread]) {
        __block int ret;

        dispatch_sync(dispatch_get_main_queue(), ^{
            ret = eagl_safe_create(ctx, other);
        });

        return ret;
    }

    struct eagl_priv *eagl = ctx->priv_data;

    eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!eagl->handle) {
        eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        if (!eagl->handle) {
            LOG(ERROR, "could not create EAGL context");
            return -1;
        }
        if (ctx->samples > 0) {
            LOG(WARNING, "multisample anti-aliasing is not supported with OpenGLES 2.0 context");
            ctx->samples = 0;
        }
    }

    CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                                NULL,
                                                eagl->handle,
                                                NULL,
                                                &eagl->texture_cache);
    if (err != noErr) {
        LOG(ERROR, "could not create CoreVideo texture cache: 0x%x", err);
        return -1;
    }

    ngli_glcontext_make_current(ctx, 1);

    if (ctx->samples > 0) {
        eagl->RenderbufferStorageMultisample = ngli_glcontext_get_proc_address(ctx, "glRenderbufferStorageMultisample");
        eagl->BlitFramebuffer = ngli_glcontext_get_proc_address(ctx, "glBlitFramebuffer");

        if (!eagl->RenderbufferStorageMultisample ||
            !eagl->BlitFramebuffer) {
            LOG(ERROR, "could not retrieve glRenderbufferStorage() and glBlitFramebuffer()");
            return -1;
        }
    }

    glGenFramebuffers(1, &eagl->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, eagl->framebuffer);

    if (eagl->pixel_buffer) {
        eagl->width = CVPixelBufferGetWidth(eagl->pixel_buffer);
        eagl->height = CVPixelBufferGetHeight(eagl->pixel_buffer);
        err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                           eagl->texture_cache,
                                                           eagl->pixel_buffer,
                                                           NULL,
                                                           GL_TEXTURE_2D,
                                                           GL_RGBA,
                                                           eagl->width,
                                                           eagl->height,
                                                           GL_BGRA,
                                                           GL_UNSIGNED_BYTE,
                                                           0,
                                                           &eagl->texture);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from CVPixelBuffer: 0x%x", err);
            return -1;
        }

        GLuint id = CVOpenGLESTextureGetName(eagl->texture);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
    } else {
        glGenRenderbuffers(1, &eagl->colorbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->colorbuffer);
        if (!ctx->offscreen)
            [eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:eagl->layer];
        else
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, ctx->width, ctx->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, eagl->colorbuffer);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &eagl->width);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &eagl->height);
    }

    if (!ctx->samples) {
        glGenRenderbuffers(1, &eagl->depthbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->depthbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, eagl->width, eagl->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer is not complete: 0x%x", status);
        return -1;
    }

    if (ctx->samples > 0) {
        glGenFramebuffers(1, &eagl->framebuffer_ms);
        glBindFramebuffer(GL_FRAMEBUFFER, eagl->framebuffer_ms);

        glGenRenderbuffers(1, &eagl->colorbuffer_ms);
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->colorbuffer_ms);
        eagl->RenderbufferStorageMultisample(GL_RENDERBUFFER, ctx->samples, GL_RGBA8, eagl->width, eagl->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, eagl->colorbuffer_ms);

        glGenRenderbuffers(1, &eagl->depthbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->depthbuffer);
        eagl->RenderbufferStorageMultisample(GL_RENDERBUFFER, ctx->samples, GL_DEPTH24_STENCIL8_OES, eagl->width, eagl->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }
    }

    ngli_glcontext_make_current(ctx, 0);

    return 0;
}

static int eagl_create(struct glcontext *ctx, uintptr_t other)
{
    struct eagl_priv *eagl = ctx->priv_data;

    int ret = eagl_safe_create(ctx, other);
    if (ret < 0)
        return ret;

    ngli_glcontext_make_current(ctx, 1);

    if (ctx->samples > 0)
        glBindFramebuffer(GL_FRAMEBUFFER, eagl->framebuffer_ms);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, eagl->framebuffer);
    glViewport(0, 0, eagl->width, eagl->height);

    return 0;
}

static int eagl_safe_resize(struct glcontext *ctx, int width, int height)
{
    if (![NSThread isMainThread]) {
        __block int ret;

        dispatch_sync(dispatch_get_main_queue(), ^{
            ret = eagl_safe_resize(ctx, width, height);
        });

        return ret;
    }

    struct eagl_priv *eagl = ctx->priv_data;

    ngli_glcontext_make_current(ctx, 1);

    glBindRenderbuffer (GL_RENDERBUFFER, eagl->colorbuffer);
    if (!ctx->offscreen)
        [eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:eagl->layer];
    else
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &eagl->width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &eagl->height);

    if (ctx->samples > 0) {
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->colorbuffer_ms);
        eagl->RenderbufferStorageMultisample(GL_RENDERBUFFER, ctx->samples, GL_RGBA8, eagl->width, eagl->height);

        glBindRenderbuffer(GL_RENDERBUFFER, eagl->depthbuffer);
        eagl->RenderbufferStorageMultisample(GL_RENDERBUFFER, ctx->samples, GL_DEPTH24_STENCIL8_OES, eagl->width, eagl->height);
    } else {
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->depthbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, eagl->width, eagl->height);
    }

    ngli_glcontext_make_current(ctx, 0);

    ctx->width = eagl->width;
    ctx->height = eagl->height;

    return 0;
}

static int eagl_resize(struct glcontext *ctx, int width, int height)
{
    struct eagl_priv *eagl = ctx->priv_data;

    int ret = eagl_safe_resize(ctx, width, height);
    if (ret < 0)
        return ret;

    ngli_glcontext_make_current(ctx, 1);

    if (ctx->samples > 0)
        glBindFramebuffer(GL_FRAMEBUFFER, eagl->framebuffer_ms);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, eagl->framebuffer);
    glViewport(0, 0, eagl->width, eagl->height);

    return 0;
}

static int eagl_make_current(struct glcontext *ctx, int current)
{
    int ret;
    struct eagl_priv *eagl = ctx->priv_data;

    if (current) {
        ret = [EAGLContext setCurrentContext:eagl->handle];
    } else {
        ret = [EAGLContext setCurrentContext:nil];
    }

    return ret;
}

static void eagl_swap_buffers(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    if (ctx->samples > 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, eagl->framebuffer_ms);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eagl->framebuffer);
        eagl->BlitFramebuffer(0, 0, eagl->width, eagl->height, 0, 0, eagl->width, eagl->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eagl->framebuffer_ms);
    }

    if (ctx->offscreen) {
        if (eagl->pixel_buffer) {
            glFinish();
        }
    } else {
        glBindRenderbuffer(GL_RENDERBUFFER, eagl->colorbuffer);
        [eagl->handle presentRenderbuffer: GL_RENDERBUFFER];
    }
}

static void *eagl_get_texture_cache(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;
    return &eagl->texture_cache;
}

static void *eagl_get_proc_address(struct glcontext *ctx, const char *name)
{
    struct eagl_priv *eagl = ctx->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(eagl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

const struct glcontext_class ngli_glcontext_eagl_class = {
    .init = eagl_init,
    .uninit = eagl_uninit,
    .create = eagl_create,
    .resize = eagl_resize,
    .make_current = eagl_make_current,
    .swap_buffers = eagl_swap_buffers,
    .get_texture_cache = eagl_get_texture_cache,
    .get_proc_address = eagl_get_proc_address,
    .priv_size = sizeof(struct eagl_priv),
};
