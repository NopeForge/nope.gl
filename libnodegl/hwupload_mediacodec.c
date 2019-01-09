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

#include <libavcodec/mediacodec.h>

#include "android_surface.h"
#include "format.h"
#include "glincludes.h"
#include "hwconv.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

struct hwupload_mc {
    struct hwconv hwconv;
};

static int mc_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload_mc *mc = s->hwupload_priv_data;

    s->data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
    int ret = ngli_format_get_gl_texture_format(gl,
                                                s->data_format,
                                                &s->format,
                                                &s->internal_format,
                                                &s->type);
    if (ret < 0)
        return ret;

    ret = ngli_node_texture_update_data(node, frame->width, frame->height, 0, NULL);
    if (ret < 0)
        return ret;

    ret = ngli_hwconv_init(&mc->hwconv, gl,
                           s->id, s->data_format, s->width, s->height,
                           NGLI_TEXTURE_LAYOUT_MEDIACODEC);
    if (ret < 0)
        return ret;

    return 0;
}

static void mc_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_mc *mc = s->hwupload_priv_data;

    ngli_hwconv_reset(&mc->hwconv);
}

static int mc_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload_mc *mc = s->hwupload_priv_data;

    struct media_priv *media = s->data_src->priv_data;
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;

    int ret = ngli_node_texture_update_data(node, frame->width, frame->height, 0, NULL);
    if (ret < 0)
        return ret;

    if (ret) {
        mc_uninit(node);
        ret = mc_init(node, frame);
        if (ret < 0)
            return ret;
    }

    NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);

    NGLI_ALIGNED_MAT(flip_matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    ngli_mat4_mul(matrix, flip_matrix, matrix);

    const struct texture_plane plane = {
        .id = media->android_texture_id,
        .target = media->android_texture_target,
    };
    ret = ngli_hwconv_convert(&mc->hwconv, &plane, matrix);
    if (ret < 0)
        return ret;

    return 0;
}

static int mc_dr_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct media_priv *media = s->data_src->priv_data;

    GLint id = media->android_texture_id;
    GLenum target = media->android_texture_target;

    ngli_glBindTexture(gl, target, id);
    ngli_glTexParameteri(gl, target, GL_TEXTURE_MIN_FILTER, s->min_filter);
    ngli_glTexParameteri(gl, target, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    ngli_glBindTexture(gl, target, 0);

    s->layout = NGLI_TEXTURE_LAYOUT_MEDIACODEC;
    s->planes[0].id = id;
    s->planes[0].target = target;

    return 0;
}

static int mc_dr_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    struct media_priv *media = s->data_src->priv_data;
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;

    NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    NGLI_ALIGNED_MAT(flip_matrix) = {
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  1.0f, 0.0f, 1.0f,
    };

    s->width  = frame->width;
    s->height = frame->height;

    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);
    ngli_mat4_mul(s->coordinates_matrix, flip_matrix, matrix);

    return 0;
}

static const struct hwmap_class hwmap_mc_class = {
    .name      = "mediacodec (oes → 2d)",
    .priv_size = sizeof(struct hwupload_mc),
    .init      = mc_init,
    .map_frame = mc_map_frame,
    .uninit    = mc_uninit,
};

static const struct hwmap_class hwmap_mc_dr_class = {
    .name      = "mediacodec (oes zero-copy)",
    .init      = mc_dr_init,
    .map_frame = mc_dr_map_frame,
};

static const struct hwmap_class *mc_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;

    if (s->direct_rendering) {
        if (s->min_filter != GL_NEAREST && s->min_filter != GL_LINEAR) {
            LOG(WARNING, "external textures only support nearest and linear filtering: "
                "disabling direct rendering");
            s->direct_rendering = 0;
        } else if (s->wrap_s != GL_CLAMP_TO_EDGE || s->wrap_t != GL_CLAMP_TO_EDGE) {
            LOG(WARNING, "external textures only support clamp to edge wrapping: "
                "disabling direct rendering");
            s->direct_rendering = 0;
        }
    }

    if (s->direct_rendering)
        return &hwmap_mc_dr_class;

    return &hwmap_mc_class;
}

const struct hwupload_class ngli_hwupload_mc_class = {
    .get_hwmap = mc_get_hwmap,
};
