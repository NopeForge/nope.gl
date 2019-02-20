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

static int gl_reconfigure(struct ngl_ctx *s, const struct ngl_config *config)
{
    int ret = ngli_glcontext_resize(s->glcontext, config->width, config->height);
    if (ret < 0)
        return ret;

    struct ngl_config *current_config = &s->config;
    current_config->width = config->width;
    current_config->height = config->height;

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);
        memcpy(current_config->viewport, config->viewport, sizeof(config->viewport));
    }

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);
    memcpy(current_config->clear_color, config->clear_color, sizeof(config->clear_color));

    return 0;
}

static int gl_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    memcpy(&s->config, config, sizeof(s->config));

    s->glcontext = ngli_glcontext_new(&s->config);
    if (!s->glcontext)
        return -1;

    if (s->glcontext->offscreen) {
        int ret = offscreen_fbo_init(s);
        if (ret < 0)
            return ret;
    }

    ngli_glstate_probe(s->glcontext, &s->glstate);

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0)
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);

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

    int ret = 0;
    if (ngli_glcontext_check_gl_error(gl, __FUNCTION__))
        ret = -1;

    if (gl->set_surface_pts)
        ngli_glcontext_set_surface_pts(gl, t);

    ngli_glcontext_swap_buffers(gl);

    return ret;
}

static void gl_destroy(struct ngl_ctx *s)
{
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
