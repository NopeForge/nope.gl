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
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

extern const struct hwupload_class ngli_hwupload_common_class;
extern const struct hwupload_class ngli_hwupload_mc_class;
extern const struct hwupload_class ngli_hwupload_vt_darwin_class;
extern const struct hwupload_class ngli_hwupload_vt_ios_class;
extern const struct hwupload_class ngli_hwupload_vaapi_class;

static const struct hwupload_class *hwupload_class_map[] = {
    [SXPLAYER_PIXFMT_RGBA]        = &ngli_hwupload_common_class,
    [SXPLAYER_PIXFMT_BGRA]        = &ngli_hwupload_common_class,
    [SXPLAYER_SMPFMT_FLT]         = &ngli_hwupload_common_class,
#if defined(TARGET_ANDROID)
    [SXPLAYER_PIXFMT_MEDIACODEC]  = &ngli_hwupload_mc_class,
#elif defined(TARGET_DARWIN)
    [SXPLAYER_PIXFMT_VT]          = &ngli_hwupload_vt_darwin_class,
#elif defined(TARGET_IPHONE)
    [SXPLAYER_PIXFMT_VT]          = &ngli_hwupload_vt_ios_class,
#elif defined(HAVE_VAAPI_X11)
    [SXPLAYER_PIXFMT_VAAPI]       = &ngli_hwupload_vaapi_class,
#endif
};

static const struct hwmap_class *get_hwmap_class(struct ngl_node *node, struct sxplayer_frame *frame)
{
    if (frame->pix_fmt < 0 || frame->pix_fmt >= NGLI_ARRAY_NB(hwupload_class_map))
        return NULL;

    const struct hwupload_class *cls = hwupload_class_map[frame->pix_fmt];
    if (!cls)
        return NULL;
    return cls->get_hwmap(node, frame);
}

int ngli_hwupload_upload_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct media_priv *media = s->data_src->priv_data;
    struct sxplayer_frame *frame = media->frame;
    if (!frame)
        return 0;
    media->frame = NULL;

    s->image.ts = frame->ts;

    const struct hwmap_class *hwmap_class = get_hwmap_class(node, frame);
    if (!hwmap_class) {
        sxplayer_release_frame(frame);
        return -1;
    }

    if (s->hwupload_map_class != hwmap_class) {
        ngli_hwupload_uninit(node);

        if (hwmap_class->priv_size) {
            s->hwupload_priv_data = ngli_calloc(1, hwmap_class->priv_size);
            if (!s->hwupload_priv_data) {
                sxplayer_release_frame(frame);
                return -1;
            }
        }

        int ret = hwmap_class->init(node, frame);
        if (ret < 0) {
            sxplayer_release_frame(frame);
            return ret;
        }
        s->hwupload_map_class = hwmap_class;

        LOG(DEBUG, "mapping texture '%s' with method: %s", node->label, hwmap_class->name);
    }

    int ret = hwmap_class->map_frame(node, frame);
    if (!(hwmap_class->flags &  HWMAP_FLAG_FRAME_OWNER))
        sxplayer_release_frame(frame);
    return ret;
}

void ngli_hwupload_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    if (s->hwupload_map_class && s->hwupload_map_class->uninit) {
        s->hwupload_map_class->uninit(node);
    }
    ngli_free(s->hwupload_priv_data);
    s->hwupload_priv_data = NULL;
    s->hwupload_map_class = NULL;
    ngli_image_reset(&s->image);
}
