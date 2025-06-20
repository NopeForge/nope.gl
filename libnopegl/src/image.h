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

#ifndef IMAGE_H
#define IMAGE_H

#include <nopemd.h>

#include "ngpu/texture.h"
#include "utils/utils.h"

#define NGLI_COLOR_INFO_DEFAULTS {             \
    .space     = NMD_COL_SPC_UNSPECIFIED,      \
    .range     = NMD_COL_RNG_UNSPECIFIED,      \
    .primaries = NMD_COL_PRI_UNSPECIFIED,      \
    .transfer  = NMD_COL_TRC_UNSPECIFIED,      \
}                                              \

struct color_info {
    int space;
    int range;
    int primaries;
    int transfer;
};

struct color_info ngli_color_info_from_nopemd_frame(const struct nmd_frame *frame);

enum image_layout {
    NGLI_IMAGE_LAYOUT_NONE           = 0,
    NGLI_IMAGE_LAYOUT_DEFAULT        = 1,
    NGLI_IMAGE_LAYOUT_MEDIACODEC     = 2,
    NGLI_IMAGE_LAYOUT_NV12           = 3,
    NGLI_IMAGE_LAYOUT_NV12_RECTANGLE = 4,
    NGLI_IMAGE_LAYOUT_YUV            = 5,
    NGLI_IMAGE_LAYOUT_RECTANGLE      = 6,
    NGLI_NB_IMAGE_LAYOUTS
};

enum {
    NGLI_IMAGE_LAYOUT_DEFAULT_BIT        = 1U << NGLI_IMAGE_LAYOUT_DEFAULT,
    NGLI_IMAGE_LAYOUT_MEDIACODEC_BIT     = 1U << NGLI_IMAGE_LAYOUT_MEDIACODEC,
    NGLI_IMAGE_LAYOUT_NV12_BIT           = 1U << NGLI_IMAGE_LAYOUT_NV12,
    NGLI_IMAGE_LAYOUT_NV12_RECTANGLE_BIT = 1U << NGLI_IMAGE_LAYOUT_NV12_RECTANGLE,
    NGLI_IMAGE_LAYOUT_YUV_BIT            = 1U << NGLI_IMAGE_LAYOUT_YUV,
    NGLI_IMAGE_LAYOUT_RECTANGLE_BIT      = 1U << NGLI_IMAGE_LAYOUT_RECTANGLE,
    NGLI_IMAGE_LAYOUT_ALL_BIT            = 0xFF,
};

struct image_params {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    float color_scale;
    enum image_layout layout;
    struct color_info color_info;
};

struct image {
    struct image_params params;
    struct ngpu_texture *planes[4];
    void *samplers[4];
    size_t nb_planes;
    NGLI_ALIGNED_MAT(color_matrix);
    /* mutable fields after initialization */
    NGLI_ALIGNED_MAT(coordinates_matrix);
    float ts;
    size_t rev;
};

void ngli_image_init(struct image *s, const struct image_params *params, struct ngpu_texture **planes);
void ngli_image_reset(struct image *s);
uint64_t ngli_image_get_memory_size(const struct image *s);

#endif
