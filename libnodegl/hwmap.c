/*
 * Copyright 2017 GoPro Inc.
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
#include <sxplayer.h>

#include "config.h"
#include "gpu_ctx.h"
#include "hwmap.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"

extern const struct hwmap_class ngli_hwmap_common_class;
extern const struct hwmap_class *ngli_hwmap_gl_classes[];

static const struct hwmap_class *get_hwmap_class(const struct hwmap *hwmap, struct sxplayer_frame *frame)
{
    const struct hwmap_class **hwmap_classes = hwmap->hwmap_classes;

    if (hwmap_classes) {
        for (int i = 0; hwmap_classes[i]; i++) {
            const struct hwmap_class *hwmap_class = hwmap_classes[i];
            if (hwmap_class->hwformat == frame->pix_fmt)
                return hwmap_class;
        }
    }

    return &ngli_hwmap_common_class;
}

static int init_hwconv(struct hwmap *hwmap)
{
    struct ngl_ctx *ctx = hwmap->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct hwmap_params *params = &hwmap->params;
    struct image *mapped_image = &hwmap->mapped_image;
    struct image *hwconv_image = &hwmap->hwconv_image;
    struct hwconv *hwconv = &hwmap->hwconv;

    ngli_hwconv_reset(hwconv);
    ngli_image_reset(hwconv_image);
    ngli_texture_freep(&hwmap->hwconv_texture);

    LOG(DEBUG, "converting texture '%s' from %s to rgba", hwmap->params.label, hwmap->hwmap_class->name);

    const struct texture_params texture_params = {
        .type          = NGLI_TEXTURE_TYPE_2D,
        .format        = NGLI_FORMAT_R8G8B8A8_UNORM,
        .width         = mapped_image->params.width,
        .height        = mapped_image->params.height,
        .min_filter    = params->texture_min_filter,
        .mag_filter    = params->texture_mag_filter,
        .mipmap_filter = params->texture_mipmap_filter,
        .wrap_s        = params->texture_wrap_s,
        .wrap_t        = params->texture_wrap_t,
        .usage         = params->texture_usage | NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
    };

    hwmap->hwconv_texture = ngli_texture_create(gpu_ctx);
    if (!hwmap->hwconv_texture)
        return NGL_ERROR_MEMORY;
    int ret = ngli_texture_init(hwmap->hwconv_texture, &texture_params);
    if (ret < 0)
        goto end;

    struct image_params image_params = {
        .width = mapped_image->params.width,
        .height = mapped_image->params.height,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .color_scale = 1.f,
        .color_info = NGLI_COLOR_INFO_DEFAULTS,
    };
    ngli_image_init(hwconv_image, &image_params, &hwmap->hwconv_texture);

    ret = ngli_hwconv_init(hwconv, ctx, hwconv_image, &mapped_image->params);
    if (ret < 0)
        goto end;

    return 0;

end:
    ngli_hwconv_reset(hwconv);
    ngli_image_reset(hwconv_image);
    ngli_texture_freep(&hwmap->hwconv_texture);
    return ret;
}

static int exec_hwconv(struct hwmap *hwmap)
{
    struct texture *texture = hwmap->hwconv_texture;
    const struct texture_params *texture_params = &texture->params;
    struct image *mapped_image = &hwmap->mapped_image;
    struct hwconv *hwconv = &hwmap->hwconv;

    int ret = ngli_hwconv_convert_image(hwconv, mapped_image);
    if (ret < 0)
        return ret;

    if (texture_params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE)
        ngli_texture_generate_mipmap(texture);

    return 0;
}

int ngli_hwmap_init(struct hwmap *hwmap, struct ngl_ctx *ctx, const struct hwmap_params *params)
{
    memset(hwmap, 0, sizeof(*hwmap));
    hwmap->ctx = ctx;
    hwmap->params = *params;
    hwmap->pix_fmt = -1; /* TODO: replace by SXPLAYER_PIXFMT_NONE */

    ngli_unused const struct ngl_config *config = &ctx->config;
#ifdef BACKEND_GL
    if (config->backend == NGL_BACKEND_OPENGL || config->backend == NGL_BACKEND_OPENGLES)
        hwmap->hwmap_classes = ngli_hwmap_gl_classes;
#endif

    return 0;
}

static void hwmap_reset(struct hwmap *hwmap)
{
    hwmap->require_hwconv = 0;
    ngli_hwconv_reset(&hwmap->hwconv);
    ngli_image_reset(&hwmap->hwconv_image);
    ngli_texture_freep(&hwmap->hwconv_texture);
    hwmap->hwconv_initialized = 0;
    ngli_image_reset(&hwmap->mapped_image);
    if (hwmap->hwmap_priv_data && hwmap->hwmap_class) {
        hwmap->hwmap_class->uninit(hwmap);
    }
    hwmap->hwmap_class = NULL;
    ngli_freep(&hwmap->hwmap_priv_data);
    hwmap->pix_fmt = -1; /* TODO: replace by SXPLAYER_PIXFMT_NONE */
    hwmap->width = 0;
    hwmap->height = 0;
}

int ngli_hwmap_map_frame(struct hwmap *hwmap, struct sxplayer_frame *frame, struct image *image)
{
    if (frame->width  != hwmap->width ||
        frame->height != hwmap->height ||
        frame->pix_fmt != hwmap->pix_fmt) {
        hwmap_reset(hwmap);

        const struct hwmap_class *hwmap_class = get_hwmap_class(hwmap, frame);
        ngli_assert(hwmap_class);
        ngli_assert(hwmap_class->priv_size);
        hwmap->hwmap_class = hwmap_class;

        hwmap->hwmap_priv_data = ngli_calloc(1, hwmap_class->priv_size);
        if (!hwmap->hwmap_priv_data) {
            sxplayer_release_frame(frame);
            return NGL_ERROR_MEMORY;
        }

        int ret = hwmap_class->init(hwmap, frame);
        if (ret < 0) {
            sxplayer_release_frame(frame);
            return ret;
        }
        hwmap->pix_fmt = frame->pix_fmt;
        hwmap->width = frame->width;
        hwmap->height = frame->height;

        LOG(DEBUG, "mapping texture '%s' with method: %s", hwmap->params.label, hwmap_class->name);
    }

    int ret = hwmap->hwmap_class->map_frame(hwmap, frame);
    if (ret < 0)
        goto end;

    if (hwmap->require_hwconv) {
        if (!hwmap->hwconv_initialized) {
            ret = init_hwconv(hwmap);
            if (ret < 0)
                goto end;
            hwmap->hwconv_initialized = 1;
        }
        ret = exec_hwconv(hwmap);
        if (ret < 0)
            goto end;
        *image = hwmap->hwconv_image;
    } else {
        *image = hwmap->mapped_image;
    }

end:
    image->ts = frame->ts;

    if (!(hwmap->hwmap_class->flags &  HWMAP_FLAG_FRAME_OWNER))
        sxplayer_release_frame(frame);
    return ret;
}

void ngli_hwmap_uninit(struct hwmap *hwmap)
{
    hwmap_reset(hwmap);
    memset(hwmap, 0, sizeof(*hwmap));
}
