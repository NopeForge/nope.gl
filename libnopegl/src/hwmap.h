/*
 * Copyright 2017-2022 GoPro Inc.
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

#ifndef HWMAP_H
#define HWMAP_H

#include <stdlib.h>
#include <stdint.h>
#include <nopemd.h>

#include "hwconv.h"
#include "image.h"
#include "nopegl.h"

#define HWMAP_FLAG_FRAME_OWNER (1U << 0)

struct hwmap_params {
    const char *label;
    uint32_t image_layouts;
    enum ngpu_filter texture_min_filter;
    enum ngpu_filter texture_mag_filter;
    enum ngpu_mipmap_filter texture_mipmap_filter;
    enum ngpu_wrap texture_wrap_s;
    enum ngpu_wrap texture_wrap_t;
    uint32_t texture_usage;
#if defined(TARGET_ANDROID)
    struct android_imagereader *android_imagereader;
#endif
};

struct hwmap {
    struct ngl_ctx *ctx;
    struct hwmap_params params;
    const struct hwmap_class **hwmap_classes;
    const struct hwmap_class *hwmap_class;
    void *hwmap_priv_data;
    int pix_fmt;
    int32_t width;
    int32_t height;
    struct image mapped_image;
    int require_hwconv;
    struct hwconv hwconv;
    struct ngpu_texture *hwconv_texture;
    struct image hwconv_image;
    int hwconv_initialized;
};

struct hwmap_class {
    const char *name;
    uint32_t flags;
    int hwformat;
    const enum image_layout *layouts;
    size_t priv_size;
    int (*init)(struct hwmap *hwmap, struct nmd_frame *frame);
    int (*map_frame)(struct hwmap *hwmap, struct nmd_frame *frame);
    void (*uninit)(struct hwmap *hwmap);
};

int ngli_hwmap_is_image_layout_supported(enum ngl_backend_type backend, enum image_layout image_layout);

int ngli_hwmap_init(struct hwmap *hwmap, struct ngl_ctx *ctx, const struct hwmap_params *params);
int ngli_hwmap_map_frame(struct hwmap *hwmap, struct nmd_frame *frame, struct image *image);
void ngli_hwmap_uninit(struct hwmap *hwmap);

#endif /* HWUPLOAD_H */
