/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2019-2022 GoPro Inc.
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

#include "config.h"

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "bindgroup_gl.h"
#include "buffer_gl.h"
#include "ctx_gl.h"
#include "glcontext.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/ctx.h"
#include "ngpu/format.h"
#include "pipeline_gl.h"
#include "program_gl.h"
#include "rendertarget_gl.h"
#include "utils/memory.h"
#include "texture_gl.h"
#include "utils/utils.h"

#if DEBUG_GPU_CAPTURE
#include "ngpu/capture.h"
#endif

static void capture_cpu(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct ngl_config *config = &s->config;
    struct ngpu_rendertarget *rt = s_priv->capture_rt;
    struct ngpu_rendertarget_gl *rt_gl = (struct ngpu_rendertarget_gl *)rt;

    gl->funcs.BindFramebuffer(GL_FRAMEBUFFER, rt_gl->id);
    gl->funcs.ReadPixels(0, 0, rt->width, rt->height, GL_RGBA, GL_UNSIGNED_BYTE, config->capture_buffer);
}

static void capture_corevideo(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    gl->funcs.Finish();
}

#if defined(TARGET_IPHONE)
static int wrap_capture_cvpixelbuffer(struct ngpu_ctx *s,
                                      CVPixelBufferRef buffer,
                                      struct ngpu_texture **texturep,
                                      CVOpenGLESTextureRef *cv_texturep)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    CVOpenGLESTextureRef cv_texture = NULL;
    CVOpenGLESTextureCacheRef *cache = ngli_glcontext_get_texture_cache(gl);
    const size_t width = CVPixelBufferGetWidth(buffer);
    const size_t height = CVPixelBufferGetHeight(buffer);
    CVReturn cv_ret = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                   *cache,
                                                                   buffer,
                                                                   NULL,
                                                                   GL_TEXTURE_2D,
                                                                   GL_RGBA,
                                                                   (GLsizei)width,
                                                                   (GLsizei)height,
                                                                   GL_BGRA,
                                                                   GL_UNSIGNED_BYTE,
                                                                   0,
                                                                   &cv_texture);
    if (cv_ret != kCVReturnSuccess) {
        LOG(ERROR, "could not create CoreVideo texture from CVPixelBuffer: %d", cv_ret);
        return NGL_ERROR_EXTERNAL;
    }

    GLuint id = CVOpenGLESTextureGetName(cv_texture);
    gl->funcs.BindTexture(GL_TEXTURE_2D, id);
    gl->funcs.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->funcs.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->funcs.BindTexture(GL_TEXTURE_2D, 0);

    struct ngpu_texture *texture = ngpu_texture_create(s);
    if (!texture) {
        CFRelease(cv_texture);
        return NGL_ERROR_MEMORY;
    }

    const struct ngpu_texture_params attachment_params = {
        .type   = NGPU_TEXTURE_TYPE_2D,
        .format = NGPU_FORMAT_B8G8R8A8_UNORM,
        .width  = (int32_t)width,
        .height = (int32_t)height,
        .usage  = NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
    };

    const struct ngpu_texture_gl_wrap_params wrap_params = {
        .params  = &attachment_params,
        .texture = id,
    };

    int ret = ngpu_texture_gl_wrap(texture, &wrap_params);
    if (ret < 0) {
        CFRelease(cv_texture);
        ngpu_texture_freep(&texture);
        return ret;
    }

    *texturep = texture;
    *cv_texturep = cv_texture;

    return 0;
}

static void reset_capture_cvpixelbuffer(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    if (s_priv->capture_cvbuffer) {
        CFRelease(s_priv->capture_cvbuffer);
        s_priv->capture_cvbuffer = NULL;
    }
    if (s_priv->capture_cvtexture) {
        CFRelease(s_priv->capture_cvtexture);
        s_priv->capture_cvtexture = NULL;
    }
}
#endif

static int create_texture(struct ngpu_ctx *s, enum ngpu_format format, int32_t samples, uint32_t usage, struct ngpu_texture **texturep)
{
    const struct ngl_config *config = &s->config;

    struct ngpu_texture *texture = ngpu_texture_create(s);
    if (!texture)
        return NGL_ERROR_MEMORY;

    const struct ngpu_texture_params params = {
        .type    = NGPU_TEXTURE_TYPE_2D,
        .format  = format,
        .width   = config->width,
        .height  = config->height,
        .samples = samples,
        .usage   = usage,
    };

    int ret = ngpu_texture_init(texture, &params);
    if (ret < 0) {
        ngpu_texture_freep(&texture);
        return ret;
    }

    *texturep = texture;
    return 0;
}

static int create_rendertarget(struct ngpu_ctx *s,
                               struct ngpu_texture *color,
                               struct ngpu_texture *resolve_color,
                               struct ngpu_texture *depth_stencil,
                               enum ngpu_load_op load_op,
                               struct ngpu_rendertarget **rendertargetp)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    const struct ngl_config *config = &s->config;
    const struct ngl_config_gl *config_gl = config->backend_config;

    struct ngpu_rendertarget *rendertarget = ngpu_rendertarget_create(s);
    if (!rendertarget)
        return NGL_ERROR_MEMORY;

    const struct ngpu_rendertarget_params params = {
        .width = config->width,
        .height = config->height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment     = color,
            .resolve_target = resolve_color,
            .load_op        = load_op,
            .clear_value[0] = config->clear_color[0],
            .clear_value[1] = config->clear_color[1],
            .clear_value[2] = config->clear_color[2],
            .clear_value[3] = config->clear_color[3],
            .store_op       = NGPU_STORE_OP_STORE,
        },
        .depth_stencil = {
            .attachment = depth_stencil,
            .load_op    = load_op,
            .store_op   = NGPU_STORE_OP_STORE,
        },
    };

    int ret;
    if (color) {
        ret = ngpu_rendertarget_init(rendertarget, &params);
    } else {
        const int external = config_gl ? config_gl->external : 0;
        const GLuint default_fbo_id = ngli_glcontext_get_default_framebuffer(gl);
        const GLuint fbo_id = external ? config_gl->external_framebuffer : default_fbo_id;
        ret = ngpu_rendertarget_gl_wrap(rendertarget, &params, fbo_id);
    }
    if (ret < 0) {
        ngpu_rendertarget_freep(&rendertarget);
        return ret;
    }

    *rendertargetp = rendertarget;
    return 0;
}

#define COLOR_USAGE NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT
#define DEPTH_USAGE NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT

static int offscreen_rendertarget_init(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngl_config *config = &s->config;

    if (config->capture_buffer_type == NGL_CAPTURE_BUFFER_TYPE_COREVIDEO) {
#if defined(TARGET_IPHONE)
        if (config->capture_buffer) {
            s_priv->capture_cvbuffer = (CVPixelBufferRef)CFRetain(config->capture_buffer);
            int ret = wrap_capture_cvpixelbuffer(s, s_priv->capture_cvbuffer,
                                                 &s_priv->capture_texture, &s_priv->capture_cvtexture);
            if (ret < 0)
                return ret;
        } else {
            int ret = create_texture(s, NGPU_FORMAT_R8G8B8A8_UNORM, 0, COLOR_USAGE, &s_priv->capture_texture);
            if (ret < 0)
                return ret;
        }
#else
        LOG(ERROR, "CoreVideo capture is only supported on iOS");
        return NGL_ERROR_UNSUPPORTED;
#endif
    } else if (config->capture_buffer_type == NGL_CAPTURE_BUFFER_TYPE_CPU) {
        int ret = create_texture(s, NGPU_FORMAT_R8G8B8A8_UNORM, 0, COLOR_USAGE, &s_priv->capture_texture);
        if (ret < 0)
            return ret;
    } else {
        LOG(ERROR, "unsupported capture buffer type: %d", config->capture_buffer_type);
        return NGL_ERROR_UNSUPPORTED;
    }

    int ret = create_rendertarget(s, s_priv->capture_texture, NULL, NULL,
                                  NGPU_LOAD_OP_CLEAR, &s_priv->capture_rt);
    if (ret < 0)
        return ret;

    ret = create_texture(s, NGPU_FORMAT_R8G8B8A8_UNORM, 0, COLOR_USAGE, &s_priv->color);
    if (ret < 0)
        return ret;

    if (config->samples) {
        ret = create_texture(s, NGPU_FORMAT_R8G8B8A8_UNORM, config->samples, COLOR_USAGE, &s_priv->ms_color);
        if (ret < 0)
            return ret;
    }

    ret = create_texture(s, NGPU_FORMAT_D24_UNORM_S8_UINT, config->samples, DEPTH_USAGE, &s_priv->depth_stencil);
    if (ret < 0)
        return ret;

    struct ngpu_texture *color         = s_priv->ms_color ? s_priv->ms_color : s_priv->color;
    struct ngpu_texture *resolve_color = s_priv->ms_color ? s_priv->color    : NULL;
    struct ngpu_texture *depth_stencil = s_priv->depth_stencil;

    if ((ret = create_rendertarget(s, color, resolve_color, depth_stencil,
                                   NGPU_LOAD_OP_CLEAR, &s_priv->default_rt)) < 0 ||
        (ret = create_rendertarget(s, color, resolve_color, depth_stencil,
                                   NGPU_LOAD_OP_LOAD, &s_priv->default_rt_load)) < 0)
        return ret;

    static const capture_func_type capture_func_map[] = {
        [NGL_CAPTURE_BUFFER_TYPE_CPU]       = capture_cpu,
        [NGL_CAPTURE_BUFFER_TYPE_COREVIDEO] = capture_corevideo,
    };
    s_priv->capture_func = capture_func_map[config->capture_buffer_type];

    return 0;
}

static int onscreen_rendertarget_init(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    int ret;
    if ((ret = create_rendertarget(s, NULL, NULL, NULL, NGPU_LOAD_OP_CLEAR,
                                   &s_priv->default_rt)) < 0 ||
        (ret = create_rendertarget(s, NULL, NULL, NULL, NGPU_LOAD_OP_LOAD,
                                   &s_priv->default_rt_load)) < 0)
        return ret;

    return 0;
}

static void rendertarget_reset(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    ngpu_rendertarget_freep(&s_priv->default_rt);
    ngpu_rendertarget_freep(&s_priv->default_rt_load);
    ngpu_texture_freep(&s_priv->color);
    ngpu_texture_freep(&s_priv->ms_color);
    ngpu_texture_freep(&s_priv->depth_stencil);

    ngpu_rendertarget_freep(&s_priv->capture_rt);
    ngpu_texture_freep(&s_priv->capture_texture);
#if defined(TARGET_IPHONE)
    reset_capture_cvpixelbuffer(s);
#endif
    s_priv->capture_func = NULL;
}

static void noop(const struct glcontext *gl, ...)
{
}

static int timer_init(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    if (gl->features & NGLI_FEATURE_GL_TIMER_QUERY) {
        s_priv->glGenQueries          = gl->funcs.GenQueries;
        s_priv->glDeleteQueries       = gl->funcs.DeleteQueries;
        s_priv->glBeginQuery          = gl->funcs.BeginQuery;
        s_priv->glEndQuery            = gl->funcs.EndQuery;
        s_priv->glQueryCounter        = gl->funcs.QueryCounter;
        s_priv->glGetQueryObjectui64v = gl->funcs.GetQueryObjectui64v;
    } else if (gl->features & NGLI_FEATURE_GL_EXT_DISJOINT_TIMER_QUERY) {
        s_priv->glGenQueries          = gl->funcs.GenQueriesEXT;
        s_priv->glDeleteQueries       = gl->funcs.DeleteQueriesEXT;
        s_priv->glBeginQuery          = gl->funcs.BeginQueryEXT;
        s_priv->glEndQuery            = gl->funcs.EndQueryEXT;
        s_priv->glQueryCounter        = gl->funcs.QueryCounterEXT;
        s_priv->glGetQueryObjectui64v = gl->funcs.GetQueryObjectui64vEXT;
    } else {
        s_priv->glGenQueries          = (void *)noop;
        s_priv->glDeleteQueries       = (void *)noop;
        s_priv->glBeginQuery          = (void *)noop;
        s_priv->glEndQuery            = (void *)noop;
        s_priv->glQueryCounter        = (void *)noop;
        s_priv->glGetQueryObjectui64v = (void *)noop;
    }
    s_priv->glGenQueries(2, s_priv->queries);

    return 0;
}

static void timer_reset(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    if (s_priv->glDeleteQueries)
        s_priv->glDeleteQueries(2, s_priv->queries);
}

static struct ngpu_ctx *gl_create(const struct ngl_config *config)
{
    struct ngpu_ctx_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    return (struct ngpu_ctx *)s;
}

#define GL_ENUM_STR_CASE(prefix, error) case prefix##_##error: return #error

static const char *gl_debug_source_to_str(GLenum source)
{
    switch (source) {
    GL_ENUM_STR_CASE(GL_DEBUG_SOURCE, API);
    GL_ENUM_STR_CASE(GL_DEBUG_SOURCE, WINDOW_SYSTEM);
    GL_ENUM_STR_CASE(GL_DEBUG_SOURCE, SHADER_COMPILER);
    GL_ENUM_STR_CASE(GL_DEBUG_SOURCE, THIRD_PARTY);
    GL_ENUM_STR_CASE(GL_DEBUG_SOURCE, APPLICATION);
    GL_ENUM_STR_CASE(GL_DEBUG_SOURCE, OTHER);
    default:
        return "UNKNOWN";
    }
}

static const char *gl_debug_type_to_str(GLenum type)
{
    switch (type) {
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, ERROR);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, DEPRECATED_BEHAVIOR);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, UNDEFINED_BEHAVIOR);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, PORTABILITY);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, PERFORMANCE);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, OTHER);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, MARKER);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, PUSH_GROUP);
    GL_ENUM_STR_CASE(GL_DEBUG_TYPE, POP_GROUP);
    default:
        return "UNKNOWN";
    }
}

static enum ngl_log_level gl_debug_type_to_log_level(GLenum type)
{
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    case GL_DEBUG_TYPE_PORTABILITY:
        return NGL_LOG_ERROR;
    default:
        return NGL_LOG_DEBUG;
    }
}

static const char *gl_debug_severity_to_str(GLenum severity)
{
    switch (severity) {
    GL_ENUM_STR_CASE(GL_DEBUG_SEVERITY, HIGH);
    GL_ENUM_STR_CASE(GL_DEBUG_SEVERITY, MEDIUM);
    GL_ENUM_STR_CASE(GL_DEBUG_SEVERITY, LOW);
    GL_ENUM_STR_CASE(GL_DEBUG_SEVERITY, NOTIFICATION);
    default:
        return "UNKNOWN";
    }
}

static void NGLI_GL_APIENTRY gl_debug_message_callback(GLenum source,
                                                       GLenum type,
                                                       GLuint id,
                                                       GLenum severity,
                                                       GLsizei length,
                                                       const GLchar *message,
                                                       const void *user_param)
{
    const enum ngl_log_level log_level = gl_debug_type_to_log_level(type);
    const char *msg_source = gl_debug_source_to_str(source);
    const char *msg_type = gl_debug_type_to_str(type);
    const char *msg_severity = gl_debug_severity_to_str(severity);

    ngli_log_print(log_level, __FILE__, __LINE__, __func__, "%s:%s:%s: %s", msg_source, msg_type, msg_severity, message);

    // Do not abort if the source is the shader compiler as we want the error to
    // be properly reported and propagated to the user (with proper error messages)
    if (log_level == NGL_LOG_ERROR && source != GL_DEBUG_SOURCE_SHADER_COMPILER && DEBUG_GL)
        ngli_assert(0);
}

static const struct {
    uint64_t feature;
    uint64_t feature_gl;
} feature_map[] = {
    {NGPU_FEATURE_COMPUTE,               NGLI_FEATURE_GL_COMPUTE_SHADER_ALL},
    {NGPU_FEATURE_SOFTWARE,              NGLI_FEATURE_GL_SOFTWARE},
    {NGPU_FEATURE_IMAGE_LOAD_STORE,      NGLI_FEATURE_GL_SHADER_IMAGE_LOAD_STORE | NGLI_FEATURE_GL_SHADER_IMAGE_SIZE},
    {NGPU_FEATURE_STORAGE_BUFFER,        NGLI_FEATURE_GL_SHADER_STORAGE_BUFFER_OBJECT},
    {NGPU_FEATURE_BUFFER_MAP_PERSISTENT, NGLI_FEATURE_GL_BUFFER_STORAGE},
    {NGPU_FEATURE_BUFFER_MAP_PERSISTENT, NGLI_FEATURE_GL_EXT_BUFFER_STORAGE},
    {NGPU_FEATURE_DEPTH_STENCIL_RESOLVE, 0},
};

static void ngpu_ctx_info_init(struct ngpu_ctx *s)
{
    const struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    const struct glcontext *gl = s_priv->glcontext;

    s->version = gl->version;
    s->language_version = gl->glsl_version;
    for (size_t i = 0; i < NGLI_ARRAY_NB(feature_map); i++) {
        const uint64_t feature = feature_map[i].feature;
        const uint64_t feature_gl = feature_map[i].feature_gl;
        if (NGLI_HAS_ALL_FLAGS(gl->features, feature_gl))
            s->features |= feature;
    }
    s->limits = gl->limits;
    s->nb_in_flight_frames = 2;
}

static int create_command_buffers(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    s_priv->update_cmd_buffers = ngli_calloc(s->nb_in_flight_frames, sizeof(struct ngpu_cmd_buffer_gl *));
    s_priv->draw_cmd_buffers = ngli_calloc(s->nb_in_flight_frames, sizeof(struct ngpu_cmd_buffer_gl *));
    if (!s_priv->update_cmd_buffers || !s_priv->draw_cmd_buffers)
        return NGL_ERROR_MEMORY;

    for (uint32_t i = 0; i < s->nb_in_flight_frames; i++) {
        s_priv->update_cmd_buffers[i] = ngpu_cmd_buffer_gl_create(s);
        if (!s_priv->update_cmd_buffers[i])
            return NGL_ERROR_MEMORY;

        int ret = ngpu_cmd_buffer_gl_init(s_priv->update_cmd_buffers[i]);
        if (ret < 0)
            return ret;

        s_priv->draw_cmd_buffers[i] = ngpu_cmd_buffer_gl_create(s);
        if (!s_priv->draw_cmd_buffers[i])
            return NGL_ERROR_MEMORY;

        ret = ngpu_cmd_buffer_gl_init(s_priv->draw_cmd_buffers[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void destroy_command_buffers(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    if (s_priv->update_cmd_buffers) {
        for (uint32_t i = 0; i < s->nb_in_flight_frames; i++)
            ngpu_cmd_buffer_gl_freep(&s_priv->update_cmd_buffers[i]);
        ngli_freep(&s_priv->update_cmd_buffers);
    }

    if (s_priv->draw_cmd_buffers) {
        for (uint32_t i = 0; i < s->nb_in_flight_frames; i++)
            ngpu_cmd_buffer_gl_freep(&s_priv->draw_cmd_buffers[i]);
        ngli_freep(&s_priv->draw_cmd_buffers);
    }
}

static int gl_init(struct ngpu_ctx *s)
{
    int ret;
    struct ngl_config *config = &s->config;
    const struct ngl_config_gl *config_gl = config->backend_config;
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    const int external = config_gl ? config_gl->external : 0;
    if (external) {
        if (config->width <= 0 || config->height <= 0) {
            LOG(ERROR, "could not create external context with invalid dimensions (%dx%d)",
                config->width, config->height);
            return NGL_ERROR_INVALID_ARG;
        }
        if (config->capture_buffer) {
            LOG(ERROR, "capture_buffer is not supported by external context");
            return NGL_ERROR_INVALID_ARG;
        }
    } else if (config->offscreen) {
        if (config->width <= 0 || config->height <= 0) {
            LOG(ERROR, "could not create offscreen context with invalid dimensions (%dx%d)",
                config->width, config->height);
            return NGL_ERROR_INVALID_ARG;
        }
    } else {
        if (config->capture_buffer) {
            LOG(ERROR, "capture_buffer is not supported by onscreen context");
            return NGL_ERROR_INVALID_ARG;
        }
    }

#if DEBUG_GPU_CAPTURE
    const char *var = getenv("NGL_GPU_CAPTURE");
    s->gpu_capture = var && !strcmp(var, "yes");
    if (s->gpu_capture) {
        s->gpu_capture_ctx = ngpu_capture_ctx_create(s);
        if (!s->gpu_capture_ctx) {
            LOG(ERROR, "could not create GPU capture context");
            return NGL_ERROR_MEMORY;
        }
        ret = ngpu_capture_init(s->gpu_capture_ctx);
        if (ret < 0) {
            LOG(ERROR, "could not initialize GPU capture");
            s->gpu_capture = 0;
            return ret;
        }
    }
#endif

    const struct glcontext_params params = {
        .platform      = config->platform,
        .backend       = config->backend,
        .external      = external,
        .display       = config->display,
        .window        = config->window,
        .swap_interval = config->swap_interval,
        .offscreen     = config->offscreen,
        .width         = config->width,
        .height        = config->height,
        .samples       = config->samples,
        .debug         = config->debug,
    };

    s_priv->glcontext = ngli_glcontext_create(&params);
    if (!s_priv->glcontext)
        return NGL_ERROR_MEMORY;

    struct glcontext *gl = s_priv->glcontext;

    if (gl->debug && (gl->features & NGLI_FEATURE_GL_KHR_DEBUG)) {
        gl->funcs.Enable(GL_DEBUG_OUTPUT);
        gl->funcs.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        gl->funcs.DebugMessageCallback(gl_debug_message_callback, NULL);
    }

    ngpu_ctx_info_init(s);

#if DEBUG_GPU_CAPTURE
    if (s->gpu_capture)
        ngpu_capture_begin(s->gpu_capture_ctx);
#endif

    if (external) {
        ret = ngpu_ctx_gl_wrap_framebuffer(s, config_gl->external_framebuffer);
    } else if (gl->offscreen) {
        ret = offscreen_rendertarget_init(s);
    } else {
        /* Sync context config dimensions with glcontext (swapchain) dimensions */
        config->width = gl->width;
        config->height = gl->height;
        ret = onscreen_rendertarget_init(s);
    }
    if (ret < 0)
        return ret;

    ret = timer_init(s);
    if (ret < 0)
        return ret;

    s_priv->default_rt_layout.samples = gl->samples;
    s_priv->default_rt_layout.nb_colors = 1;
    s_priv->default_rt_layout.colors[0].format = NGPU_FORMAT_R8G8B8A8_UNORM;
    s_priv->default_rt_layout.colors[0].resolve = gl->samples > 1;
    s_priv->default_rt_layout.depth_stencil.format = NGPU_FORMAT_D24_UNORM_S8_UINT;
    s_priv->default_rt_layout.depth_stencil.resolve = gl->samples > 1;

    ngpu_glstate_reset(gl, &s_priv->glstate);
    ngpu_glstate_enable_scissor_test(gl, &s_priv->glstate, GL_TRUE);

    ret = create_command_buffers(s);
    if (ret < 0)
        return ret;

    return 0;
}

static int gl_resize(struct ngpu_ctx *s, int32_t width, int32_t height)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct ngl_config *config = &s->config;
    struct ngl_config_gl *config_gl = config->backend_config;
    const int external = config_gl ? config_gl->external : 0;

    if (external) {
        config->width = width;
        config->height = height;
    } else if (!config->offscreen) {
        int ret = ngli_glcontext_resize(gl, width, height);
        if (ret < 0)
            return ret;
        config->width = gl->width;
        config->height = gl->height;
    } else {
        LOG(ERROR, "resize operation is not supported by offscreen context");
        return NGL_ERROR_UNSUPPORTED;
    }

    s_priv->default_rt->width = config->width;
    s_priv->default_rt->height = config->height;
    s_priv->default_rt_load->width = config->width;
    s_priv->default_rt_load->height = config->height;

    if (!external) {
        /*
        * The default framebuffer id can change after a resize operation on EAGL,
        * thus we need to update the rendertargets wrapping the default framebuffer
        */
        struct ngpu_rendertarget_gl *rt_gl = (struct ngpu_rendertarget_gl *)s_priv->default_rt;
        struct ngpu_rendertarget_gl *rt_load_gl = (struct ngpu_rendertarget_gl *)s_priv->default_rt_load;
        rt_gl->id = rt_load_gl->id = ngli_glcontext_get_default_framebuffer(gl);
    }

    return 0;
}

#if defined(TARGET_IPHONE)
static int update_capture_cvpixelbuffer(struct ngpu_ctx *s, CVPixelBufferRef capture_buffer)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    ngpu_rendertarget_freep(&s_priv->default_rt);
    ngpu_rendertarget_freep(&s_priv->default_rt_load);
    ngpu_texture_freep(&s_priv->color);
    reset_capture_cvpixelbuffer(s);

    if (capture_buffer) {
        s_priv->capture_cvbuffer = (CVPixelBufferRef)CFRetain(capture_buffer);
        int ret = wrap_capture_cvpixelbuffer(s, s_priv->capture_cvbuffer,
                                             &s_priv->color, &s_priv->capture_cvtexture);
        if (ret < 0)
            return ret;
    } else {
        int ret = create_texture(s, NGPU_FORMAT_R8G8B8A8_UNORM, 0, COLOR_USAGE, &s_priv->color);
        if (ret < 0)
            return ret;
    }

    struct ngpu_texture *color         = s_priv->ms_color ? s_priv->ms_color : s_priv->color;
    struct ngpu_texture *resolve_color = s_priv->ms_color ? s_priv->color : NULL;
    struct ngpu_texture *depth_stencil = s_priv->depth_stencil;

    int ret;
    if ((ret = create_rendertarget(s, color, resolve_color, depth_stencil,
                                   NGPU_LOAD_OP_CLEAR, &s_priv->default_rt)) < 0 ||
        (ret = create_rendertarget(s, color, resolve_color, depth_stencil,
                                   NGPU_LOAD_OP_LOAD, &s_priv->default_rt_load)) < 0)
        return ret;

    return 0;
}
#endif

static int gl_set_capture_buffer(struct ngpu_ctx *s, void *capture_buffer)
{
    struct ngl_config *config = &s->config;
    const struct ngl_config_gl *config_gl = config->backend_config;
    const int external = config_gl ? config_gl->external : 0;

    if (external) {
        LOG(ERROR, "capture_buffer is not supported by external context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (!config->offscreen) {
        LOG(ERROR, "capture_buffer is not supported by onscreen context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (config->capture_buffer_type == NGL_CAPTURE_BUFFER_TYPE_COREVIDEO) {
#if defined(TARGET_IPHONE)
        int ret = update_capture_cvpixelbuffer(s, capture_buffer);
        if (ret < 0)
            return ret;
#else
        return NGL_ERROR_UNSUPPORTED;
#endif
    }

    config->capture_buffer = capture_buffer;

    return 0;
}

int ngpu_ctx_gl_make_current(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    return ngli_glcontext_make_current(gl, 1);
}

int ngpu_ctx_gl_release_current(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    return ngli_glcontext_make_current(gl, 0);
}

void ngpu_ctx_gl_reset_state(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    ngpu_glstate_reset(gl, &s_priv->glstate);
}

int ngpu_ctx_gl_wrap_framebuffer(struct ngpu_ctx *s, GLuint fbo)
{
    struct ngl_config *config = &s->config;
    struct ngl_config_gl *config_gl = config->backend_config;
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    const int external = config_gl ? config_gl->external : 0;
    if (!external) {
        LOG(ERROR, "wrapping external OpenGL framebuffers is not supported by context");
        return NGL_ERROR_UNSUPPORTED;
    }

    GLuint prev_fbo = 0;
    gl->funcs.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prev_fbo);

    const GLenum target = GL_DRAW_FRAMEBUFFER;
    gl->funcs.BindFramebuffer(target, fbo);

    const int es = config->backend == NGL_BACKEND_OPENGLES;
    const GLenum default_color_attachment = es ? GL_BACK : GL_FRONT_LEFT;
    const GLenum color_attachment   = fbo ? GL_COLOR_ATTACHMENT0  : default_color_attachment;
    const GLenum depth_attachment   = fbo ? GL_DEPTH_ATTACHMENT   : GL_DEPTH;
    const GLenum stencil_attachment = fbo ? GL_STENCIL_ATTACHMENT : GL_STENCIL;
    const struct {
        const char *buffer_name;
        const char *component_name;
        GLenum attachment;
        const GLenum property;
    } components[] = {
        {"color",   "red",     color_attachment,   GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE},
        {"color",   "green",   color_attachment,   GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE},
        {"color",   "blue",    color_attachment,   GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE},
        {"color",   "alpha",   color_attachment,   GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE},
        {"depth",   "depth",   depth_attachment,   GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE},
        {"stencil", "stencil", stencil_attachment, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE},
    };
    for (size_t i = 0; i < NGLI_ARRAY_NB(components); i++) {
        GLint type = 0;
        gl->funcs.GetFramebufferAttachmentParameteriv(target,
            components[i].attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (!type) {
            LOG(ERROR, "external framebuffer have no %s buffer attached to it", components[i].buffer_name);
            gl->funcs.BindFramebuffer(target, prev_fbo);
            return NGL_ERROR_GRAPHICS_UNSUPPORTED;
        }

        GLint size = 0;
        gl->funcs.GetFramebufferAttachmentParameteriv(target,
            components[i].attachment, components[i].property, &size);
        if (!size) {
            LOG(ERROR, "external framebuffer have no %s component", components[i].component_name);
            gl->funcs.BindFramebuffer(target, prev_fbo);
            return NGL_ERROR_GRAPHICS_UNSUPPORTED;
        }
    }

    gl->funcs.BindFramebuffer(target, prev_fbo);

    ngpu_rendertarget_freep(&s_priv->default_rt);
    ngpu_rendertarget_freep(&s_priv->default_rt_load);

    int ret;
    if ((ret = create_rendertarget(s, NULL, NULL, NULL,
                                   NGPU_LOAD_OP_CLEAR, &s_priv->default_rt)) < 0 ||
        (ret = create_rendertarget(s, NULL, NULL, NULL,
                                   NGPU_LOAD_OP_LOAD, &s_priv->default_rt_load)) < 0)
        return ret;

    config_gl->external_framebuffer = fbo;

    return 0;
}

static int gl_begin_update(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    s_priv->cur_cmd_buffer = s_priv->update_cmd_buffers[s->current_frame_index];
    int ret = ngpu_cmd_buffer_gl_wait(s_priv->cur_cmd_buffer);
    if (ret < 0)
        return ret;

    ret = ngpu_cmd_buffer_gl_begin(s_priv->cur_cmd_buffer);
    if (ret < 0)
        return ret;

    return 0;
}

static int gl_end_update(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    int ret = ngpu_cmd_buffer_gl_submit(s_priv->cur_cmd_buffer);
    if (ret < 0)
        return ret;

    return 0;
}

static int gl_begin_draw(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    const struct ngl_config *config = &s->config;

    if (config->hud)
#if defined(TARGET_DARWIN)
        s_priv->glBeginQuery(GL_TIME_ELAPSED, s_priv->queries[0]);
#else
        s_priv->glQueryCounter(s_priv->queries[0], GL_TIMESTAMP);
#endif

    s_priv->cur_cmd_buffer = s_priv->draw_cmd_buffers[s->current_frame_index];
    int ret = ngpu_cmd_buffer_gl_wait(s_priv->cur_cmd_buffer);
    if (ret < 0)
        return ret;

    ret = ngpu_cmd_buffer_gl_begin(s_priv->cur_cmd_buffer);
    if (ret < 0)
        return ret;

    return 0;
}

static void blit_vflip(struct ngpu_ctx *s, struct ngpu_rendertarget *src, struct ngpu_rendertarget *dst)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct ngpu_glstate *glstate = &s_priv->glstate;

    struct ngpu_rendertarget_gl *src_gl = (struct ngpu_rendertarget_gl *)src;
    const GLuint src_fbo = src_gl->resolve_id ? src_gl->resolve_id : src_gl->id;

    struct ngpu_rendertarget_gl *dst_gl = (struct ngpu_rendertarget_gl *)dst;
    const GLuint dst_fbo = dst_gl->id;

    const int32_t w = src->width, h = dst->height;

    gl->funcs.BindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
    gl->funcs.BindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);

    ngpu_glstate_enable_scissor_test(gl, glstate, GL_FALSE);

    gl->funcs.BlitFramebuffer(0, 0, w, h,
                               0, h, w, 0,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);

    ngpu_glstate_enable_scissor_test(gl, glstate, GL_TRUE);
}

static int gl_end_draw(struct ngpu_ctx *s, double t)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    const struct ngl_config *config = &s->config;
    const struct ngl_config_gl *config_gl = config->backend_config;

    int ret = ngpu_cmd_buffer_gl_submit(s_priv->cur_cmd_buffer);
    if (ret < 0)
        return ret;

    if (s_priv->capture_func && config->capture_buffer) {
        blit_vflip(s, s_priv->default_rt, s_priv->capture_rt);
        s_priv->capture_func(s);
    }

    ret = ngli_glcontext_check_gl_error(gl, __func__);

    const int external = config_gl ? config_gl->external : 0;
    if (!external && !config->offscreen) {
        if (config->set_surface_pts)
            ngli_glcontext_set_surface_pts(gl, t);

        ngli_glcontext_swap_buffers(gl);
    }

    return ret;
}

static int gl_query_draw_time(struct ngpu_ctx *s, int64_t *time)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;

    const struct ngl_config *config = &s->config;
    if (!config->hud)
        return NGL_ERROR_INVALID_USAGE;

    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    int ret = ngpu_cmd_buffer_gl_submit(cmd_buffer);
    if (ret < 0)
        return ret;

#if defined(TARGET_DARWIN)
    GLuint64 time_elapsed = 0;
    s_priv->glEndQuery(GL_TIME_ELAPSED);
    s_priv->glGetQueryObjectui64v(s_priv->queries[0], GL_QUERY_RESULT, &time_elapsed);
    *time = time_elapsed;
#else
    s_priv->glQueryCounter(s_priv->queries[1], GL_TIMESTAMP);

    GLuint64 start_time = 0;
    s_priv->glGetQueryObjectui64v(s_priv->queries[0], GL_QUERY_RESULT, &start_time);

    GLuint64 end_time = 0;
    s_priv->glGetQueryObjectui64v(s_priv->queries[1], GL_QUERY_RESULT, &end_time);

    *time = end_time - start_time;
#endif
    ret = ngpu_cmd_buffer_gl_begin(cmd_buffer);
    if (ret < 0)
        return ret;

    return 0;
}

static void gl_wait_idle(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    for (size_t i = 0; i < s->nb_in_flight_frames; i++) {
        ngpu_cmd_buffer_gl_wait(s_priv->update_cmd_buffers[i]);
        ngpu_cmd_buffer_gl_wait(s_priv->draw_cmd_buffers[i]);
    }
    gl->funcs.Finish();
}

static void gl_destroy(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    timer_reset(s);
    rendertarget_reset(s);
    destroy_command_buffers(s);
#if DEBUG_GPU_CAPTURE
    if (s->gpu_capture)
        ngpu_capture_end(s->gpu_capture_ctx);
    ngpu_capture_freep(&s->gpu_capture_ctx);
#endif
    ngli_glcontext_freep(&s_priv->glcontext);
}

static enum ngpu_cull_mode gl_transform_cull_mode(struct ngpu_ctx *s, enum ngpu_cull_mode cull_mode)
{
    return cull_mode;
}

static void gl_transform_projection_matrix(struct ngpu_ctx *s, float *dst)
{
}

static void gl_get_rendertarget_uvcoord_matrix(struct ngpu_ctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    memcpy(dst, matrix, 4 * 4 * sizeof(float));
}

static struct ngpu_rendertarget *gl_get_default_rendertarget(struct ngpu_ctx *s, enum ngpu_load_op load_op)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    switch (load_op) {
    case NGPU_LOAD_OP_DONT_CARE:
    case NGPU_LOAD_OP_CLEAR:
        return s_priv->default_rt;
    case NGPU_LOAD_OP_LOAD:
        return s_priv->default_rt_load;
    default:
        ngli_assert(0);
    }
}

static const struct ngpu_rendertarget_layout *gl_get_default_rendertarget_layout(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    return &s_priv->default_rt_layout;
}

static void gl_get_default_rendertarget_size(struct ngpu_ctx *s, int32_t *width, int32_t *height)
{
    *width = s->config.width;
    *height = s->config.height;
}

static void gl_begin_render_pass(struct ngpu_ctx *s, struct ngpu_rendertarget *rt)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    NGLI_CMD_BUFFER_GL_CMD_REF(cmd_buffer, rt);

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_BEGIN_RENDER_PASS,
                                            .begin_render_pass.rendertarget = rt,
                                        });
}

static void gl_end_render_pass(struct ngpu_ctx *s)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_END_RENDER_PASS,
                                        });
}

static void gl_set_viewport(struct ngpu_ctx *s, const struct ngpu_viewport *viewport)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_SET_VIEWPORT,
                                            .set_viewport.viewport = *viewport,
                                        });
}

static void gl_set_scissor(struct ngpu_ctx *s, const struct ngpu_scissor *scissor)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_SET_SCISSOR,
                                            .set_scissor.scissor = *scissor,
                                        });
}

static enum ngpu_format gl_get_preferred_depth_format(struct ngpu_ctx *s)
{
    return NGPU_FORMAT_D16_UNORM;
}

static enum ngpu_format gl_get_preferred_depth_stencil_format(struct ngpu_ctx *s)
{
    return NGPU_FORMAT_D24_UNORM_S8_UINT;
}

static uint32_t gl_get_format_features(struct ngpu_ctx *s, enum ngpu_format format)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    const struct ngpu_format_gl *format_gl = ngpu_format_get_gl_texture_format(gl, format);
    return format_gl->features;
}

static void gl_generate_texture_mipmap(struct ngpu_ctx *s, struct ngpu_texture *texture)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    NGLI_CMD_BUFFER_GL_CMD_REF(cmd_buffer, texture);

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_GENERATE_TEXTURE_MIPMAP,
                                            .generate_texture_mipmap.texture = texture,
                                        });
}

static void gl_set_bindgroup(struct ngpu_ctx *s, struct ngpu_bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    NGLI_CMD_BUFFER_GL_CMD_REF(cmd_buffer, bindgroup);

    struct ngpu_bindgroup_gl *bindgroup_gl = (struct ngpu_bindgroup_gl *)bindgroup;
    for (size_t i = 0; i < ngli_darray_count(&bindgroup_gl->buffer_bindings); i++) {
        struct buffer_binding_gl *binding = ngli_darray_get(&bindgroup_gl->buffer_bindings, i);
        ngpu_cmd_buffer_gl_ref_buffer(cmd_buffer, (struct ngpu_buffer *)binding->buffer);
    }

    struct ngpu_cmd_gl cmd_gl = {
        .type = NGPU_CMD_TYPE_GL_SET_BINDGROUP,
        .set_bindgroup.bindgroup = bindgroup,
    };
    memcpy(cmd_gl.set_bindgroup.offsets, offsets, nb_offsets * sizeof(*offsets));
    cmd_gl.set_bindgroup.nb_offsets = nb_offsets;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &cmd_gl);
}

static void gl_set_pipeline(struct ngpu_ctx *s, struct ngpu_pipeline *pipeline)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    NGLI_CMD_BUFFER_GL_CMD_REF(cmd_buffer, pipeline);

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_SET_PIPELINE,
                                            .set_pipeline.pipeline = pipeline,
                                        });
}

static void gl_draw(struct ngpu_ctx *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_DRAW,
                                            .draw.nb_vertices = nb_vertices,
                                            .draw.nb_instances = nb_instances,
                                            .draw.first_vertex = first_vertex,
                                        });
}

static void gl_draw_indexed(struct ngpu_ctx *s, uint32_t nb_indices, uint32_t nb_instances)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_DRAW_INDEXED,
                                            .draw_indexed.nb_indices = nb_indices,
                                            .draw_indexed.nb_instances = nb_instances,
                                        });
}

static void gl_dispatch(struct ngpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_DISPATCH,
                                            .dispatch.nb_group_x = nb_group_x,
                                            .dispatch.nb_group_y = nb_group_y,
                                            .dispatch.nb_group_z = nb_group_z,
                                        });
}

static void gl_set_vertex_buffer(struct ngpu_ctx *s, uint32_t index, const struct ngpu_buffer *buffer)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    NGLI_CMD_BUFFER_GL_CMD_REF(cmd_buffer, buffer);

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_SET_VERTEX_BUFFER,
                                            .set_vertex_buffer.index = index,
                                            .set_vertex_buffer.buffer = buffer,
                                        });
}

static void gl_set_index_buffer(struct ngpu_ctx *s, const struct ngpu_buffer *buffer, enum ngpu_format format)
{
    struct ngpu_ctx_gl *s_priv = (struct ngpu_ctx_gl *)s;
    struct ngpu_cmd_buffer_gl *cmd_buffer = s_priv->cur_cmd_buffer;

    NGLI_CMD_BUFFER_GL_CMD_REF(cmd_buffer, buffer);

    ngpu_cmd_buffer_gl_push(cmd_buffer, &(struct ngpu_cmd_gl){
                                            .type = NGPU_CMD_TYPE_GL_SET_INDEX_BUFFER,
                                            .set_index_buffer.buffer = buffer,
                                            .set_index_buffer.format = format,
                                        });
}

#define DECLARE_GPU_CTX_CLASS(cls_suffix, cls_id)                                \
const struct ngpu_ctx_class ngpu_ctx_##cls_suffix = {                            \
    .id                                 = cls_id,                                \
    .create                             = gl_create,                             \
    .init                               = gl_init,                               \
    .resize                             = gl_resize,                             \
    .set_capture_buffer                 = gl_set_capture_buffer,                 \
    .begin_update                       = gl_begin_update,                       \
    .end_update                         = gl_end_update,                         \
    .begin_draw                         = gl_begin_draw,                         \
    .end_draw                           = gl_end_draw,                           \
    .query_draw_time                    = gl_query_draw_time,                    \
    .wait_idle                          = gl_wait_idle,                          \
    .destroy                            = gl_destroy,                            \
                                                                                 \
    .transform_cull_mode                = gl_transform_cull_mode,                \
    .transform_projection_matrix        = gl_transform_projection_matrix,        \
    .get_rendertarget_uvcoord_matrix    = gl_get_rendertarget_uvcoord_matrix,    \
                                                                                 \
    .get_default_rendertarget           = gl_get_default_rendertarget,           \
    .get_default_rendertarget_layout    = gl_get_default_rendertarget_layout,    \
    .get_default_rendertarget_size      = gl_get_default_rendertarget_size,      \
                                                                                 \
    .begin_render_pass                  = gl_begin_render_pass,                  \
    .end_render_pass                    = gl_end_render_pass,                    \
                                                                                 \
    .set_viewport                       = gl_set_viewport,                       \
    .set_scissor                        = gl_set_scissor,                        \
                                                                                 \
    .get_preferred_depth_format         = gl_get_preferred_depth_format,         \
    .get_preferred_depth_stencil_format = gl_get_preferred_depth_stencil_format, \
    .get_format_features                = gl_get_format_features,                \
                                                                                 \
    .generate_texture_mipmap            = gl_generate_texture_mipmap,            \
                                                                                 \
    .set_bindgroup                      = gl_set_bindgroup,                      \
                                                                                 \
    .set_pipeline                       = gl_set_pipeline,                       \
    .draw                               = gl_draw,                               \
    .draw_indexed                       = gl_draw_indexed,                       \
    .dispatch                           = gl_dispatch,                           \
                                                                                 \
    .set_vertex_buffer                  = gl_set_vertex_buffer,                  \
    .set_index_buffer                   = gl_set_index_buffer,                   \
                                                                                 \
    .buffer_create                      = ngpu_buffer_gl_create,                 \
    .buffer_init                        = ngpu_buffer_gl_init,                   \
    .buffer_wait                        = ngpu_buffer_gl_wait,                   \
    .buffer_upload                      = ngpu_buffer_gl_upload,                 \
    .buffer_map                         = ngpu_buffer_gl_map,                    \
    .buffer_unmap                       = ngpu_buffer_gl_unmap,                  \
    .buffer_freep                       = ngpu_buffer_gl_freep,                  \
                                                                                 \
    .bindgroup_layout_create            = ngpu_bindgroup_layout_gl_create,       \
    .bindgroup_layout_init              = ngpu_bindgroup_layout_gl_init,         \
    .bindgroup_layout_freep             = ngpu_bindgroup_layout_gl_freep,        \
                                                                                 \
    .bindgroup_create                   = ngpu_bindgroup_gl_create,              \
    .bindgroup_init                     = ngpu_bindgroup_gl_init,                \
    .bindgroup_update_texture           = ngpu_bindgroup_gl_update_texture,      \
    .bindgroup_update_buffer            = ngpu_bindgroup_gl_update_buffer,       \
    .bindgroup_freep                    = ngpu_bindgroup_gl_freep,               \
                                                                                 \
    .pipeline_create                    = ngpu_pipeline_gl_create,               \
    .pipeline_init                      = ngpu_pipeline_gl_init,                 \
    .pipeline_freep                     = ngpu_pipeline_gl_freep,                \
                                                                                 \
    .program_create                     = ngpu_program_gl_create,                \
    .program_init                       = ngpu_program_gl_init,                  \
    .program_freep                      = ngpu_program_gl_freep,                 \
                                                                                 \
    .rendertarget_create                = ngpu_rendertarget_gl_create,           \
    .rendertarget_init                  = ngpu_rendertarget_gl_init,             \
    .rendertarget_freep                 = ngpu_rendertarget_gl_freep,            \
                                                                                 \
    .texture_create                     = ngpu_texture_gl_create,                \
    .texture_init                       = ngpu_texture_gl_init,                  \
    .texture_upload                     = ngpu_texture_gl_upload,                \
    .texture_upload_with_params         = ngpu_texture_gl_upload_with_params,    \
    .texture_generate_mipmap            = ngpu_texture_gl_generate_mipmap,       \
    .texture_freep                      = ngpu_texture_gl_freep,                 \
}                                                                                \

#ifdef BACKEND_GL
DECLARE_GPU_CTX_CLASS(gl,   NGL_BACKEND_OPENGL);
#endif
#ifdef BACKEND_GLES
DECLARE_GPU_CTX_CLASS(gles, NGL_BACKEND_OPENGLES);
#endif
