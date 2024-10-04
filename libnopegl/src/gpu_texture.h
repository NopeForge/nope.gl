/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef GPU_TEXTURE_H
#define GPU_TEXTURE_H

#include "utils.h"

struct gpu_ctx;

enum {
    NGLI_GPU_MIPMAP_FILTER_NONE,
    NGLI_GPU_MIPMAP_FILTER_NEAREST,
    NGLI_GPU_MIPMAP_FILTER_LINEAR,
    NGLI_GPU_NB_MIPMAP
};

enum {
    NGLI_GPU_FILTER_NEAREST,
    NGLI_GPU_FILTER_LINEAR,
    NGLI_GPU_NB_FILTER
};

enum {
    NGLI_GPU_WRAP_CLAMP_TO_EDGE,
    NGLI_GPU_WRAP_MIRRORED_REPEAT,
    NGLI_GPU_WRAP_REPEAT,
    NGLI_GPU_NB_WRAP
};

enum {
    NGLI_GPU_TEXTURE_USAGE_TRANSFER_SRC_BIT             = 1 << 0,
    NGLI_GPU_TEXTURE_USAGE_TRANSFER_DST_BIT             = 1 << 1,
    NGLI_GPU_TEXTURE_USAGE_SAMPLED_BIT                  = 1 << 2,
    NGLI_GPU_TEXTURE_USAGE_STORAGE_BIT                  = 1 << 3,
    NGLI_GPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         = 1 << 4,
    NGLI_GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 1 << 5,
    NGLI_GPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT     = 1 << 6,
};

enum texture_type {
    NGLI_GPU_TEXTURE_TYPE_2D,
    NGLI_GPU_TEXTURE_TYPE_2D_ARRAY,
    NGLI_GPU_TEXTURE_TYPE_3D,
    NGLI_GPU_TEXTURE_TYPE_CUBE,
    NGLI_GPU_TEXTURE_TYPE_NB
};

NGLI_STATIC_ASSERT(texture_params_type_default, NGLI_GPU_TEXTURE_TYPE_2D == 0);
NGLI_STATIC_ASSERT(texture_params_filter_default, NGLI_GPU_FILTER_NEAREST == 0);
NGLI_STATIC_ASSERT(texture_params_mipmap_filter_default, NGLI_GPU_MIPMAP_FILTER_NONE == 0);
NGLI_STATIC_ASSERT(texture_params_wrap_default, NGLI_GPU_WRAP_CLAMP_TO_EDGE == 0);

struct gpu_texture_params {
    enum texture_type type;
    int format;
    int32_t width;
    int32_t height;
    int32_t depth;
    int32_t samples;
    int min_filter;
    int mag_filter;
    int mipmap_filter;
    int wrap_s;
    int wrap_t;
    int wrap_r;
    uint32_t usage;
};

struct gpu_texture {
    struct ngli_rc rc;
    struct gpu_ctx *gpu_ctx;
    struct gpu_texture_params params;
};

NGLI_RC_CHECK_STRUCT(gpu_texture);

struct gpu_texture *ngli_gpu_texture_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_texture_init(struct gpu_texture *s, const struct gpu_texture_params *params);
int ngli_gpu_texture_upload(struct gpu_texture *s, const uint8_t *data, int linesize);
int ngli_gpu_texture_generate_mipmap(struct gpu_texture *s);
void ngli_gpu_texture_freep(struct gpu_texture **sp);

#endif
