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

#include <stdlib.h>
#include <string.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "utils.h"
#include "gl_utils.h"

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
    if (glcontext->loaded)
        return 0;

    glcontext->glGetStringi         = ngli_glcontext_get_proc_address(glcontext, "glGetStringi");
    glcontext->glGenVertexArrays    = ngli_glcontext_get_proc_address(glcontext, "glGenVertexArrays");
    glcontext->glBindVertexArray    = ngli_glcontext_get_proc_address(glcontext, "glBindVertexArray");
    glcontext->glDeleteVertexArrays = ngli_glcontext_get_proc_address(glcontext, "glDeleteVertexArrays");

    if (glcontext->api == NGL_GLAPI_OPENGL3) {
        GLint i, nb_extensions;

        glGetIntegerv(GL_MAJOR_VERSION, &glcontext->major_version);
        glGetIntegerv(GL_MINOR_VERSION, &glcontext->minor_version);

        if (glcontext->major_version >= 4)
            glcontext->has_vao_compatibility = 1;

        glGetIntegerv(GL_NUM_EXTENSIONS, &nb_extensions);
        for (i = 0; i < nb_extensions; i++) {
            const char *extension = (const char *)glcontext->glGetStringi(GL_EXTENSIONS, i);
            if (!extension)
                break;
            if (!strcmp(extension, "GL_ARB_ES2_compatibility")) {
                glcontext->has_es2_compatibility = 1;
            } else if (!strcmp(extension, "GL_ARB_vertex_array_object")) {
                glcontext->has_vao_compatibility = 1;
            }
        }
    } else if (glcontext->api == NGL_GLAPI_OPENGLES2) {
        const char *gl_extensions = (const char *)glGetString(GL_EXTENSIONS);
        glcontext->major_version = 2;
        glcontext->minor_version = 0;
        glcontext->has_es2_compatibility = 1;
        glcontext->has_vao_compatibility = ngli_glcontext_check_extension("GL_OES_vertex_array_object", gl_extensions);
    }

    if (glcontext->has_vao_compatibility) {
        glcontext->has_vao_compatibility =
            glcontext->glGenVertexArrays != NULL &&
            glcontext->glBindVertexArray != NULL &&
            glcontext->glDeleteVertexArrays != NULL;
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
