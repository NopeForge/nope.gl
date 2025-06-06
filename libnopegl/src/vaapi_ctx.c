/*
 * Copyright 2018-2022 GoPro Inc.
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

#if defined(BACKEND_GL) || defined(BACKEND_GLES)
#include "ngpu/opengl/ctx_gl.h"
#endif

#if defined(BACKEND_VK)
#include "ngpu/vulkan/ctx_vk.h"
#endif

#if defined(HAVE_VAAPI_X11)
#include <X11/Xlib.h>
#include <va/va_x11.h>
#endif

#if defined(HAVE_VAAPI_WAYLAND)
#include <wayland-client.h>
#include <va/va_wayland.h>
#endif

#include "log.h"
#include "ngpu/ctx.h"
#include "utils/utils.h"
#include "vaapi_ctx.h"

static int check_extensions(const struct ngpu_ctx *gpu_ctx)
{
    ngli_unused const struct ngl_config *config = &gpu_ctx->config;
#if defined(BACKEND_GL) || defined(BACKEND_GLES)
        if (config->backend == NGL_BACKEND_OPENGL ||
            config->backend == NGL_BACKEND_OPENGLES) {
        const struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
        const struct glcontext *gl = gpu_ctx_gl->glcontext;
        const uint64_t features = NGLI_FEATURE_GL_OES_EGL_IMAGE |
                                  NGLI_FEATURE_GL_EGL_IMAGE_BASE_KHR |
                                  NGLI_FEATURE_GL_EGL_EXT_IMAGE_DMA_BUF_IMPORT;
        if (NGLI_HAS_ALL_FLAGS(gl->features, features))
            return 1;
    }
#endif
#if defined(BACKEND_VK)
    if (config->backend == NGL_BACKEND_VULKAN) {
        const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
        const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
        static const char * const required_extensions[] = {
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
            VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
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

int ngli_vaapi_ctx_init(struct ngpu_ctx *gpu_ctx, struct vaapi_ctx *s)
{
    const struct ngl_config *config = &gpu_ctx->config;

    if (gpu_ctx->features & NGPU_FEATURE_SOFTWARE)
        return -1;

    if (!check_extensions(gpu_ctx))
        return -1;

    VADisplay va_display = NULL;
    if (config->platform == NGL_PLATFORM_XLIB) {
#if defined(HAVE_VAAPI_X11)
        Display *x11_display = XOpenDisplay(NULL);
        if (!x11_display) {
            LOG(ERROR, "could not initialize X11 display");
            return -1;
        }
        s->x11_display = x11_display;

        va_display = vaGetDisplay(x11_display);
#endif
    } else if (config->platform == NGL_PLATFORM_WAYLAND) {
#if defined(HAVE_VAAPI_WAYLAND)
        struct wl_display *wl_display = (struct wl_display *)gpu_ctx->config.display;
        if (!wl_display) {
            wl_display = wl_display_connect(NULL);
            if (!wl_display) {
                LOG(ERROR, "could not connect to Wayland display");
                return -1;
            }
            s->wl_display = wl_display;
        }

        va_display = vaGetDisplayWl(wl_display);
#endif
    }
    if (!va_display) {
        LOG(ERROR, "could not get va display");
        return -1;
    }

    int major_version;
    int minor_version;
    VAStatus va_status = vaInitialize(va_display, &major_version, &minor_version);
    if (va_status != VA_STATUS_SUCCESS) {
        LOG(ERROR, "could not initialize va display: %s", vaErrorStr(va_status));
        return -1;
    }

    s->va_display = va_display;
    s->va_version = major_version * 100 + minor_version;

    LOG(INFO, "VAAPI version: %d.%d", major_version, minor_version);

    return 0;
}

void ngli_vaapi_ctx_reset(struct vaapi_ctx *s)
{
    if (s->va_display)
        vaTerminate(s->va_display);
#if defined(HAVE_VAAPI_X11)
    if (s->x11_display)
        XCloseDisplay(s->x11_display);
#endif
#if defined(HAVE_VAAPI_WAYLAND)
    if (s->wl_display)
        wl_display_disconnect(s->wl_display);
#endif
    memset(s, 0, sizeof(*s));
}
