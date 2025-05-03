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
#include <nopemd.h>

#include <va/va.h>
#include <va/va_drmcommon.h>

#include "hwmap.h"
#include "image.h"
#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "ngpu/opengl/ctx_gl.h"
#include "ngpu/opengl/egl.h"
#include "ngpu/opengl/glincludes.h"
#include "ngpu/opengl/texture_gl.h"
#include "nopegl.h"
#include "utils/utils.h"

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

struct hwmap_vaapi {
    int use_drm_format_modifiers;
    struct nmd_frame *frame;
    struct ngpu_texture *planes[2];

    GLuint gl_planes[2];
    EGLImageKHR egl_images[2];

    VADRMPRIMESurfaceDescriptor surface_descriptor;
    int surface_acquired;
};

static int support_direct_rendering(struct hwmap *hwmap)
{
    const struct hwmap_params *params = &hwmap->params;

    int direct_rendering = params->image_layouts & NGLI_IMAGE_LAYOUT_NV12_BIT;

    if (direct_rendering && params->texture_mipmap_filter) {
        LOG(WARNING,
            "vaapi direct rendering does not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering;
}

static int vaapi_init(struct hwmap *hwmap, struct nmd_frame *frame)
{
    const struct hwmap_params *params = &hwmap->params;
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    if (!(gl->features & (NGLI_FEATURE_GL_OES_EGL_IMAGE |
                          NGLI_FEATURE_GL_EGL_IMAGE_BASE_KHR |
                          NGLI_FEATURE_GL_EGL_EXT_IMAGE_DMA_BUF_IMPORT))) {
        LOG(ERROR, "context does not support required extensions for vaapi");
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    vaapi->use_drm_format_modifiers =
        NGLI_HAS_ALL_FLAGS(gl->features, NGLI_FEATURE_GL_EGL_EXT_IMAGE_DMA_BUF_IMPORT_MODIFIERS);

    gl->funcs.GenTextures(2, vaapi->gl_planes);

    for (size_t i = 0; i < 2; i++) {
        const GLint min_filter = ngpu_texture_get_gl_min_filter(params->texture_min_filter,
                                                                    NGPU_MIPMAP_FILTER_NONE);
        const GLint mag_filter = ngpu_texture_get_gl_mag_filter(params->texture_mag_filter);
        const GLint wrap_s = ngpu_texture_get_gl_wrap(params->texture_wrap_s);
        const GLint wrap_t = ngpu_texture_get_gl_wrap(params->texture_wrap_t);

        gl->funcs.BindTexture(GL_TEXTURE_2D, vaapi->gl_planes[i]);
        gl->funcs.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
        gl->funcs.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
        gl->funcs.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
        gl->funcs.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
        gl->funcs.BindTexture(GL_TEXTURE_2D, 0);

        const enum ngpu_format format = i == 0 ? NGPU_FORMAT_R8_UNORM : NGPU_FORMAT_R8G8_UNORM;

        const struct ngpu_texture_params plane_params = {
            .type             = NGPU_TEXTURE_TYPE_2D,
            .format           = format,
            .min_filter       = params->texture_min_filter,
            .mag_filter       = params->texture_mag_filter,
            .wrap_s           = params->texture_wrap_s,
            .wrap_t           = params->texture_wrap_t,
            .usage            = NGPU_TEXTURE_USAGE_SAMPLED_BIT,
        };

        const struct ngpu_texture_gl_wrap_params wrap_params = {
            .params  = &plane_params,
            .texture = vaapi->gl_planes[i],
        };

        vaapi->planes[i] = ngpu_texture_create(gpu_ctx);
        if (!vaapi->planes[i])
            return NGL_ERROR_MEMORY;

        int ret = ngpu_texture_gl_wrap(vaapi->planes[i], &wrap_params);
        if (ret < 0)
            return ret;
    }

    const struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_NV12,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_nopemd_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, vaapi->planes);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

    return 0;
}

static void vaapi_release_frame_resources(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    if (vaapi->surface_acquired) {
        for (size_t i = 0; i < 2; i++) {
            if (vaapi->egl_images[i]) {
                ngli_eglDestroyImageKHR(gl, vaapi->egl_images[i]);
                vaapi->egl_images[i] = NULL;
            }
        }
        for (uint32_t i = 0; i < vaapi->surface_descriptor.num_objects; i++) {
            close(vaapi->surface_descriptor.objects[i].fd);
        }
        vaapi->surface_acquired = 0;
    }

    nmd_frame_releasep(&vaapi->frame);
}

static void vaapi_uninit(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    for (size_t i = 0; i < 2; i++)
        ngpu_texture_freep(&vaapi->planes[i]);

    gl->funcs.DeleteTextures(2, vaapi->gl_planes);

    vaapi_release_frame_resources(hwmap);
}

static int vaapi_map_frame(struct hwmap *hwmap, struct nmd_frame *frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct vaapi_ctx *vaapi_ctx = &ctx->vaapi_ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_vaapi *vaapi = hwmap->hwmap_priv_data;

    vaapi_release_frame_resources(hwmap);
    vaapi->frame = frame;

    VASurfaceID surface_id = (VASurfaceID)(intptr_t)frame->datap[0];
    VAStatus status = vaExportSurfaceHandle(vaapi_ctx->va_display,
                                            surface_id,
                                            VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_READ_ONLY |
                                            VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                            &vaapi->surface_descriptor);
    if (status != VA_STATUS_SUCCESS) {
        LOG(ERROR, "failed to export vaapi surface handle: %s", vaErrorStr(status));
        return NGL_ERROR_EXTERNAL;
    }
    vaapi->surface_acquired = 1;

    status = vaSyncSurface(vaapi_ctx->va_display, surface_id);
    if (status != VA_STATUS_SUCCESS)
        LOG(WARNING, "failed to sync surface: %s", vaErrorStr(status));

    if (vaapi->surface_descriptor.fourcc != VA_FOURCC_NV12 &&
        vaapi->surface_descriptor.fourcc != VA_FOURCC_P010 &&
        vaapi->surface_descriptor.fourcc != VA_FOURCC_P016) {
        LOG(ERROR, "unsupported vaapi surface format: 0x%x", vaapi->surface_descriptor.fourcc);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    size_t num_layers = vaapi->surface_descriptor.num_layers;
    if (num_layers > NGLI_ARRAY_NB(vaapi->egl_images)) {
        LOG(WARNING, "vaapi layer count (%zu) exceeds plane count (%zu)", num_layers, NGLI_ARRAY_NB(vaapi->egl_images));
        num_layers = NGLI_ARRAY_NB(vaapi->egl_images);
    }

    for (size_t i = 0; i < num_layers; i++) {
        EGLint attribs[32] = {EGL_NONE};
        size_t nb_attribs = 0;

#define ADD_ATTRIB(name, value) do {                          \
    ngli_assert(nb_attribs + 3 < NGLI_ARRAY_NB(attribs));     \
    attribs[nb_attribs++] = (name);                           \
    attribs[nb_attribs++] = (EGLint)(value);                  \
    attribs[nb_attribs] = EGL_NONE;                           \
} while(0)

#define ADD_PLANE_ATTRIBS(plane) do {                                                                         \
    const uint32_t object_index = vaapi->surface_descriptor.layers[i].object_index[plane];                    \
    const uint64_t drm_format_modifier = vaapi->surface_descriptor.objects[object_index].drm_format_modifier; \
    ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _FD_EXT,                                                         \
               vaapi->surface_descriptor.objects[object_index].fd);                                           \
    ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _OFFSET_EXT,                                                     \
               vaapi->surface_descriptor.layers[i].offset[plane]);                                            \
    ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _PITCH_EXT,                                                      \
               vaapi->surface_descriptor.layers[i].pitch[plane]);                                             \
    if (vaapi->use_drm_format_modifiers && drm_format_modifier != DRM_FORMAT_MOD_INVALID) {                   \
        ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _MODIFIER_LO_EXT,                                            \
                   (uint32_t)(drm_format_modifier & 0xFFFFFFFFLU));                                           \
        ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _MODIFIER_HI_EXT,                                            \
                   (uint32_t)((drm_format_modifier >> 32U) & 0xFFFFFFFFLU));                                  \
    }                                                                                                         \
} while (0)

        int32_t width = i == 0 ? frame->width : (frame->width + 1) >> 1;
        int32_t height = i == 0 ? frame->height : (frame->height + 1) >> 1;

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
            return NGL_ERROR_EXTERNAL;
        }

        struct ngpu_texture *plane = vaapi->planes[i];
        struct ngpu_texture_gl *plane_gl = (struct ngpu_texture_gl *)plane;
        ngpu_texture_gl_set_dimensions(plane, width, height, 0);

        gl->funcs.BindTexture(plane_gl->target, plane_gl->id);
        gl->funcs.EGLImageTargetTexture2DOES(plane_gl->target, vaapi->egl_images[i]);
    }

    return 0;
}

const struct hwmap_class ngli_hwmap_vaapi_gl_class = {
    .name      = "vaapi (dma buf → egl image)",
    .hwformat  = NMD_PIXFMT_VAAPI,
    .layouts   = (const enum image_layout[]){
        NGLI_IMAGE_LAYOUT_NV12,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_vaapi),
    .init      = vaapi_init,
    .map_frame = vaapi_map_frame,
    .uninit    = vaapi_uninit,
};
