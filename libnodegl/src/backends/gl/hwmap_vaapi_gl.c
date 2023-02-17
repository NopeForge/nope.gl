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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sxplayer.h>

#include <va/va.h>
#include <va/va_drmcommon.h>

#include "egl.h"
#include "gpu_ctx.h"
#include "gpu_ctx_gl.h"
#include "glincludes.h"
#include "hwmap.h"
#include "image.h"
#include "log.h"
#include "nodegl.h"
#include "internal.h"
#include "texture_gl.h"
#include "utils.h"

struct hwmap_vaapi {
    struct sxplayer_frame *frame;
    struct texture *planes[2];

    GLuint gl_planes[2];
    EGLImageKHR egl_images[2];

    VADRMPRIMESurfaceDescriptor surface_descriptor;
    int surface_acquired;
};

static int support_direct_rendering(struct hwmap *hwmap)
{
    const struct hwmap_params *params = &hwmap->params;

    int direct_rendering = params->image_layouts & (1 << NGLI_IMAGE_LAYOUT_NV12);

    if (direct_rendering && params->texture_mipmap_filter) {
        LOG(WARNING,
            "vaapi direct rendering does not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering;
}

static int vaapi_init(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    const struct hwmap_params *params = &hwmap->params;
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    if (!(gl->features & (NGLI_FEATURE_GL_OES_EGL_IMAGE |
                          NGLI_FEATURE_GL_EGL_IMAGE_BASE_KHR |
                          NGLI_FEATURE_GL_EGL_EXT_IMAGE_DMA_BUF_IMPORT))) {
        LOG(ERROR, "context does not support required extensions for vaapi");
        return -1;
    }

    ngli_glGenTextures(gl, 2, vaapi->gl_planes);

    for (int i = 0; i < 2; i++) {
        const GLint min_filter = ngli_texture_get_gl_min_filter(params->texture_min_filter, NGLI_MIPMAP_FILTER_NONE);
        const GLint mag_filter = ngli_texture_get_gl_mag_filter(params->texture_mag_filter);
        const GLint wrap_s = ngli_texture_get_gl_wrap(params->texture_wrap_s);
        const GLint wrap_t = ngli_texture_get_gl_wrap(params->texture_wrap_t);

        ngli_glBindTexture(gl, GL_TEXTURE_2D, vaapi->gl_planes[i]);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

        const int format = i == 0 ? NGLI_FORMAT_R8_UNORM : NGLI_FORMAT_R8G8_UNORM;

        const struct texture_params plane_params = {
            .type             = NGLI_TEXTURE_TYPE_2D,
            .format           = format,
            .min_filter       = params->texture_min_filter,
            .mag_filter       = params->texture_mag_filter,
            .wrap_s           = params->texture_wrap_s,
            .wrap_t           = params->texture_wrap_t,
            .usage            = NGLI_TEXTURE_USAGE_SAMPLED_BIT,
        };

        const struct texture_gl_wrap_params wrap_params = {
            .params  = &plane_params,
            .texture = vaapi->gl_planes[i],
        };

        vaapi->planes[i] = ngli_texture_create(gpu_ctx);
        if (!vaapi->planes[i])
            return NGL_ERROR_MEMORY;

        int ret = ngli_texture_gl_wrap(vaapi->planes[i], &wrap_params);
        if (ret < 0)
            return ret;
    }

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_NV12,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vaapi->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

    return 0;
}

static void vaapi_uninit(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    for (int i = 0; i < 2; i++)
        ngli_texture_freep(&vaapi->planes[i]);

    ngli_glDeleteTextures(gl, 2, vaapi->gl_planes);

    if (vaapi->surface_acquired) {
        for (int i = 0; i < 2; i++) {
            if (vaapi->egl_images[i]) {
                ngli_eglDestroyImageKHR(gl, vaapi->egl_images[i]);
                vaapi->egl_images[i] = NULL;
            }
        }
        for (int i = 0; i < vaapi->surface_descriptor.num_objects; i++) {
            close(vaapi->surface_descriptor.objects[i].fd);
        }
        vaapi->surface_acquired = 0;
    }

    sxplayer_release_frame(vaapi->frame);
    vaapi->frame = NULL;
}

static int vaapi_map_frame(struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct vaapi_ctx *vaapi_ctx = &ctx->vaapi_ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    sxplayer_release_frame(vaapi->frame);
    vaapi->frame = frame;

    if (vaapi->surface_acquired) {
        for (int i = 0; i < 2; i++) {
            if (vaapi->egl_images[i]) {
                ngli_eglDestroyImageKHR(gl, vaapi->egl_images[i]);
                vaapi->egl_images[i] = NULL;
            }
        }
        for (int i = 0; i < vaapi->surface_descriptor.num_objects; i++) {
            close(vaapi->surface_descriptor.objects[i].fd);
        }
        vaapi->surface_acquired = 0;
    }

    VASurfaceID surface_id = (VASurfaceID)(intptr_t)frame->data;
    VAStatus status = vaExportSurfaceHandle(vaapi_ctx->va_display,
                                            surface_id,
                                            VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_READ_ONLY |
                                            VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                            &vaapi->surface_descriptor);
    if (status != VA_STATUS_SUCCESS) {
        LOG(ERROR, "failed to export vaapi surface handle: 0x%x", status);
        return -1;
    }
    vaapi->surface_acquired = 1;

    status = vaSyncSurface(vaapi_ctx->va_display, surface_id);
    if (status != VA_STATUS_SUCCESS)
        LOG(WARNING, "failed to sync surface");

    if (vaapi->surface_descriptor.fourcc != VA_FOURCC_NV12 &&
        vaapi->surface_descriptor.fourcc != VA_FOURCC_P010 &&
        vaapi->surface_descriptor.fourcc != VA_FOURCC_P016) {
        LOG(ERROR, "unsupported vaapi surface format: 0x%x", vaapi->surface_descriptor.fourcc);
        return -1;
    }

    int num_layers = vaapi->surface_descriptor.num_layers;
    if (num_layers > NGLI_ARRAY_NB(vaapi->egl_images)) {
        LOG(WARNING, "vaapi layer count (%d) exceeds plane count (%d)", num_layers, NGLI_ARRAY_NB(vaapi->egl_images));
        num_layers = NGLI_ARRAY_NB(vaapi->egl_images);
    }

    for (int i = 0; i < num_layers; i++) {
        int attribs[32] = {EGL_NONE};
        int nb_attribs = 0;

#define ADD_ATTRIB(name, value) do {                          \
    ngli_assert(nb_attribs + 3 < NGLI_ARRAY_NB(attribs));     \
    attribs[nb_attribs++] = (name);                           \
    attribs[nb_attribs++] = (value);                          \
    attribs[nb_attribs] = EGL_NONE;                           \
} while(0)

#define ADD_PLANE_ATTRIBS(plane) do {                                                \
    uint32_t object_index = vaapi->surface_descriptor.layers[i].object_index[plane]; \
    ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _FD_EXT,                                \
               vaapi->surface_descriptor.objects[object_index].fd);                  \
    ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _OFFSET_EXT,                            \
               vaapi->surface_descriptor.layers[i].offset[plane]);                   \
    ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _PITCH_EXT,                             \
               vaapi->surface_descriptor.layers[i].pitch[plane]);                    \
} while (0)

        int width = i == 0 ? frame->width : (frame->width + 1) >> 1;
        int height = i == 0 ? frame->height : (frame->height + 1) >> 1;

        ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, vaapi->surface_descriptor.layers[i].drm_format);
        ADD_ATTRIB(EGL_WIDTH,  width);
        ADD_ATTRIB(EGL_HEIGHT, height);

        ADD_PLANE_ATTRIBS(0);
        if (vaapi->surface_descriptor.layers[i].num_planes > 1)
            ADD_PLANE_ATTRIBS(1);
        if (vaapi->surface_descriptor.layers[i].num_planes > 2)
            ADD_PLANE_ATTRIBS(2);
        if (vaapi->surface_descriptor.layers[i].num_planes > 3)
            ADD_PLANE_ATTRIBS(3);

        vaapi->egl_images[i] = ngli_eglCreateImageKHR(gl,
                                                      EGL_NO_CONTEXT,
                                                      EGL_LINUX_DMA_BUF_EXT,
                                                      NULL,
                                                      attribs);
        if (!vaapi->egl_images[i]) {
            LOG(ERROR, "failed to create egl image");
            return -1;
        }

        struct texture *plane = vaapi->planes[i];
        struct texture_gl *plane_gl = (struct texture_gl *)plane;
        ngli_texture_gl_set_dimensions(plane, width, height, 0);

        ngli_glBindTexture(gl, plane_gl->target, plane_gl->id);
        ngli_glEGLImageTargetTexture2DOES(gl, plane_gl->target, vaapi->egl_images[i]);
    }

    return 0;
}

const struct hwmap_class ngli_hwmap_vaapi_gl_class = {
    .name      = "vaapi (dma buf → egl image)",
    .hwformat  = SXPLAYER_PIXFMT_VAAPI,
    .layouts   = (const int[]){
        NGLI_IMAGE_LAYOUT_NV12,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_vaapi),
    .init      = vaapi_init,
    .map_frame = vaapi_map_frame,
    .uninit    = vaapi_uninit,
};
