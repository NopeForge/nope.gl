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

#include <CoreVideo/CoreVideo.h>

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

static int vt_get_data_format(struct sxplayer_frame *frame)
{
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
        return NGLI_FORMAT_B8G8R8A8_UNORM;
    case kCVPixelFormatType_32RGBA:
        return NGLI_FORMAT_R8G8B8A8_UNORM;
    default:
        return -1;
    }
}

static int vt_darwin_init(struct ngl_node *node, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;

    s->data_format = vt_get_data_format(frame);
    if (s->data_format < 0)
        return -1;

    return ngli_format_get_gl_format_type(gl,
                                          s->data_format,
                                          &s->format,
                                          &s->internal_format,
                                          &s->type);
}

static int vt_darwin_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    CVPixelBufferLockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    uint8_t *data = CVPixelBufferGetBaseAddress(cvpixbuf);
    const int width = CVPixelBufferGetWidth(cvpixbuf);
    const int height = CVPixelBufferGetHeight(cvpixbuf);
    const int linesize = CVPixelBufferGetBytesPerRow(cvpixbuf) >> 2;
    s->coordinates_matrix[0] = linesize ? width / (float)linesize : 1.0;

    ngli_texture_update_data(node, linesize, height, 0, data);

    CVPixelBufferUnlockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    return 0;
}

static const struct hwmap_class hwmap_vt_darwin_class = {
    .name      = "videotoolbox (copy)",
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
};

static const struct hwmap_class *vt_darwin_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    return &hwmap_vt_darwin_class;
}

const struct hwupload_class ngli_hwupload_vt_darwin_class = {
    .get_hwmap = vt_darwin_get_hwmap,
};
