/*
 * Copyright 2024 Nope Forge
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
#include <nopemd.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>

#include "android_imagereader.h"
#include "egl.h"
#include "format.h"
#include "gpu_ctx_gl.h"
#include "glincludes.h"
#include "hwmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nopegl.h"
#include "internal.h"
#include "texture_gl.h"

struct hwmap_mc {
    struct android_image *android_image;
    EGLImageKHR egl_image;
    GLuint gl_texture;
    struct texture *texture;
};

static int support_direct_rendering(struct hwmap *hwmap)
{
    const struct hwmap_params *params = &hwmap->params;

    int direct_rendering = params->image_layouts & (1 << NGLI_IMAGE_LAYOUT_MEDIACODEC);

    if (direct_rendering) {
        if (params->texture_mipmap_filter) {
            LOG(WARNING, "external textures do not support mipmapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        } else if (params->texture_wrap_s != NGLI_WRAP_CLAMP_TO_EDGE ||
                   params->texture_wrap_t != NGLI_WRAP_CLAMP_TO_EDGE) {
            LOG(WARNING, "external textures only support clamp to edge wrapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        }
    }

    return direct_rendering;
}

static int mc_init(struct hwmap *hwmap, struct nmd_frame *frame)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct hwmap_params *params = &hwmap->params;
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;

    ngli_glGenTextures(gl, 1, &mc->gl_texture);

    const GLint min_filter = ngli_texture_get_gl_min_filter(params->texture_min_filter, NGLI_MIPMAP_FILTER_NONE);
    const GLint mag_filter = ngli_texture_get_gl_mag_filter(params->texture_mag_filter);

    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, mc->gl_texture);
    ngli_glTexParameteri(gl, GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, min_filter);
    ngli_glTexParameteri(gl, GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, mag_filter);
    ngli_glTexParameteri(gl, GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ngli_glTexParameteri(gl, GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, 0);

    struct texture_params texture_params = {
        .type         = NGLI_TEXTURE_TYPE_2D,
        .format       = NGLI_FORMAT_UNDEFINED,
        .min_filter   = params->texture_min_filter,
        .mag_filter   = params->texture_mag_filter,
        .wrap_s       = NGLI_WRAP_CLAMP_TO_EDGE,
        .wrap_t       = NGLI_WRAP_CLAMP_TO_EDGE,
        .wrap_r       = NGLI_WRAP_CLAMP_TO_EDGE,
        .usage        = NGLI_TEXTURE_USAGE_SAMPLED_BIT,
    };

    struct texture_gl_wrap_params wrap_params = {
        .params  = &texture_params,
        .texture = mc->gl_texture,
        .target  = GL_TEXTURE_EXTERNAL_OES,
    };

    mc->texture = ngli_texture_create(gpu_ctx);
    if (!mc->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngli_texture_gl_wrap(mc->texture, &wrap_params);
    if (ret < 0)
        return ret;

    const struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_MEDIACODEC,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_nopemd_frame(frame),
    };
    ngli_image_init(&hwmap->mapped_image, &image_params, &mc->texture);

    hwmap->require_hwconv = !support_direct_rendering(hwmap);

    return 0;
}

static int mc_map_frame(struct hwmap *hwmap, struct nmd_frame *frame)
{
    const struct hwmap_params *params = &hwmap->params;
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct android_ctx *android_ctx = &ctx->android_ctx;

    ngli_texture_gl_set_dimensions(mc->texture, frame->width, frame->height, 0);

    int ret = nmd_mc_frame_render_and_releasep(&frame);
    if (ret < 0)
        return ret;

    struct android_image *android_image;
    ret = ngli_android_imagereader_acquire_next_image(params->android_imagereader, &android_image);
    if (ret < 0)
        return ret;

    ngli_eglDestroyImageKHR(gl, mc->egl_image);
    mc->egl_image = NULL;
    ngli_android_image_freep(&mc->android_image);

    mc->android_image = android_image;

    AHardwareBuffer *hardware_buffer = ngli_android_image_get_hardware_buffer(mc->android_image);
    if (!hardware_buffer)
        return NGL_ERROR_EXTERNAL;

    AHardwareBuffer_Desc desc;
    android_ctx->AHardwareBuffer_describe(hardware_buffer, &desc);

    AImageCropRect crop_rect;
    ret = ngli_android_image_get_crop_rect(mc->android_image, &crop_rect);
    if (ret < 0)
        return ret;

    float *matrix = hwmap->mapped_image.coordinates_matrix;
    const int filtering = params->texture_min_filter || params->texture_mag_filter;
    ngli_android_get_crop_matrix(matrix, &desc, &crop_rect, filtering);

    EGLClientBuffer egl_buffer = ngli_eglGetNativeClientBufferANDROID(gl, hardware_buffer);
    if (!egl_buffer)
        return NGL_ERROR_EXTERNAL;

    static const EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR,
        EGL_TRUE,
        EGL_NONE,
    };

    const struct texture_gl *texture_gl = (struct texture_gl *)mc->texture;
    const GLuint id = texture_gl->id;

    mc->egl_image = ngli_eglCreateImageKHR(gl, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, egl_buffer, attrs);
    if (!mc->egl_image) {
        LOG(ERROR, "failed to create egl image");
        return NGL_ERROR_EXTERNAL;
    }

    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, id);
    ngli_glEGLImageTargetTexture2DOES(gl, GL_TEXTURE_EXTERNAL_OES, mc->egl_image);

    return 0;
}

static void mc_uninit(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwmap_mc *mc = hwmap->hwmap_priv_data;

    ngli_texture_freep(&mc->texture);
    ngli_glDeleteTextures(gl, 1, &mc->gl_texture);

    ngli_eglDestroyImageKHR(gl, mc->egl_image);
    ngli_android_image_freep(&mc->android_image);
}

const struct hwmap_class ngli_hwmap_mc_gl_class = {
    .name      = "mediacodec (oes zero-copy)",
    .hwformat  = NMD_PIXFMT_MEDIACODEC,
    .layouts   = (const int[]){
        NGLI_IMAGE_LAYOUT_MEDIACODEC,
        NGLI_IMAGE_LAYOUT_NONE
    },
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwmap_mc),
    .init      = mc_init,
    .map_frame = mc_map_frame,
    .uninit    = mc_uninit,
};
