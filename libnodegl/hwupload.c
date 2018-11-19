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

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

#include "hwupload_common.h"
#if defined(TARGET_ANDROID)
#include "hwupload_mediacodec.h"
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
#include "hwupload_videotoolbox.h"
#endif

static int get_config_from_frame(struct ngl_node *node, struct sxplayer_frame *frame, struct hwupload_config *config)
{
    switch (frame->pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
    case SXPLAYER_PIXFMT_BGRA:
    case SXPLAYER_SMPFMT_FLT:
        ngli_hwupload_common_get_config_from_frame(node, frame, config);
        break;
#if defined(TARGET_ANDROID)
    case SXPLAYER_PIXFMT_MEDIACODEC:
        ngli_hwupload_mc_get_config_from_frame(node, frame, config);
        break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case SXPLAYER_PIXFMT_VT:
        ngli_hwupload_vt_get_config_from_frame(node, frame, config);
        break;
#endif
    default:
        ngli_assert(0);
    }

    return 0;
}

static int hwupload_init(struct ngl_node *node, struct hwupload_config *config)
{
    int ret = 0;

    switch (config->format) {
    case NGLI_HWUPLOAD_FMT_COMMON:
        ret = ngli_hwupload_common_init(node, config);
        break;
#if defined(TARGET_ANDROID)
    case NGLI_HWUPLOAD_FMT_MEDIACODEC:
        ret = ngli_hwupload_mc_init(node, config);
        break;
    case NGLI_HWUPLOAD_FMT_MEDIACODEC_DR:
        ret = ngli_hwupload_mc_dr_init(node, config);
        break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12:
        ret = ngli_hwupload_vt_init(node, config);
        break;
# if defined(TARGET_IPHONE)
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12_DR:
        ret = ngli_hwupload_vt_dr_init(node, config);
        break;
# endif
#endif
    default:
        ngli_assert(0);
    }

    return ret;
}

static int hwupload_upload_frame(struct ngl_node *node,
                                 struct hwupload_config *config,
                                 struct sxplayer_frame *frame)
{
    int ret = 0;

    switch (config->format) {
    case NGLI_HWUPLOAD_FMT_COMMON:
        ret = ngli_hwupload_common_upload(node, config, frame);
        break;
#if defined(TARGET_ANDROID)
    case NGLI_HWUPLOAD_FMT_MEDIACODEC:
        ret = ngli_hwupload_mc_upload(node, config, frame);
        break;
    case NGLI_HWUPLOAD_FMT_MEDIACODEC_DR:
        ret = ngli_hwupload_mc_dr_upload(node, config, frame);
        break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12:
        ret = ngli_hwupload_vt_upload(node, config, frame);
        break;
# if defined(TARGET_IPHONE)
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12_DR:
        ret = ngli_hwupload_vt_dr_upload(node, config, frame);
        break;
# endif
#endif
    default:
        ngli_assert(0);
    }

    return ret;
}

int ngli_hwupload_upload_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    if (!frame)
        return 0;

    struct hwupload_config config = {0};
    int ret = get_config_from_frame(node, frame, &config);
    if (ret < 0)
        return ret;

    ret = hwupload_init(node, &config);
    if (ret < 0)
        return ret;

    return hwupload_upload_frame(node, &config, frame);
}

void ngli_hwupload_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    if (s->hwupload_fmt == NGLI_HWUPLOAD_FMT_NONE)
        return;

    switch (s->hwupload_fmt) {
    case NGLI_HWUPLOAD_FMT_COMMON:
        ngli_hwupload_common_uninit(node);
        break;
#if defined(TARGET_ANDROID)
    case NGLI_HWUPLOAD_FMT_MEDIACODEC:
        ngli_hwupload_mc_uninit(node);
        break;
    case NGLI_HWUPLOAD_FMT_MEDIACODEC_DR:
        ngli_hwupload_mc_dr_uninit(node);
        break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12:
        ngli_hwupload_vt_uninit(node);
        break;
# if defined(TARGET_IPHONE)
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12_DR:
        ngli_hwupload_vt_dr_uninit(node);
        break;
# endif
#endif
    default:
        ngli_assert(0);
    }
    ngli_assert(s->hwupload_fmt == NGLI_HWUPLOAD_FMT_NONE);
}
