/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2016-2022 GoPro Inc.
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "glcontext.h"
#include "glincludes.h"
#include "log.h"
#include "ngpu/limits.h"
#include "nopegl.h"
#include "utils/bstr.h"
#include "utils/memory.h"
#include "utils/utils.h"

#include "gldefinitions_data.h"
#include "glfeatures_data.h"

#ifdef HAVE_GLPLATFORM_EGL
#include "egl.h"
#endif

NGLI_STATIC_ASSERT(sizeof(GLfloat)  == sizeof(float),          "GLfloat size");
NGLI_STATIC_ASSERT(sizeof(GLbyte)   == sizeof(char),           "GLbyte size");
NGLI_STATIC_ASSERT(sizeof(GLshort)  == sizeof(short),          "GLshort size");
NGLI_STATIC_ASSERT(sizeof(GLint)    == sizeof(int),            "GLint size");
NGLI_STATIC_ASSERT(sizeof(GLubyte)  == sizeof(unsigned char),  "GLubyte size");
NGLI_STATIC_ASSERT(sizeof(GLushort) == sizeof(unsigned short), "GLushort size");
NGLI_STATIC_ASSERT(sizeof(GLuint)   == sizeof(unsigned int),   "GLuint size");
NGLI_STATIC_ASSERT(GL_FALSE == 0 && GL_TRUE == 1,              "GLboolean values");

enum {
    GLPLATFORM_EGL,
    GLPLATFORM_NSGL,
    GLPLATFORM_EAGL,
    GLPLATFORM_WGL,
};

extern const struct glcontext_class ngli_glcontext_egl_class;
extern const struct glcontext_class ngli_glcontext_egl_external_class;
extern const struct glcontext_class ngli_glcontext_nsgl_class;
extern const struct glcontext_class ngli_glcontext_nsgl_external_class;
extern const struct glcontext_class ngli_glcontext_eagl_class;
extern const struct glcontext_class ngli_glcontext_eagl_external_class;
extern const struct glcontext_class ngli_glcontext_wgl_class;
extern const struct glcontext_class ngli_glcontext_wgl_external_class;

static const struct {
    const struct glcontext_class *cls;
    const struct glcontext_class *external_cls;
} glcontext_class_map[] = {
#ifdef HAVE_GLPLATFORM_EGL
    [GLPLATFORM_EGL] = {
        .cls = &ngli_glcontext_egl_class,
        .external_cls = &ngli_glcontext_egl_external_class,
    },
#endif
#ifdef HAVE_GLPLATFORM_NSGL
    [GLPLATFORM_NSGL] = {
        .cls = &ngli_glcontext_nsgl_class,
        .external_cls = &ngli_glcontext_nsgl_external_class,
    },
#endif
#ifdef HAVE_GLPLATFORM_EAGL
    [GLPLATFORM_EAGL] = {
        .cls = &ngli_glcontext_eagl_class,
        .external_cls = &ngli_glcontext_eagl_external_class,
    },
#endif
#ifdef HAVE_GLPLATFORM_WGL
    [GLPLATFORM_WGL] = {
        .cls = &ngli_glcontext_wgl_class,
        .external_cls = &ngli_glcontext_wgl_external_class,
    },
#endif
};

static const int platform_to_glplatform[] = {
    [NGL_PLATFORM_XLIB]    = GLPLATFORM_EGL,
    [NGL_PLATFORM_ANDROID] = GLPLATFORM_EGL,
    [NGL_PLATFORM_MACOS]   = GLPLATFORM_NSGL,
    [NGL_PLATFORM_IOS]     = GLPLATFORM_EAGL,
    [NGL_PLATFORM_WINDOWS] = GLPLATFORM_WGL,
    [NGL_PLATFORM_WAYLAND] = GLPLATFORM_EGL,
};

static const char * const backend_names[] = {
    [NGL_BACKEND_AUTO]     = "NGL_BACKEND_AUTO",
    [NGL_BACKEND_OPENGL]   = "NGL_BACKEND_OPENGL",
    [NGL_BACKEND_OPENGLES] = "NGL_BACKEND_OPENGLES",
};

static int glcontext_load_functions(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    for (size_t i = 0; i < NGLI_ARRAY_NB(gldefinitions); i++) {
        void *func;
        const struct gldefinition *gldefinition = &gldefinitions[i];

        func = ngli_glcontext_get_proc_address(glcontext, gldefinition->name);
        if ((gldefinition->flags & M) && !func) {
            LOG(ERROR, "could not find core function: %s", gldefinition->name);
            return NGL_ERROR_NOT_FOUND;
        }

        *(void **)((uintptr_t)gl + gldefinition->offset) = func;
    }

    return 0;
}

static int glcontext_probe_version(struct glcontext *glcontext)
{
    GLint major_version = 0;
    GLint minor_version = 0;

    const char *gl_version = (const char *)glcontext->funcs.GetString(GL_VERSION);
    if (!gl_version) {
        LOG(ERROR, "could not get OpenGL version");
        return NGL_ERROR_BUG;
    }

    const char *es_prefix = "OpenGL ES";
    const int es = !strncmp(es_prefix, gl_version, strlen(es_prefix));
    const int backend = es ? NGL_BACKEND_OPENGLES : NGL_BACKEND_OPENGL;
    if (glcontext->backend != backend) {
        LOG(ERROR, "OpenGL context (%s) does not match requested backend (%s)",
            backend_names[backend], backend_names[glcontext->backend]);
        return NGL_ERROR_INVALID_USAGE;
    }

    if (glcontext->backend == NGL_BACKEND_OPENGL) {
        glcontext->funcs.GetIntegerv(GL_MAJOR_VERSION, &major_version);
        glcontext->funcs.GetIntegerv(GL_MINOR_VERSION, &minor_version);
    } else if (glcontext->backend == NGL_BACKEND_OPENGLES) {
        int ret = sscanf(gl_version,
                         "OpenGL ES %d.%d",
                         &major_version,
                         &minor_version);
        if (ret != 2) {
            LOG(ERROR, "could not parse OpenGL ES version: \"%s\"", gl_version);
            return NGL_ERROR_BUG;
        }
    } else {
        ngli_assert(0);
    }

    LOG(INFO, "OpenGL version: %d.%d %s",
        major_version,
        minor_version,
        glcontext->backend == NGL_BACKEND_OPENGLES ? "ES " : "");

    const char *renderer = (const char *)glcontext->funcs.GetString(GL_RENDERER);
    if (!renderer) {
        LOG(ERROR, "could not get OpenGL renderer");
        return NGL_ERROR_BUG;
    }
    LOG(INFO, "OpenGL renderer: %s", renderer);

    if (strstr(renderer, "llvmpipe") || // Mesa llvmpipe
        strstr(renderer, "softpipe") || // Mesa softpipe
        strstr(renderer, "SWR")) {      // Mesa swrast
        glcontext->features |= NGLI_FEATURE_GL_SOFTWARE;
        LOG(INFO, "software renderer detected");
    }

    glcontext->version = major_version * 100 + minor_version * 10;

    if (glcontext->backend == NGL_BACKEND_OPENGL && glcontext->version < 330) {
        LOG(ERROR, "nope.gl only supports OpenGL >= 3.3");
        return NGL_ERROR_UNSUPPORTED;
    } else if (glcontext->backend == NGL_BACKEND_OPENGLES && glcontext->version < 300) {
        LOG(ERROR, "nope.gl only supports OpenGL ES >= 3.0");
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

static int glcontext_probe_glsl_version(struct glcontext *glcontext)
{
    if (glcontext->backend == NGL_BACKEND_OPENGL) {
        const char *glsl_version = (const char *)glcontext->funcs.GetString(GL_SHADING_LANGUAGE_VERSION);
        if (!glsl_version) {
            LOG(ERROR, "could not get GLSL version");
            return NGL_ERROR_BUG;
        }

        int major_version;
        int minor_version;
        int ret = sscanf(glsl_version, "%d.%d", &major_version, &minor_version);
        if (ret != 2) {
            LOG(ERROR, "could not parse GLSL version: \"%s\"", glsl_version);
            return NGL_ERROR_BUG;
        }
        glcontext->glsl_version = major_version * 100 + minor_version;
    } else if (glcontext->backend == NGL_BACKEND_OPENGLES) {
        glcontext->glsl_version = glcontext->version;
    } else {
        ngli_assert(0);
    }

    return 0;
}

static int glcontext_check_extension(const char *extension,
                                     const struct glcontext *glcontext)
{
    GLint nb_extensions;
    glcontext->funcs.GetIntegerv(GL_NUM_EXTENSIONS, &nb_extensions);

    for (GLint i = 0; i < nb_extensions; i++) {
        const char *tmp = (const char *)glcontext->funcs.GetStringi(GL_EXTENSIONS, (GLuint)i);
        if (!tmp)
            break;
        if (!strcmp(extension, tmp))
            return 1;
    }

    return 0;
}

static int glcontext_check_extensions(struct glcontext *glcontext,
                                      const char **extensions)
{
    if (!extensions || !*extensions)
        return 0;

    if (glcontext->backend == NGL_BACKEND_OPENGLES) {
        const char *gl_extensions = (const char *)glcontext->funcs.GetString(GL_EXTENSIONS);
        while (*extensions) {
            if (!ngli_glcontext_check_extension(*extensions, gl_extensions))
                return 0;

            extensions++;
        }
    } else if (glcontext->backend == NGL_BACKEND_OPENGL) {
        while (*extensions) {
            if (!glcontext_check_extension(*extensions, glcontext))
                return 0;

            extensions++;
        }
    } else {
        ngli_assert(0);
    }

    return 1;
}

static int glcontext_check_functions(struct glcontext *glcontext,
                                     const size_t *funcs_offsets)
{
    const struct glfunctions *gl = &glcontext->funcs;

    if (!funcs_offsets)
        return 1;

    while (*funcs_offsets != -1) {
        void *func_ptr = *(void **)((uintptr_t)gl + *funcs_offsets);
        if (!func_ptr)
            return 0;
        funcs_offsets++;
    }

    return 1;
}

static int glcontext_probe_extensions(struct glcontext *glcontext)
{
    const int es = glcontext->backend == NGL_BACKEND_OPENGLES;
    struct bstr *features_str = ngli_bstr_create();

    if (!features_str)
        return NGL_ERROR_MEMORY;

    for (size_t i = 0; i < NGLI_ARRAY_NB(glfeatures); i++) {
        const struct glfeature *glfeature = &glfeatures[i];

        const char **extensions = es ? glfeature->es_extensions : glfeature->extensions;
        ngli_assert(!extensions || *extensions);

        int version = es ? glfeature->es_version : glfeature->version;
        if (!version && !extensions)
            continue;

        if (!version || glcontext->version < version) {
            if (!glcontext_check_extensions(glcontext, extensions))
                continue;
        }

        if (!glcontext_check_functions(glcontext, glfeature->funcs_offsets))
            continue;

        ngli_bstr_printf(features_str, " %s", glfeature->name);
        glcontext->features |= glfeature->flag;
    }

    LOG(INFO, "OpenGL%s features:%s", es ? " ES" : "", ngli_bstr_strptr(features_str));
    ngli_bstr_freep(&features_str);

    return 0;
}

#define GET(name, value) do {                         \
    GLint gl_value;                                   \
    glcontext->funcs.GetIntegerv((name), &gl_value);  \
    *(value) = (__typeof__(*(value)))gl_value;        \
} while (0)                                           \

#define GET_I(name, index, value) do {                           \
    GLint gl_value;                                              \
    glcontext->funcs.GetIntegeri_v((name), (index), &gl_value);  \
    *(value) = (__typeof__((*value)))gl_value;                   \
} while (0)                                                      \

static int glcontext_probe_limits(struct glcontext *glcontext)
{
    struct ngpu_limits *limits = &glcontext->limits;

    GET(GL_MAX_VERTEX_ATTRIBS, &limits->max_vertex_attributes);
    limits->max_vertex_attributes = NGLI_MIN(limits->max_vertex_attributes, NGPU_MAX_VERTEX_BUFFERS);
    /*
     * macOS and iOS OpenGL drivers pass gl_VertexID and gl_InstanceID as
     * standard attributes and forget to count them in GL_MAX_VERTEX_ATTRIBS.
     */
    if (glcontext->platform == NGL_PLATFORM_MACOS || glcontext->platform == NGL_PLATFORM_IOS)
        limits->max_vertex_attributes -= 2;
    GET(GL_MAX_TEXTURE_IMAGE_UNITS, &limits->max_texture_image_units);
    GET(GL_MAX_TEXTURE_SIZE, &limits->max_texture_dimension_1d);
    GET(GL_MAX_TEXTURE_SIZE, &limits->max_texture_dimension_2d);
    GET(GL_MAX_3D_TEXTURE_SIZE, &limits->max_texture_dimension_3d);
    GET(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limits->max_texture_dimension_cube);
    GET(GL_MAX_ARRAY_TEXTURE_LAYERS, &limits->max_texture_array_layers);
    GET(GL_MAX_SAMPLES, &limits->max_samples);
    GET(GL_MAX_COLOR_ATTACHMENTS, &limits->max_color_attachments);
    limits->max_color_attachments = NGLI_MIN(limits->max_color_attachments, NGPU_MAX_COLOR_ATTACHMENTS);
    GET(GL_MAX_UNIFORM_BLOCK_SIZE, &limits->max_uniform_block_size);
    GET(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &limits->min_uniform_block_offset_alignment);

    if (glcontext->features & NGLI_FEATURE_GL_SHADER_STORAGE_BUFFER_OBJECT) {
        GET(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &limits->max_storage_block_size);
        GET(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &limits->min_storage_block_offset_alignment);
    }

    if (glcontext->features & NGLI_FEATURE_GL_SHADER_IMAGE_LOAD_STORE) {
        GET(GL_MAX_IMAGE_UNITS, &limits->max_image_units);
    }

    if (glcontext->features & NGLI_FEATURE_GL_COMPUTE_SHADER) {
        for (GLuint i = 0; i < (GLuint)NGLI_ARRAY_NB(limits->max_compute_work_group_count); i++) {
            GET_I(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &limits->max_compute_work_group_count[i]);
        }

        GET(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &limits->max_compute_work_group_invocations);

        for (GLuint i = 0; i < (GLuint)NGLI_ARRAY_NB(limits->max_compute_work_group_size); i++) {
            GET_I(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &limits->max_compute_work_group_size[i]);
        }

        GET(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &limits->max_compute_shared_memory_size);
    }

    GET(GL_MAX_DRAW_BUFFERS, &limits->max_draw_buffers);

    return 0;
}

static int glcontext_probe_formats(struct glcontext *glcontext)
{
    ngpu_format_gl_init(glcontext);

    return 0;
}

static int glcontext_check_driver(struct glcontext *glcontext)
{
    const char *gl_version = (const char *)glcontext->funcs.GetString(GL_VERSION);
    if (!gl_version) {
        LOG(ERROR, "could not get OpenGL version");
        return NGL_ERROR_BUG;
    }

    int mesa_version[3] = {0};
    const char *mesa = strstr(gl_version, "Mesa");
    if (mesa) {
        int ret = sscanf(mesa, "Mesa %d.%d.%d", &mesa_version[0], &mesa_version[1], &mesa_version[2]);
        if (ret != 3) {
            LOG(ERROR, "could not parse Mesa version: \"%s\"", mesa);
            return NGL_ERROR_BUG;
        }
        LOG(INFO, "Mesa version: %d.%d.%d", NGLI_ARG_VEC3(mesa_version));
    }

#ifdef HAVE_GLPLATFORM_EGL
    if (glcontext->features & NGLI_FEATURE_GL_EGL_MESA_QUERY_DRIVER) {
        const char *driver_name = ngli_eglGetDisplayDriverName(glcontext);
        if (driver_name) {
            LOG(INFO, "EGL driver name: %s", driver_name);
            if (!strcmp(driver_name, "radeonsi"))
                glcontext->workaround_radeonsi_sync = 1;
        }
    }
#endif

    return 0;
}

static int glcontext_load_extensions(struct glcontext *glcontext)
{
    int ret = glcontext_load_functions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_version(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_glsl_version(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_extensions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_limits(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_formats(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_check_driver(glcontext);
    if (ret < 0)
        return ret;

    return 0;
}

struct glcontext *ngli_glcontext_create(const struct glcontext_params *params)
{
    if (params->platform < 0 || params->platform >= NGLI_ARRAY_NB(platform_to_glplatform))
        return NULL;

    const int glplatform = platform_to_glplatform[params->platform];
    if (glplatform < 0 || glplatform >= NGLI_ARRAY_NB(glcontext_class_map))
        return NULL;

    struct glcontext *glcontext = ngli_calloc(1, sizeof(*glcontext));
    if (!glcontext)
        return NULL;
    if (params->external) {
        glcontext->cls = glcontext_class_map[glplatform].external_cls;
    } else {
        glcontext->cls = glcontext_class_map[glplatform].cls;
    }

    if (glcontext->cls->priv_size) {
        glcontext->priv_data = ngli_calloc(1, glcontext->cls->priv_size);
        if (!glcontext->priv_data) {
            ngli_free(glcontext);
            return NULL;
        }
    }

    glcontext->platform = params->platform;
    glcontext->backend = params->backend;
    glcontext->external = params->external;
    glcontext->offscreen = params->offscreen;
    glcontext->width = params->width;
    glcontext->height = params->height;
    glcontext->samples = params->samples;
    glcontext->debug = params->debug;

    if (glcontext->cls->init) {
        int ret = glcontext->cls->init(glcontext, params->display, params->window, params->shared_ctx);
        if (ret < 0)
            goto fail;
    }

    int ret = ngli_glcontext_make_current(glcontext, 1);
    if (ret < 0)
        goto fail;

    ret = glcontext_load_extensions(glcontext);
    if (ret < 0)
        goto fail;

    if (glcontext->backend == NGL_BACKEND_OPENGL)
        glcontext->funcs.Enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    if (!glcontext->external && !glcontext->offscreen) {
        ret = ngli_glcontext_resize(glcontext, glcontext->width, glcontext->height);
        if (ret < 0)
            goto fail;
    }

    if (!params->external && params->swap_interval >= 0)
        ngli_glcontext_set_swap_interval(glcontext, params->swap_interval);

    return glcontext;
fail:
    ngli_glcontext_freep(&glcontext);
    return NULL;
}

int ngli_glcontext_make_current(struct glcontext *glcontext, int current)
{
    if (glcontext->cls->make_current)
        return glcontext->cls->make_current(glcontext, current);

    return 0;
}

int ngli_glcontext_set_swap_interval(struct glcontext *glcontext, int interval)
{
    if (glcontext->cls->set_swap_interval)
        return glcontext->cls->set_swap_interval(glcontext, interval);

    return 0;
}

void ngli_glcontext_swap_buffers(struct glcontext *glcontext)
{
    if (glcontext->cls->swap_buffers)
        glcontext->cls->swap_buffers(glcontext);
}

void ngli_glcontext_set_surface_pts(struct glcontext *glcontext, double t)
{
    if (glcontext->cls->set_surface_pts)
        glcontext->cls->set_surface_pts(glcontext, t);
}

int ngli_glcontext_resize(struct glcontext *glcontext, uint32_t width, uint32_t height)
{
    if (glcontext->offscreen) {
        LOG(ERROR, "offscreen context does not support resize operation");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (glcontext->external) {
        LOG(ERROR, "external context does not support resize operation");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (glcontext->cls->resize)
        return glcontext->cls->resize(glcontext, width, height);

    return NGL_ERROR_UNSUPPORTED;
}

void ngli_glcontext_freep(struct glcontext **glcontextp)
{
    struct glcontext *glcontext;

    if (!glcontextp || !*glcontextp)
        return;

    glcontext = *glcontextp;

    if (glcontext->cls->uninit)
        glcontext->cls->uninit(glcontext);

    ngli_free(glcontext->priv_data);
    ngli_freep(glcontextp);
}

void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name)
{
    void *ptr = NULL;

    if (glcontext->cls->get_proc_address)
        ptr = glcontext->cls->get_proc_address(glcontext, name);

    return ptr;
}

void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext)
{
    void *texture_cache = NULL;

    if (glcontext->cls->get_texture_cache)
        texture_cache = glcontext->cls->get_texture_cache(glcontext);

    return texture_cache;
}

uintptr_t ngli_glcontext_get_display(struct glcontext *glcontext)
{
    uintptr_t handle = 0;

    if (glcontext->cls->get_display)
        handle = glcontext->cls->get_display(glcontext);

    return handle;
}

uintptr_t ngli_glcontext_get_handle(struct glcontext *glcontext)
{
    uintptr_t handle = 0;

    if (glcontext->cls->get_handle)
        handle = glcontext->cls->get_handle(glcontext);

    return handle;
}

GLuint ngli_glcontext_get_default_framebuffer(struct glcontext *glcontext)
{
    GLuint fbo_id = 0;

    if (glcontext->cls->get_default_framebuffer)
        fbo_id = glcontext->cls->get_default_framebuffer(glcontext);

    return fbo_id;
}

int ngli_glcontext_check_extension(const char *extension, const char *extensions)
{
    if (!extension || !extensions)
        return 0;

    size_t len = strlen(extension);
    const char *cur = extensions;
    const char *end = extensions + strlen(extensions);

    while (cur < end) {
        cur = strstr(extensions, extension);
        if (!cur)
            break;

        cur += len;
        if (cur[0] == ' ' || cur[0] == '\0')
            return 1;
    }

    return 0;
}

#define GL_ERROR_STR_CASE(error) case error: error_str = #error; break

int ngli_glcontext_check_gl_error(const struct glcontext *glcontext, const char *context)
{
    const GLenum error = glcontext->funcs.GetError();
    if (!error)
        return 0;

    const char *error_str = NULL;
    switch (error) {
    GL_ERROR_STR_CASE(GL_INVALID_ENUM);
    GL_ERROR_STR_CASE(GL_INVALID_VALUE);
    GL_ERROR_STR_CASE(GL_INVALID_OPERATION);
    GL_ERROR_STR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
    GL_ERROR_STR_CASE(GL_OUT_OF_MEMORY);
    default:
        error_str = "unknown error";
    }

    LOG(ERROR, "%s: GL error: %s (0x%04x)", context, error_str, error);

#if DEBUG_GL
    ngli_assert(0);
#endif

    return NGL_ERROR_GRAPHICS_GENERIC;
}
