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
#include "glincludes.h"
#include "hwupload.h"
#include "hwupload_common.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

int ngli_hwupload_common_get_config_from_frame(struct ngl_node *node,
                                               struct sxplayer_frame *frame,
                                               struct hwupload_config *config)
{
    config->format = NGLI_HWUPLOAD_FMT_COMMON;
    config->width = frame->width;
    config->height = frame->height;
    config->linesize = frame->linesize;

    switch (frame->pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
    case SXPLAYER_PIXFMT_BGRA:
    case SXPLAYER_SMPFMT_FLT:
        config->data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
        break;
    default:
        ngli_assert(0);
    }

    return 0;
}

int ngli_hwupload_common_init(struct ngl_node *node,
                              struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;

    if (s->hwupload_fmt == config->format)
        return 0;

    s->hwupload_fmt = config->format;
    s->data_format = config->data_format;

    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    ngli_mat4_identity(s->coordinates_matrix);

    return 0;
}

int ngli_hwupload_common_upload(struct ngl_node *node,
                                struct hwupload_config *config,
                                struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;

    const int linesize       = config->linesize >> 2;
    s->coordinates_matrix[0] = linesize ? config->width / (float)linesize : 1.0;

    ngli_texture_update_local_texture(node, linesize, config->height, 0, frame->data);

    return 0;
}

void ngli_hwupload_common_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    s->hwupload_fmt = NGLI_HWUPLOAD_FMT_NONE;
}
