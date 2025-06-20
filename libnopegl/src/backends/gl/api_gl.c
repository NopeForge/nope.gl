/*
 * Copyright 2022 GoPro Inc.
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

#include "config.h"

#include "internal.h"
#include "log.h"
#include "ngpu/opengl/ctx_gl.h"

static int cmd_make_current(struct ngl_ctx *s, void *arg)
{
    return ngpu_ctx_gl_make_current(s->gpu_ctx);
}

static int cmd_release_current(struct ngl_ctx *s, void *arg)
{
    return ngpu_ctx_gl_release_current(s->gpu_ctx);
}

static int cmd_configure(struct ngl_ctx *s, void *arg)
{
    const struct ngl_config *config = arg;
    return ngli_ctx_configure(s, config);
}

static int gl_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    if (config->platform == NGL_PLATFORM_MACOS ||
        config->platform == NGL_PLATFORM_IOS) {
        int ret = ngli_ctx_configure(s, config);
        if (ret < 0)
            return ret;

        ngpu_ctx_gl_release_current(s->gpu_ctx);

        return ngli_ctx_dispatch_cmd(s, cmd_make_current, NULL);
    }

    return ngli_ctx_dispatch_cmd(s, cmd_configure, (void *)config);
}

static int glw_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    int ret = ngli_ctx_configure(s, config);
    if (ret < 0)
        return ret;

    ngpu_ctx_gl_reset_state(s->gpu_ctx);

    return 0;
}

struct resize_params {
    uint32_t width;
    uint32_t height;
};

static int cmd_resize(struct ngl_ctx *s, void *arg)
{
    const struct resize_params *params = arg;
    return ngli_ctx_resize(s, params->width, params->height);
}

static int cmd_get_viewport(struct ngl_ctx *s, void *arg)
{
    int32_t *viewport = arg;
    return ngli_ctx_get_viewport(s, viewport);
}

static int gl_resize(struct ngl_ctx *s, uint32_t width, uint32_t height)
{
    const struct ngl_config *config = &s->config;
    if (config->platform == NGL_PLATFORM_MACOS ||
        config->platform == NGL_PLATFORM_IOS) {
        int ret = ngli_ctx_dispatch_cmd(s, cmd_release_current, NULL);
        if (ret < 0)
            return ret;

        ngpu_ctx_gl_make_current(s->gpu_ctx);
        ret = ngli_ctx_resize(s, width, height);
        if (ret < 0)
            return ret;
        ngpu_ctx_gl_release_current(s->gpu_ctx);

        return ngli_ctx_dispatch_cmd(s, cmd_make_current, NULL);
    }

    struct resize_params params = {
        .width = width,
        .height = height,
    };
    return ngli_ctx_dispatch_cmd(s, cmd_resize, &params);
}

static int gl_get_viewport(struct ngl_ctx *s, int32_t *viewport)
{
    return ngli_ctx_dispatch_cmd(s, cmd_get_viewport, viewport);
}

static int glw_resize(struct ngl_ctx *s, uint32_t width, uint32_t height)
{
    return ngli_ctx_resize(s, width, height);
}

static int glw_get_viewport(struct ngl_ctx *s, int32_t *viewport)
{
    return ngli_ctx_get_viewport(s, viewport);
}

static int cmd_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer)
{
    return ngli_ctx_set_capture_buffer(s, capture_buffer);
}

static int gl_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer)
{
    return ngli_ctx_dispatch_cmd(s, cmd_set_capture_buffer, capture_buffer);
}

static int glw_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer)
{
    LOG(ERROR, "capture_buffer is not supported by external OpenGL context");
    return NGL_ERROR_UNSUPPORTED;
}

static int cmd_set_scene(struct ngl_ctx *s, void *arg)
{
    struct ngl_scene *scene = arg;
    return ngli_ctx_set_scene(s, scene);
}

static int gl_set_scene(struct ngl_ctx *s, struct ngl_scene *scene)
{
    return ngli_ctx_dispatch_cmd(s, cmd_set_scene, scene);
}

static int glw_set_scene(struct ngl_ctx *s, struct ngl_scene *scene)
{
    ngpu_ctx_gl_reset_state(s->gpu_ctx);
    int ret = ngli_ctx_set_scene(s, scene);
    ngpu_ctx_gl_reset_state(s->gpu_ctx);
    return ret;
}

static int cmd_prepare_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;
    return ngli_ctx_prepare_draw(s, t);
}

static int gl_prepare_draw(struct ngl_ctx *s, double t)
{
    return ngli_ctx_dispatch_cmd(s, cmd_prepare_draw, &t);
}

static int glw_prepare_draw(struct ngl_ctx *s, double t)
{
    ngpu_ctx_gl_reset_state(s->gpu_ctx);
    int ret = ngli_ctx_prepare_draw(s, t);
    ngpu_ctx_gl_reset_state(s->gpu_ctx);
    return ret;
}

static int cmd_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;
    return ngli_ctx_draw(s, t);
}

static int gl_draw(struct ngl_ctx *s, double t)
{
    return ngli_ctx_dispatch_cmd(s, cmd_draw, &t);
}

static int glw_draw(struct ngl_ctx *s, double t)
{
    ngpu_ctx_gl_reset_state(s->gpu_ctx);
    int ret = ngli_ctx_draw(s, t);
    ngpu_ctx_gl_reset_state(s->gpu_ctx);
    return ret;
}

static int cmd_reset(struct ngl_ctx *s, void *arg)
{
    const int action = *(int *)arg;
    ngli_ctx_reset(s, action);
    return 0;
}

static void gl_reset(struct ngl_ctx *s, int action)
{
    ngli_ctx_dispatch_cmd(s, cmd_reset, &action);
}

static void glw_reset(struct ngl_ctx *s, int action)
{
    ngli_ctx_reset(s, action);
}

static int gl_wrap_framebuffer(struct ngl_ctx *s, uint32_t framebuffer)
{
    LOG(ERROR, "wrapping external OpenGL framebuffer is not supported by context");
    return NGL_ERROR_UNSUPPORTED;
}

static int glw_wrap_framebuffer(struct ngl_ctx *s, uint32_t framebuffer)
{
    int ret = ngpu_ctx_gl_wrap_framebuffer(s->gpu_ctx, framebuffer);
    if (ret < 0) {
        ngli_ctx_reset(s, NGLI_ACTION_KEEP_SCENE);
        return ret;
    }

    struct ngl_config *config = &s->config;
    struct ngl_config_gl *config_gl = config->backend_config;
    config_gl->external_framebuffer = framebuffer;

    return 0;
}

static int is_glw(const struct ngl_config *config)
{
    const struct ngl_config_gl *config_gl = config->backend_config;
    return config_gl && config_gl->external;
}

static int glv_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    return is_glw(config) ? glw_configure(s, config) : gl_configure(s, config);
}

static int glv_resize(struct ngl_ctx *s, uint32_t width, uint32_t height)
{
    return is_glw(&s->config) ? glw_resize(s, width, height) : gl_resize(s, width, height);
}

static int glv_get_viewport(struct ngl_ctx *s, int32_t *viewport)
{
    return is_glw(&s->config) ? glw_get_viewport(s, viewport) : gl_get_viewport(s, viewport);
}

static int glv_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer)
{
    return is_glw(&s->config) ? glw_set_capture_buffer(s, capture_buffer) : gl_set_capture_buffer(s, capture_buffer);
}

static int glv_set_scene(struct ngl_ctx *s, struct ngl_scene *scene)
{
    return is_glw(&s->config) ? glw_set_scene(s, scene) : gl_set_scene(s, scene);
}

static int glv_prepare_draw(struct ngl_ctx *s, double t)
{
    return is_glw(&s->config) ? glw_prepare_draw(s, t) : gl_prepare_draw(s, t);
}

static int glv_draw(struct ngl_ctx *s, double t)
{
    return is_glw(&s->config) ? glw_draw(s, t) : gl_draw(s, t);
}

static void glv_reset(struct ngl_ctx *s, int action)
{
    is_glw(&s->config) ? glw_reset(s, action) : gl_reset(s, action);
}

static int glv_wrap_framebuffer(struct ngl_ctx *s, uint32_t framebuffer)
{
    return is_glw(&s->config) ? glw_wrap_framebuffer(s, framebuffer) : gl_wrap_framebuffer(s, framebuffer);
}

const struct api_impl api_gl = {
    .configure           = glv_configure,
    .resize              = glv_resize,
    .get_viewport        = glv_get_viewport,
    .set_capture_buffer  = glv_set_capture_buffer,
    .set_scene           = glv_set_scene,
    .prepare_draw        = glv_prepare_draw,
    .draw                = glv_draw,
    .reset               = glv_reset,
    .gl_wrap_framebuffer = glv_wrap_framebuffer,
};
