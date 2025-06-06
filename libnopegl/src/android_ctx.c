/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2020-2022 GoPro Inc.
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

#include <dlfcn.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "android_ctx.h"
#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "utils/utils.h"

#if defined(BACKEND_GLES)
#include "ngpu/opengl/ctx_gl.h"
#endif

#if defined(BACKEND_VK)
#include "ngpu/vulkan/ctx_vk.h"
#endif

#define NDK_LOAD_FUNC(handle, name) do {       \
    s->name = dlsym(handle, #name);            \
    if (!s->name) {                            \
        LOG(INFO, "missing %s symbol", #name); \
        goto done;                             \
    }                                          \
} while (0)

static int load_media_api(struct android_ctx *s)
{
    s->libmediandk_handle = dlopen("libmediandk.so", RTLD_NOW);
    if (!s->libmediandk_handle) {
        LOG(ERROR, "could not open libmediandk.so");
        return NGL_ERROR_UNSUPPORTED;
    }

    NDK_LOAD_FUNC(s->libmediandk_handle, AImage_delete);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImage_getHardwareBuffer);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImage_getCropRect);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_newWithUsage);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_getWindow);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_acquireNextImage);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_setImageListener);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_delete);
    NDK_LOAD_FUNC(s->libmediandk_handle, AHardwareBuffer_describe);

    return 0;

done:
    dlclose(s->libmediandk_handle);
    s->libmediandk_handle = NULL;
    return NGL_ERROR_UNSUPPORTED;
}

static int load_window_api(struct android_ctx *s)
{
    s->libandroid_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!s->libandroid_handle) {
        LOG(ERROR, "could not open libandroid.so");
        return NGL_ERROR_UNSUPPORTED;
    }

    NDK_LOAD_FUNC(s->libandroid_handle, ANativeWindow_toSurface);

    return 0;

done:
    dlclose(s->libandroid_handle);
    s->libandroid_handle = NULL;
    return NGL_ERROR_UNSUPPORTED;
}

static int has_native_imagereader_api_support(struct ngpu_ctx *gpu_ctx)
{
    ngli_unused const struct ngl_config *config = &gpu_ctx->config;
#if defined(BACKEND_GLES)
    if (config->backend == NGL_BACKEND_OPENGLES) {
        const struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
        const struct glcontext *gl = gpu_ctx_gl->glcontext;
        const uint64_t features = NGLI_FEATURE_GL_OES_EGL_EXTERNAL_IMAGE |
                                  NGLI_FEATURE_GL_EGL_ANDROID_GET_IMAGE_NATIVE_CLIENT_BUFFER;
        return (NGLI_HAS_ALL_FLAGS(gl->features, features));
    }
#endif
#if defined(BACKEND_VK)
    if (config->backend == NGL_BACKEND_VULKAN) {
        const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
        const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
        static const char * const required_extensions[] = {
            VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
            VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        };
        for (size_t i = 0; i < NGLI_ARRAY_NB(required_extensions); i++) {
            if (!ngli_vkcontext_has_extension(vk, required_extensions[i], 1))
                return 0;
        }
        return 1;
    }
#endif
    return 0;
}

int ngli_android_ctx_init(struct ngpu_ctx *gpu_ctx, struct android_ctx *s)
{
    memset(s, 0, sizeof(*s));

    int ret = load_media_api(s);
    if (ret < 0) {
        LOG(INFO, "could not load native media API");
        return ret;
    }

    ret = load_window_api(s);
    if (ret < 0) {
        LOG(INFO, "could not load native window API");
        return ret;
    }

    if (!has_native_imagereader_api_support(gpu_ctx)) {
        LOG(ERROR, "device is missing required functions/extensions available since Android 9.0");
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

void ngli_android_ctx_reset(struct android_ctx *s)
{
    if (s->libmediandk_handle)
        dlclose(s->libmediandk_handle);
    if (s->libandroid_handle)
        dlclose(s->libandroid_handle);
    memset(s, 0, sizeof(*s));
}
