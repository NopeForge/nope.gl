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

#include <string.h>

#include "log.h"
#include "nodes.h"
#include "backend.h"
#include "glcontext.h"
#include "memory.h"

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#if defined(HAVE_VAAPI_X11)
#include "vaapi.h"
#endif

static int offscreen_fbo_init(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &s->config;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) && config->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, "
            "multisample anti-aliasing will be disabled");
        config->samples = 0;
    }

    struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    attachment_params.width = config->width;
    attachment_params.height = config->height;
    attachment_params.samples = config->samples;
    attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
    int ret = ngli_texture_init(&s->fbo_color, gl, &attachment_params);
    if (ret < 0)
        return ret;

    attachment_params.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    ret = ngli_texture_init(&s->fbo_depth, gl, &attachment_params);
    if (ret < 0)
        return ret;

    const struct texture *attachments[] = {&s->fbo_color, &s->fbo_depth};
    const int nb_attachments = NGLI_ARRAY_NB(attachments);
    struct fbo_params fbo_params = {
        .width = config->width,
        .height = config->height,
        .nb_attachments = nb_attachments,
        .attachments = attachments,
    };
    ret = ngli_fbo_init(&s->fbo, gl, &fbo_params);
    if (ret < 0)
        return ret;

    ret = ngli_fbo_bind(&s->fbo);
    if (ret < 0)
        return ret;

    ngli_glViewport(gl, 0, 0, config->width, config->height);

    return 0;
}

static void offscreen_fbo_reset(struct ngl_ctx *s)
{
    ngli_fbo_reset(&s->fbo);
    ngli_texture_reset(&s->fbo_color);
    ngli_texture_reset(&s->fbo_depth);
}

static void capture_default(struct ngl_ctx *s)
{
    struct ngl_config *config = &s->config;
    struct fbo *fbo = &s->fbo;
    struct fbo *capture_fbo = &s->capture_fbo;

    ngli_fbo_blit(fbo, capture_fbo, 1);
    ngli_fbo_bind(capture_fbo);
    ngli_fbo_read_pixels(capture_fbo, config->capture_buffer);
    ngli_fbo_unbind(capture_fbo);
}

static void capture_ios(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct fbo *fbo = &s->fbo;
    struct fbo *capture_fbo = &s->capture_fbo;

    ngli_fbo_blit(fbo, capture_fbo, 1);
    ngli_glFinish(gl);
}

static void capture_gles_msaa(struct ngl_ctx *s)
{
    struct ngl_config *config = &s->config;
    struct fbo *fbo = &s->fbo;
    struct fbo *capture_fbo = &s->capture_fbo;
    struct fbo *oes_resolve_fbo = &s->oes_resolve_fbo;

    ngli_fbo_blit(fbo, oes_resolve_fbo, 0);
    ngli_fbo_bind(oes_resolve_fbo);
    ngli_fbo_blit(oes_resolve_fbo, capture_fbo, 1);
    ngli_fbo_unbind(oes_resolve_fbo);
    ngli_fbo_bind(capture_fbo);
    ngli_fbo_read_pixels(capture_fbo, config->capture_buffer);
    ngli_fbo_unbind(capture_fbo);
}

static void capture_ios_msaa(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct fbo *fbo = &s->fbo;
    struct fbo *capture_fbo = &s->capture_fbo;
    struct fbo *oes_resolve_fbo = &s->oes_resolve_fbo;

    ngli_fbo_blit(fbo, oes_resolve_fbo, 0);
    ngli_fbo_bind(oes_resolve_fbo);
    ngli_fbo_blit(oes_resolve_fbo, capture_fbo, 1);
    ngli_fbo_unbind(oes_resolve_fbo);
    ngli_glFinish(gl);
}

static void capture_cpu_fallback(struct ngl_ctx *s)
{
    struct ngl_config *config = &s->config;
    struct fbo *fbo = &s->fbo;

    ngli_fbo_read_pixels(fbo, s->capture_buffer);
    const int step = config->width * 4;
    const uint8_t *src = s->capture_buffer + (config->height - 1) * step;
    uint8_t *dst = config->capture_buffer;
    for (int i = 0; i < config->height; i++) {
        memcpy(dst, src, step);
        dst += step;
        src -= step;
    }
}

static int capture_init(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &s->config;
    const int ios_capture = gl->platform == NGL_PLATFORM_IOS && config->window;

    if (!config->capture_buffer && !ios_capture)
        return 0;

    if (gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) {
        if (ios_capture) {
#if defined(TARGET_IPHONE)
            CVPixelBufferRef capture_cvbuffer = (CVPixelBufferRef)config->window;
            s->capture_cvbuffer = (CVPixelBufferRef)CFRetain(capture_cvbuffer);
            if (!s->capture_cvbuffer)
                return -1;

            CVOpenGLESTextureCacheRef *cache = ngli_glcontext_get_texture_cache(gl);
            int width = CVPixelBufferGetWidth(s->capture_cvbuffer);
            int height = CVPixelBufferGetHeight(s->capture_cvbuffer);
            CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                        *cache,
                                                                        s->capture_cvbuffer,
                                                                        NULL,
                                                                        GL_TEXTURE_2D,
                                                                        GL_RGBA,
                                                                        width,
                                                                        height,
                                                                        GL_BGRA,
                                                                        GL_UNSIGNED_BYTE,
                                                                        0,
                                                                        &s->capture_cvtexture);
            if (err != noErr) {
                LOG(ERROR, "could not create CoreVideo texture from CVPixelBuffer: 0x%x", err);
                return -1;
            }

            GLuint id = CVOpenGLESTextureGetName(s->capture_cvtexture);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_B8G8R8A8_UNORM;
            attachment_params.width = width;
            attachment_params.height = height;
            int ret = ngli_texture_wrap(&s->capture_fbo_color, gl, &attachment_params, id);
            if (ret < 0)
                return ret;
#endif
        } else {
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            attachment_params.width = config->width;
            attachment_params.height = config->height;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            int ret = ngli_texture_init(&s->capture_fbo_color, gl, &attachment_params);
            if (ret < 0)
                return ret;
        }

        const struct texture *attachments[] = {&s->capture_fbo_color};
        const int nb_attachments = NGLI_ARRAY_NB(attachments);

        struct fbo_params fbo_params = {
            .width = config->width,
            .height = config->height,
            .nb_attachments = nb_attachments,
            .attachments = attachments,
        };
        int ret = ngli_fbo_init(&s->capture_fbo, gl, &fbo_params);
        if (ret < 0)
            return ret;

        if (gl->backend == NGL_BACKEND_OPENGLES && config->samples > 0) {
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            attachment_params.width = config->width;
            attachment_params.height = config->height;
            attachment_params.samples = 0;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            int ret = ngli_texture_init(&s->oes_resolve_fbo_color, gl, &attachment_params);
            if (ret < 0)
                return ret;

            const struct texture *attachments[] = {&s->oes_resolve_fbo_color};
            const int nb_attachments = NGLI_ARRAY_NB(attachments);
            struct fbo_params fbo_params = {
                .width = config->width,
                .height = config->height,
                .nb_attachments = nb_attachments,
                .attachments = attachments,
            };
            ret = ngli_fbo_init(&s->oes_resolve_fbo, gl, &fbo_params);
            if (ret < 0)
                return ret;

            s->capture_func = config->capture_buffer ? capture_gles_msaa : capture_ios_msaa;
        } else {
            s->capture_func = config->capture_buffer ? capture_default : capture_ios;
        }

    } else {
        if (ios_capture) {
            LOG(WARNING, "context does not support the framebuffer object feature, "
                "capturing to a CVPixelBuffer is not supported");
            return -1;
        }
        s->capture_buffer = ngli_calloc(config->width * config->height, 4 /* RGBA */);
        if (!s->capture_buffer)
            return -1;

        s->capture_func = capture_cpu_fallback;
    }

    ngli_assert(s->capture_func);

    return 0;
}

static void capture_reset(struct ngl_ctx *s)
{
    ngli_fbo_reset(&s->capture_fbo);
    ngli_texture_reset(&s->capture_fbo_color);
    ngli_fbo_reset(&s->oes_resolve_fbo);
    ngli_texture_reset(&s->oes_resolve_fbo_color);
    ngli_free(s->capture_buffer);
    s->capture_buffer = NULL;
#if defined(TARGET_IPHONE)
    if (s->capture_cvbuffer) {
        CFRelease(s->capture_cvbuffer);
        s->capture_cvbuffer = NULL;
    }
    if (s->capture_cvtexture) {
        CFRelease(s->capture_cvtexture);
        s->capture_cvtexture = NULL;
    }
#endif
    s->capture_func = NULL;
}

static int gl_reconfigure(struct ngl_ctx *s, const struct ngl_config *config)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *current_config = &s->config;

    ngli_glcontext_set_swap_interval(gl, config->swap_interval);
    current_config->swap_interval = config->swap_interval;

    current_config->set_surface_pts = config->set_surface_pts;

    const int update_dimensions = current_config->width != config->width ||
                                  current_config->height != config->height;
    current_config->width = config->width;
    current_config->height = config->height;

    const int update_capture = !current_config->capture_buffer != !config->capture_buffer;
    current_config->capture_buffer = config->capture_buffer;

    if (config->offscreen) {
        if (update_dimensions) {
            offscreen_fbo_reset(s);
            int ret = offscreen_fbo_init(s);
            if (ret < 0)
                return ret;
        }

        if (update_dimensions || update_capture) {
            capture_reset(s);
            int ret = capture_init(s);
            if (ret < 0)
                return ret;
        }
    } else {
        int ret = ngli_glcontext_resize(gl, config->width, config->height);
        if (ret < 0)
            return ret;
    }

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
        memcpy(current_config->viewport, config->viewport, sizeof(config->viewport));
    }

    const float *rgba = config->clear_color;
    ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    memcpy(current_config->clear_color, config->clear_color, sizeof(config->clear_color));

    const int scissor[] = {0, 0, gl->width, gl->height};
    struct graphicconfig *graphicconfig = &s->graphicconfig;
    memcpy(graphicconfig->scissor, scissor, sizeof(scissor));

    return 0;
}

static int gl_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    memcpy(&s->config, config, sizeof(s->config));

    if (!config->offscreen && config->capture_buffer) {
        LOG(ERROR, "capture_buffer is only supported with offscreen rendering");
        return -1;
    }

    s->glcontext = ngli_glcontext_new(&s->config);
    if (!s->glcontext)
        return -1;

    if (s->glcontext->offscreen) {
        int ret = offscreen_fbo_init(s);
        if (ret < 0)
            return ret;

        ret = capture_init(s);
        if (ret < 0)
            return ret;
    }

    ngli_glstate_probe(s->glcontext, &s->glstate);

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0)
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);

    struct graphicconfig *graphicconfig = &s->graphicconfig;
    ngli_graphicconfig_init(graphicconfig);
    const GLint scissor[] = {0, 0, config->width, config->height};
    memcpy(graphicconfig->scissor, scissor, sizeof(scissor));

#if defined(HAVE_VAAPI_X11)
    int ret = ngli_vaapi_init(s);
    if (ret < 0)
        LOG(WARNING, "could not initialize vaapi");
#endif

    return 0;
}

static int gl_pre_draw(struct ngl_ctx *s, double t)
{
    const struct glcontext *gl = s->glcontext;

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    return 0;
}

static int gl_post_draw(struct ngl_ctx *s, double t)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &s->config;

    ngli_honor_pending_glstate(s);

    if (s->capture_func)
        s->capture_func(s);

    int ret = 0;
    if (ngli_glcontext_check_gl_error(gl, __FUNCTION__))
        ret = -1;

    if (config->set_surface_pts)
        ngli_glcontext_set_surface_pts(gl, t);

    ngli_glcontext_swap_buffers(gl);

    return ret;
}

static void gl_destroy(struct ngl_ctx *s)
{
    capture_reset(s);
    offscreen_fbo_reset(s);
#if defined(HAVE_VAAPI_X11)
    ngli_vaapi_reset(s);
#endif
    ngli_glcontext_freep(&s->glcontext);
}

const struct backend ngli_backend_gl = {
    .name         = "OpenGL",
    .reconfigure  = gl_reconfigure,
    .configure    = gl_configure,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,
};

const struct backend ngli_backend_gles = {
    .name         = "OpenGL ES",
    .reconfigure  = gl_reconfigure,
    .configure    = gl_configure,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,
};
