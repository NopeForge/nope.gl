/*
 * Copyright 2018 GoPro Inc.
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

#include "android_surface.h"
#include "format.h"
#include "glincludes.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

static int common_get_data_format(int pix_fmt)
{
    switch (pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
        return NGLI_FORMAT_R8G8B8A8_UNORM;
    case SXPLAYER_PIXFMT_BGRA:
        return NGLI_FORMAT_B8G8R8A8_UNORM;
    case SXPLAYER_SMPFMT_FLT:
        return NGLI_FORMAT_R32_SFLOAT;
    default:
        return -1;
    }
}

static int common_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;

    struct texture_params params = s->params;
    params.width  = frame->linesize >> 2;
    params.height = frame->height;

    params.format = common_get_data_format(frame->pix_fmt);
    if (params.format < 0)
        return -1;

    int ret = ngli_texture_init(&s->texture, gl, &params);
    if (ret < 0)
        return ret;

    ngli_image_init(&s->image, NGLI_IMAGE_LAYOUT_DEFAULT, &s->texture);

    return 0;
}

static int common_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    struct texture *texture = &s->texture;
    struct image *image = &s->image;

    const int linesize       = frame->linesize >> 2;
    image->coordinates_matrix[0] = linesize ? frame->width / (float)linesize : 1.0;

    if (!ngli_texture_match_dimensions(&s->texture, linesize, frame->height, 0)) {
        ngli_texture_reset(texture);

        int ret = common_init(node, frame);
        if (ret < 0)
            return ret;
    }

    return ngli_texture_upload(texture, frame->data, 0);
}

static const struct hwmap_class hwmap_common_class = {
    .name      = "default",
    .init      = common_init,
    .map_frame = common_map_frame,
};

static const struct hwmap_class *common_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    return &hwmap_common_class;
}

const struct hwupload_class ngli_hwupload_common_class = {
    .get_hwmap = common_get_hwmap,
};
