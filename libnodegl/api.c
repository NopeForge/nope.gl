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
#include <pthread.h>
#include <stdio.h>

#if defined(TARGET_ANDROID)
#include <jni.h>

#include "jni_utils.h"
#endif

#include "backend.h"
#include "darray.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "rnode.h"

#if defined(TARGET_IPHONE) || defined(TARGET_ANDROID)
# define DEFAULT_BACKEND NGL_BACKEND_OPENGLES
#else
# define DEFAULT_BACKEND NGL_BACKEND_OPENGL
#endif

extern const struct backend ngli_backend_gl;
extern const struct backend ngli_backend_gles;

static const struct backend *backend_map[] = {
    [NGL_BACKEND_OPENGL]   = &ngli_backend_gl,
    [NGL_BACKEND_OPENGLES] = &ngli_backend_gles,
};

static int get_default_platform(void)
{
#if defined(TARGET_LINUX)
    return NGL_PLATFORM_XLIB;
#elif defined(TARGET_IPHONE)
    return NGL_PLATFORM_IOS;
#elif defined(TARGET_DARWIN)
    return NGL_PLATFORM_MACOS;
#elif defined(TARGET_ANDROID)
    return NGL_PLATFORM_ANDROID;
#elif defined(TARGET_MINGW_W64)
    return NGL_PLATFORM_WINDOWS;
#else
    return NGL_ERROR_UNSUPPORTED;
#endif
}

static int cmd_configure(struct ngl_ctx *s, void *arg)
{
    struct ngl_config *config = arg;

    if (s->scene)
        ngli_node_detach_ctx(s->scene, s);
    ngli_rnode_clear(&s->rnode);

    if (s->backend) {
        s->backend->destroy(s);
        s->backend = NULL;
    }

    if (config->backend == NGL_BACKEND_AUTO)
        config->backend = DEFAULT_BACKEND;

    if (config->backend < 0 ||
        config->backend >= NGLI_ARRAY_NB(backend_map) ||
        !backend_map[config->backend]) {
        LOG(ERROR, "unknown backend %d", config->backend);
        return NGL_ERROR_INVALID_ARG;
    }

    const struct backend *backend = backend_map[config->backend];
    LOG(INFO, "selected backend: %s", backend->name);

    if (config->platform == NGL_PLATFORM_AUTO)
        config->platform = get_default_platform();
    if (config->platform < 0) {
        LOG(ERROR, "can not determine which platform to use");
        return config->platform;
    }

    int ret = backend->configure(s, config);
    if (ret < 0) {
        LOG(ERROR, "unable to configure %s", backend->name);
        backend->destroy(s);
        return ret;
    }
    s->backend = backend;

    if (s->scene) {
        ret = ngli_node_attach_ctx(s->scene, s);
        if (ret < 0) {
            ngli_node_detach_ctx(s->scene, s);
            ngl_node_unrefp(&s->scene);
            s->backend->destroy(s);
            s->backend = NULL;
            return ret;
        }
    }

    return 0;
}

struct resize_params {
    int width;
    int height;
    const int *viewport;
};

static int cmd_resize(struct ngl_ctx *s, void *arg)
{
    const struct resize_params *params = arg;
    return s->backend->resize(s, params->width, params->height, params->viewport);
}

static int cmd_set_scene(struct ngl_ctx *s, void *arg)
{
    if (s->scene) {
        ngli_node_detach_ctx(s->scene, s);
        ngl_node_unrefp(&s->scene);
    }
    ngli_rnode_clear(&s->rnode);

    struct ngl_node *scene = arg;
    if (!scene)
        return 0;

    int ret = ngli_node_attach_ctx(scene, s);
    if (ret < 0) {
        ngli_node_detach_ctx(scene, s);
        return ret;
    }

    s->scene = ngl_node_ref(scene);
    return 0;
}

static int cmd_prepare_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;

    struct ngl_node *scene = s->scene;
    if (!scene) {
        return 0;
    }

    LOG(DEBUG, "prepare scene %s @ t=%f", scene->label, t);

    s->activitycheck_nodes.count = 0;
    int ret = ngli_node_visit(scene, 1, t);
    if (ret < 0)
        return ret;

    ret = ngli_node_honor_release_prefetch(&s->activitycheck_nodes);
    if (ret < 0)
        return ret;

    ret = ngli_node_update(scene, t);
    if (ret < 0)
        return ret;

    return 0;
}

static int cmd_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;

    int ret = cmd_prepare_draw(s, arg);
    if (ret < 0)
        goto end;

    ret = s->backend->pre_draw(s, t);
    if (ret < 0)
        goto end;

    if (s->scene) {
        LOG(DEBUG, "draw scene %s @ t=%f", s->scene->label, t);
        ngli_node_draw(s->scene);
    }

end:;
    int end_ret = s->backend->post_draw(s, t);
    if (end_ret < 0)
        return end_ret;

    return ret;
}

static int cmd_stop(struct ngl_ctx *s, void *arg)
{
    if (s->backend)
        s->backend->destroy(s);

    return 0;
}

static int dispatch_cmd(struct ngl_ctx *s, cmd_func_type cmd_func, void *arg)
{
    pthread_mutex_lock(&s->lock);
    s->cmd_func = cmd_func;
    s->cmd_arg = arg;
    pthread_cond_signal(&s->cond_wkr);
    while (s->cmd_func)
        pthread_cond_wait(&s->cond_ctl, &s->lock);
    pthread_mutex_unlock(&s->lock);

    return s->cmd_ret;
}

static void *worker_thread(void *arg)
{
    struct ngl_ctx *s = arg;

    ngli_thread_set_name("ngl-thread");

    pthread_mutex_lock(&s->lock);
    for (;;) {
        while (!s->cmd_func)
            pthread_cond_wait(&s->cond_wkr, &s->lock);
        s->cmd_ret = s->cmd_func(s, s->cmd_arg);
        int need_stop = s->cmd_func == cmd_stop;
        s->cmd_func = s->cmd_arg = NULL;
        pthread_cond_signal(&s->cond_ctl);

        if (need_stop)
            break;
    }
    pthread_mutex_unlock(&s->lock);

    return NULL;
}

#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
static int cmd_make_current(struct ngl_ctx *s, void *arg)
{
    const int current = *(int *)arg;
    ngli_glcontext_make_current(s->glcontext, current);
    return 0;
}

#define MAKE_CURRENT &(int[]){1}
#define DONE_CURRENT &(int[]){0}
static int configure_ios(struct ngl_ctx *s, struct ngl_config *config)
{
    int ret = cmd_configure(s, config);
    if (ret < 0)
        return ret;
    cmd_make_current(s, DONE_CURRENT);

    return dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
}

static int resize_ios(struct ngl_ctx *s, const struct resize_params *params)
{
    int ret = dispatch_cmd(s, cmd_make_current, DONE_CURRENT);
    if (ret < 0)
        return ret;

    cmd_make_current(s, MAKE_CURRENT);
    ret = cmd_resize(s, params);
    if (ret < 0)
        return ret;
    cmd_make_current(s, DONE_CURRENT);

    return dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
}
#endif

static void stop_thread(struct ngl_ctx *s)
{
    dispatch_cmd(s, cmd_stop, NULL);
    pthread_join(s->worker_tid, NULL);
    pthread_cond_destroy(&s->cond_ctl);
    pthread_cond_destroy(&s->cond_wkr);
    pthread_mutex_destroy(&s->lock);
}

struct ngl_ctx *ngl_create(void)
{
    struct ngl_ctx *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    if (pthread_mutex_init(&s->lock, NULL) ||
        pthread_cond_init(&s->cond_ctl, NULL) ||
        pthread_cond_init(&s->cond_wkr, NULL) ||
        pthread_create(&s->worker_tid, NULL, worker_thread, s)) {
        pthread_cond_destroy(&s->cond_ctl);
        pthread_cond_destroy(&s->cond_wkr);
        pthread_mutex_destroy(&s->lock);
        ngli_free(s);
        return NULL;
    }

    ngli_rnode_init(&s->rnode);
    s->rnode_pos = &s->rnode;

    ngli_darray_init(&s->modelview_matrix_stack, 4 * 4 * sizeof(float), 1);
    ngli_darray_init(&s->projection_matrix_stack, 4 * 4 * sizeof(float), 1);
    ngli_darray_init(&s->activitycheck_nodes, sizeof(struct ngl_node *), 0);

    static const NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;
    if (!ngli_darray_push(&s->modelview_matrix_stack, id_matrix) ||
        !ngli_darray_push(&s->projection_matrix_stack, id_matrix))
        goto fail;

    LOG(INFO, "context create in node.gl v%d.%d.%d",
        NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);

    return s;

fail:
    ngli_rnode_reset(&s->rnode);
    ngl_freep(&s);
    return NULL;
}

int ngl_configure(struct ngl_ctx *s, struct ngl_config *config)
{
    if (!config) {
        LOG(ERROR, "context configuration cannot be NULL");
        return NGL_ERROR_INVALID_ARG;
    }

    if (config->offscreen) {
        if (config->width <= 0 || config->height <= 0) {
            LOG(ERROR,
                "could not initialize offscreen rendering with invalid dimensions (%dx%d)",
                config->width,
                config->height);
            return NGL_ERROR_INVALID_ARG;
        }
    } else {
        if (config->capture_buffer) {
            LOG(ERROR, "capture_buffer is only supported with offscreen rendering");
            return NGL_ERROR_INVALID_ARG;
        }
    }

    s->configured = 0;
#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    int ret = configure_ios(s, config);
#else
    int ret = dispatch_cmd(s, cmd_configure, config);
#endif
    if (ret < 0)
        return ret;
    s->configured = 1;
    return 0;
}

int ngl_resize(struct ngl_ctx *s, int width, int height, const int *viewport)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before resizing rendering buffers");
        return NGL_ERROR_INVALID_USAGE;
    }

    const struct ngl_config *config = &s->config;
    if (config->offscreen) {
        LOG(ERROR, "offscreen context does not support resize operation");
        return NGL_ERROR_INVALID_USAGE;
    }

    struct resize_params params = {
        .width = width,
        .height = height,
        .viewport = viewport,
    };

#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    return resize_ios(s, &params);
#else
    return dispatch_cmd(s, cmd_resize, &params);
#endif
}

int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before setting a scene");
        return NGL_ERROR_INVALID_USAGE;
    }

    return dispatch_cmd(s, cmd_set_scene, scene);
}

int ngli_prepare_draw(struct ngl_ctx *s, double t)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before updating");
        return NGL_ERROR_INVALID_USAGE;
    }

    return dispatch_cmd(s, cmd_prepare_draw, &t);
}

int ngl_draw(struct ngl_ctx *s, double t)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before drawing");
        return NGL_ERROR_INVALID_USAGE;
    }

    return dispatch_cmd(s, cmd_draw, &t);
}

void ngl_freep(struct ngl_ctx **ss)
{
    struct ngl_ctx *s = *ss;

    if (!s)
        return;

    if (s->configured)
        ngl_set_scene(s, NULL);

    stop_thread(s);
    ngli_rnode_reset(&s->rnode);
    ngli_darray_reset(&s->modelview_matrix_stack);
    ngli_darray_reset(&s->projection_matrix_stack);
    ngli_darray_reset(&s->activitycheck_nodes);
    ngli_free(*ss);
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
        LOG(ERROR, "a Java virtual machine has already been set");
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
        return NGL_ERROR_EXTERNAL;

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
    return NGL_ERROR_UNSUPPORTED;
}

void *ngl_jni_get_java_vm(void)
{
    return NULL;
}

int ngl_android_set_application_context(void *application_context)
{
    return NGL_ERROR_UNSUPPORTED;
}

void *ngl_android_get_application_context(void)
{
    return NULL;
}
#endif
