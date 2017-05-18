/*
 * Copyright 2016 GoPro Inc.
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

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "utils.h"
#include "glincludes.h"

#include "gldefinitions_data.h"

#ifdef HAVE_PLATFORM_GLX
extern const struct glcontext_class ngli_glcontext_x11_class;
#endif

#ifdef HAVE_PLATFORM_EGL
extern const struct glcontext_class ngli_glcontext_egl_class;
#endif

#ifdef HAVE_PLATFORM_CGL
extern const struct glcontext_class ngli_glcontext_cgl_class;
#endif

#ifdef HAVE_PLATFORM_EAGL
extern const struct glcontext_class ngli_glcontext_eagl_class;
#endif

#ifdef HAVE_PLATFORM_WGL
extern const struct glcontext_class ngli_glcontext_wgl_class;
#endif

static const struct glcontext_class *glcontext_class_map[] = {
#ifdef HAVE_PLATFORM_GLX
    [NGL_GLPLATFORM_GLX] = &ngli_glcontext_x11_class,
#endif
#ifdef HAVE_PLATFORM_EGL
    [NGL_GLPLATFORM_EGL] = &ngli_glcontext_egl_class,
#endif
#ifdef HAVE_PLATFORM_CGL
    [NGL_GLPLATFORM_CGL] = &ngli_glcontext_cgl_class,
#endif
#ifdef HAVE_PLATFORM_EAGL
    [NGL_GLPLATFORM_EAGL] = &ngli_glcontext_eagl_class,
#endif
#ifdef HAVE_PLATFORM_WGL
    [NGL_GLPLATFORM_WGL] = &ngli_glcontext_wgl_class,
#endif
};

static struct glcontext *glcontext_new(void *display, void *window, void *handle, int platform, int api)
{
    struct glcontext *glcontext = NULL;

    if (platform < 0 || platform >= NGLI_ARRAY_NB(glcontext_class_map))
        return NULL;

    glcontext = calloc(1, sizeof(*glcontext));
    if (!glcontext)
        return NULL;

    glcontext->class = glcontext_class_map[platform];
    if (glcontext->class->priv_size) {
        glcontext->priv_data = calloc(1, glcontext->class->priv_size);
        if (!glcontext->priv_data) {
            goto fail;
        }
    }

    glcontext->platform = platform;
    glcontext->api = api;

    if (glcontext->class->init) {
        int ret = glcontext->class->init(glcontext, display, window, handle);
        if (ret < 0)
            goto fail;
    }

    return glcontext;
fail:
    ngli_glcontext_freep(&glcontext);
    return NULL;
}

struct glcontext *ngli_glcontext_new_wrapped(void *display, void *window, void *handle, int platform, int api)
{
    struct glcontext *glcontext;

    if (platform == NGL_GLPLATFORM_AUTO) {
#if defined(TARGET_LINUX)
        platform = NGL_GLPLATFORM_GLX;
#elif defined(TARGET_IPHONE)
        platform = NGL_GLPLATFORM_EAGL;
#elif defined(TARGET_DARWIN)
        platform = NGL_GLPLATFORM_CGL;
#elif defined(TARGET_ANDROID)
        platform = NGL_GLPLATFORM_EGL;
#elif defined(TARGET_MINGW_W64)
        platform = NGL_GLPLATFORM_WGL;
#else
        LOG(ERROR, "Can not determine which GL platform to use");
        return NULL;
#endif
    }

    if (api == NGL_GLAPI_AUTO) {
#if defined(TARGET_IPHONE) || defined(TARGET_ANDROID)
        api = NGL_GLAPI_OPENGLES2;
#else
        api = NGL_GLAPI_OPENGL3;
#endif
    }

    glcontext = glcontext_new(display, window, handle, platform, api);
    if (!glcontext)
        return NULL;

    glcontext->wrapped = 1;

    return glcontext;
}

struct glcontext *ngli_glcontext_new_shared(struct glcontext *other)
{
    struct glcontext *glcontext;

    if (!other)
        return NULL;

    void *display = other->class->get_display(other);
    void *window  = other->class->get_window(other);
    void *handle  = other->class->get_handle(other);

    glcontext = glcontext_new(display, window, handle, other->platform, other->api);

    if (glcontext->class->create) {
        int ret = glcontext->class->create(glcontext, other);
        if (ret < 0)
            ngli_glcontext_freep(&glcontext);
    }

    return glcontext;
}

int ngli_glcontext_load_extensions(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    if (glcontext->loaded)
        return 0;

    for (int i = 0; i < NGLI_ARRAY_NB(gldefinitions); i++) {
        void *func;
        const struct gldefinition *gldefinition = &gldefinitions[i];

        func = ngli_glcontext_get_proc_address(glcontext, gldefinition->name);
        if ((gldefinition->flags & M) && !func) {
            LOG(ERROR, "could not find core function: %s", gldefinition->name);
            return -1;
        }

        *(void **)((uint8_t *)gl + gldefinition->offset) = func;
    }

    if (glcontext->api == NGL_GLAPI_OPENGL3) {
        GLint i, nb_extensions;

        ngli_glGetIntegerv(gl, GL_MAJOR_VERSION, &glcontext->major_version);
        ngli_glGetIntegerv(gl, GL_MINOR_VERSION, &glcontext->minor_version);

        if (glcontext->major_version >= 4)
            glcontext->has_vao_compatibility = 1;

        ngli_glGetIntegerv(gl, GL_NUM_EXTENSIONS, &nb_extensions);
        for (i = 0; i < nb_extensions; i++) {
            const char *extension = (const char *)ngli_glGetStringi(gl, GL_EXTENSIONS, i);
            if (!extension)
                break;
            if (!strcmp(extension, "GL_ARB_ES2_compatibility")) {
                glcontext->has_es2_compatibility = 1;
            } else if (!strcmp(extension, "GL_ARB_vertex_array_object")) {
                glcontext->has_vao_compatibility = 1;
            }
        }
    } else if (glcontext->api == NGL_GLAPI_OPENGLES2) {
        const char *gl_extensions = (const char *)ngli_glGetString(gl, GL_EXTENSIONS);
        glcontext->major_version = 2;
        glcontext->minor_version = 0;
        glcontext->has_es2_compatibility = 1;
        glcontext->has_vao_compatibility = ngli_glcontext_check_extension("GL_OES_vertex_array_object", gl_extensions);
    }

    ngli_glGetIntegerv(gl, GL_MAX_TEXTURE_IMAGE_UNITS, &glcontext->max_texture_image_units);

    if (glcontext->has_vao_compatibility) {
        glcontext->has_vao_compatibility =
            gl->GenVertexArrays != NULL &&
            gl->BindVertexArray != NULL &&
            gl->DeleteVertexArrays != NULL;
        if (!glcontext->has_vao_compatibility)
            LOG(WARNING, "OpenGL driver claims VAO support but we could not load related functions");

    }

    LOG(INFO, "OpenGL %d.%d ES2_compatibility=%d vertex_array_object=%d",
        glcontext->major_version,
        glcontext->minor_version,
        glcontext->has_es2_compatibility,
        glcontext->has_vao_compatibility);

    glcontext->loaded = 1;

    return 0;
}

int ngli_glcontext_make_current(struct glcontext *glcontext, int current)
{
    if (glcontext->class->make_current)
        return glcontext->class->make_current(glcontext, current);

    return 0;
}

void ngli_glcontext_swap_buffers(struct glcontext *glcontext)
{
    if (glcontext->class->swap_buffers)
        glcontext->class->swap_buffers(glcontext);
}

void ngli_glcontext_freep(struct glcontext **glcontextp)
{
    struct glcontext *glcontext;

    if (!glcontextp || !*glcontextp)
        return;

    glcontext = *glcontextp;

    if (glcontext->class->uninit)
        glcontext->class->uninit(glcontext);

    free(glcontext->priv_data);
    free(glcontext);

    *glcontextp = NULL;
}

void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name)
{
    void *ptr = NULL;

    if (glcontext->class->get_proc_address)
        ptr = glcontext->class->get_proc_address(glcontext, name);

    return ptr;
}

void *ngli_glcontext_get_handle(struct glcontext *glcontext)
{
    void *handle = NULL;

    if (glcontext->class->get_handle)
        handle = glcontext->class->get_handle(glcontext);

    return handle;
}

void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext)
{
    void *texture_cache = NULL;

    if (glcontext->class->get_texture_cache)
        texture_cache = glcontext->class->get_texture_cache(glcontext);

    return texture_cache;
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

int ngli_glcontext_check_gl_error(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    static const char * const errors[] = {
        [GL_INVALID_ENUM]                   = "GL_INVALID_ENUM",
        [GL_INVALID_VALUE]                  = "GL_INVALID_VALUE",
        [GL_INVALID_OPERATION]              = "GL_INVALID_OPERATION",
        [GL_INVALID_FRAMEBUFFER_OPERATION ] = "GL_INVALID_FRAMEBUFFER_OPERATION",
        [GL_OUT_OF_MEMORY]                  = "GL_OUT_OF_MEMORY",
    };
    const GLenum error = ngli_glGetError(gl);

    if (!error)
        return error;

    if (error < NGLI_ARRAY_NB(errors) && errors[error])
        LOG(ERROR, "GL error: %s", errors[error]);
    else
        LOG(ERROR, "GL error: %04x", error);

    return error;
}
