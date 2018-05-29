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
#include <stdio.h>

#if defined(TARGET_ANDROID)
#include <jni.h>
#include <pthread.h>

#include "jni_utils.h"
#endif

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "glcontext.h"

struct ngl_ctx *ngl_create(void)
{
    struct ngl_ctx *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    LOG(INFO, "Context create in node.gl v%d.%d.%d",
        NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    return s;
}

int ngl_set_glcontext(struct ngl_ctx *s, void *display, void *window, void *handle, int platform, int api)
{
    if (s->configured) {
        LOG(ERROR, "Context is already configured");
        return -1;
    }

    s->glcontext = ngli_glcontext_new_wrapped(display, window, handle, platform, api);
    if (!s->glcontext)
        return -1;

    int ret = ngli_glcontext_load_extensions(s->glcontext);
    if (ret < 0)
        return ret;

    const struct glfunctions *gl = &s->glcontext->funcs;
    s->glstate = ngli_glstate_create(gl);
    if (!s->glstate)
        return -1;

    s->configured = 1;
    return 0;
}

int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene)
{
    if (!s->configured) {
        LOG(ERROR, "Context must be configured before setting a scene");
        return -1;
    }

    if (s->scene) {
        ngli_node_detach_ctx(s->scene);
        ngl_node_unrefp(&s->scene);
    }

    if (!scene)
        return 0;

    int ret = ngli_node_attach_ctx(scene, s);
    if (ret < 0)
        return ret;

    s->scene = ngl_node_ref(scene);
    return 0;
}

int ngli_prepare_draw(struct ngl_ctx *s, double t)
{
    struct glcontext *glcontext = s->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    if (!glcontext->loaded) {
        LOG(ERROR, "glcontext not loaded");
        return -1;
    }

    struct ngl_node *scene = s->scene;
    if (!scene) {
        LOG(ERROR, "scene is not set, can not draw");
        return -1;
    }

    LOG(DEBUG, "prepare scene %s @ t=%f", scene->name, t);

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    int ret = ngli_node_visit(scene, 1, t);
    if (ret < 0)
        return ret;

    ret = ngli_node_honor_release_prefetch(scene, t);
    if (ret < 0)
        return ret;

    ret = ngli_node_update(scene, t);
    if (ret < 0)
        return ret;

    return 0;
}

int ngl_draw(struct ngl_ctx *s, double t)
{
    int ret = ngli_prepare_draw(s, t);
    if (ret < 0)
        goto end;

    LOG(DEBUG, "draw scene %s @ t=%f", s->scene->name, t);
    ngli_node_draw(s->scene);

end:
    if (ret == 0 && ngli_glcontext_check_gl_error(s->glcontext))
        ret = -1;
    return ret;
}

void ngl_free(struct ngl_ctx **ss)
{
    struct ngl_ctx *s = *ss;

    if (!s)
        return;

    ngl_set_scene(s, NULL);
    ngli_glcontext_freep(&s->glcontext);
    ngli_glstate_freep(&s->glstate);
    free(*ss);
    *ss = NULL;
}

#if defined(TARGET_ANDROID)
static void *java_vm;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int ngl_jni_set_java_vm(void *vm)
{
    int ret = 0;

    pthread_mutex_lock(&lock);
    if (java_vm == NULL) {
        java_vm = vm;
    } else if (java_vm != vm) {
        ret = -1;
        LOG(ERROR, "A Java virtual machine has already been set");
    }
    pthread_mutex_unlock(&lock);

    return ret;
}

void *ngl_jni_get_java_vm(void)
{
    void *vm;

    pthread_mutex_lock(&lock);
    vm = java_vm;
    pthread_mutex_unlock(&lock);

    return vm;
}

static void *android_application_context;

int ngl_android_set_application_context(void *application_context)
{
    JNIEnv *env;

    env = ngli_jni_get_env();
    if (!env)
        return -1;

    pthread_mutex_lock(&lock);

    if (android_application_context) {
        (*env)->DeleteGlobalRef(env, android_application_context);
        android_application_context = NULL;
    }

    if (application_context)
        android_application_context = (*env)->NewGlobalRef(env, application_context);

    pthread_mutex_unlock(&lock);

    return 0;
}

void *ngl_android_get_application_context(void)
{
    void *context;

    pthread_mutex_lock(&lock);
    context = android_application_context;
    pthread_mutex_unlock(&lock);

    return context;
}

#else
int ngl_jni_set_java_vm(void *vm)
{
    return -1;
}

void *ngl_jni_get_java_vm(void)
{
    return NULL;
}

int ngl_android_set_application_context(void *application_context)
{
    return -1;
}

void *ngl_android_get_application_context(void)
{
    return NULL;
}
#endif
