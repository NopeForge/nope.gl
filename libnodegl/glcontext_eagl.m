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

#include "fbo.h"
#include "format.h"
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
    GLuint colorbuffer;
    struct fbo fbo;
    struct fbo fbo_ms;
};

static int eagl_init_layer(struct glcontext *ctx)
{
    if (![NSThread isMainThread]) {
        __block int ret;

        dispatch_sync(dispatch_get_main_queue(), ^{
            ret = eagl_init_layer(ctx);
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

static int eagl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
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
        eagl->view = (UIView *)window;
        if (!eagl->view) {
            LOG(ERROR, "could not retrieve UI view");
            return -1;
        }

        int ret = eagl_init_layer(ctx);
        if (ret < 0)
            return ret;
    }

    EAGLSharegroup *share_group = nil;
    if (other) {
        EAGLContext *shared_context = (EAGLContext *)other;
        share_group = [shared_context sharegroup];
    }

    eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3 sharegroup:share_group];
    if (!eagl->handle) {
        eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:share_group];
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

    return 0;
}

static int eagl_init_framebuffer(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    if (![NSThread isMainThread]) {
        __block int ret;
        ngli_glcontext_make_current(ctx, 0);

        dispatch_sync(dispatch_get_main_queue(), ^{
            ngli_glcontext_make_current(ctx, 1);
            ret = eagl_init_framebuffer(ctx);
            ngli_glcontext_make_current(ctx, 0);
        });

        ngli_glcontext_make_current(ctx, 1);
        return ret;
    }

    struct fbo *fbo = &eagl->fbo;
    struct fbo *fbo_ms = &eagl->fbo_ms;

    int ret = ngli_fbo_init(fbo, ctx, ctx->width, ctx->height, 0);
    if (ret < 0)
        return ret;

    if (eagl->pixel_buffer) {
        ctx->width = CVPixelBufferGetWidth(eagl->pixel_buffer);
        ctx->height = CVPixelBufferGetHeight(eagl->pixel_buffer);
        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    eagl->texture_cache,
                                                                    eagl->pixel_buffer,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    GL_RGBA,
                                                                    ctx->width,
                                                                    ctx->height,
                                                                    GL_BGRA,
                                                                    GL_UNSIGNED_BYTE,
                                                                    0,
                                                                    &eagl->texture);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from CVPixelBuffer: 0x%x", err);
            return -1;
        }

        GLuint id = CVOpenGLESTextureGetName(eagl->texture);
        ngli_glBindTexture(ctx, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(ctx, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ngli_glTexParameteri(ctx, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ngli_glBindTexture(ctx, GL_TEXTURE_2D, 0);

        if ((ret = ngli_fbo_resize(fbo, ctx->width, ctx->height))                < 0 ||
            (ret = ngli_fbo_attach_texture(fbo, NGLI_FORMAT_B8G8R8A8_UNORM, id)) < 0)
            return ret;
    } else {
        if (ctx->offscreen) {
            ret = ngli_fbo_create_renderbuffer(fbo, NGLI_FORMAT_B8G8R8A8_UNORM);
            if (ret < 0)
                return ret;
        } else {
            ngli_glGenRenderbuffers(ctx, 1, &eagl->colorbuffer);
            ngli_glBindRenderbuffer (ctx, GL_RENDERBUFFER, eagl->colorbuffer);
            [eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:eagl->layer];
            ngli_glGetRenderbufferParameteriv(ctx, GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &ctx->width);
            ngli_glGetRenderbufferParameteriv(ctx, GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &ctx->height);

            if ((ret = ngli_fbo_resize(fbo, ctx->width, ctx->height))                                    < 0 ||
                (ret = ngli_fbo_attach_renderbuffer(fbo, NGLI_FORMAT_B8G8R8A8_UNORM, eagl->colorbuffer)) < 0)
                return ret;
        }
    }

    if (!ctx->samples) {
        ret = ngli_fbo_create_renderbuffer(fbo, NGLI_FORMAT_D24_UNORM_S8_UINT);
        if (ret < 0)
            return ret;
    }

    ret = ngli_fbo_allocate(fbo);
    if (ret < 0)
        return ret;

    if (ctx->samples > 0) {
        if ((ret = ngli_fbo_init(fbo_ms, ctx, ctx->width, ctx->height, ctx->samples))   < 0 ||
            (ret = ngli_fbo_create_renderbuffer(fbo_ms, NGLI_FORMAT_B8G8R8A8_UNORM))    < 0 ||
            (ret = ngli_fbo_create_renderbuffer(fbo_ms, NGLI_FORMAT_D24_UNORM_S8_UINT)) < 0 ||
            (ret = ngli_fbo_allocate(fbo_ms))                                           < 0)
            return ret;
    }

    ngli_fbo_bind(ctx->samples ? fbo_ms : fbo);

    glViewport(0, 0, ctx->width, ctx->height);

    return 0;
}

static void eagl_uninit(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    ngli_fbo_reset(&eagl->fbo);
    ngli_fbo_reset(&eagl->fbo_ms);

    if (eagl->colorbuffer)
        ngli_glDeleteRenderbuffers(ctx, 1, &eagl->colorbuffer);

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

static int eagl_resize(struct glcontext *ctx, int width, int height)
{
    struct eagl_priv *eagl = ctx->priv_data;

    if (![NSThread isMainThread]) {
        __block int ret;
        ngli_glcontext_make_current(ctx, 0);

        dispatch_sync(dispatch_get_main_queue(), ^{
            ngli_glcontext_make_current(ctx, 1);
            ret = eagl_resize(ctx, width, height);
            ngli_glcontext_make_current(ctx, 0);
        });

        ngli_glcontext_make_current(ctx, 1);
        return ret;
    }

    glBindRenderbuffer (GL_RENDERBUFFER, eagl->colorbuffer);
    [eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:eagl->layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &ctx->width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &ctx->height);

    ngli_fbo_resize(&eagl->fbo, ctx->width, ctx->height);
    ngli_fbo_resize(&eagl->fbo_ms, ctx->width, ctx->height);

    struct fbo *fbo = ctx->samples ? &eagl->fbo_ms : &eagl->fbo;
    ngli_fbo_bind(fbo);

    glViewport(0, 0, ctx->width, ctx->height);

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

    if (ctx->samples > 0)
        ngli_fbo_blit(&eagl->fbo_ms, &eagl->fbo);

    if (ctx->offscreen) {
        if (eagl->pixel_buffer) {
            glFinish();
        }
    } else {
        ngli_glBindRenderbuffer(ctx, GL_RENDERBUFFER, eagl->colorbuffer);
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

static uintptr_t eagl_get_handle(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;
    return (uintptr_t)eagl->handle;
}

const struct glcontext_class ngli_glcontext_eagl_class = {
    .init = eagl_init,
    .init_framebuffer = eagl_init_framebuffer,
    .uninit = eagl_uninit,
    .resize = eagl_resize,
    .make_current = eagl_make_current,
    .swap_buffers = eagl_swap_buffers,
    .get_texture_cache = eagl_get_texture_cache,
    .get_proc_address = eagl_get_proc_address,
    .get_handle = eagl_get_handle,
    .priv_size = sizeof(struct eagl_priv),
};
