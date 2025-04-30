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

#ifndef NGPU_TEXTURE_H
#define NGPU_TEXTURE_H

#include "format.h"
#include "utils/utils.h"
#include "utils/refcount.h"

struct ngpu_ctx;

enum ngpu_mipmap_filter {
    NGPU_MIPMAP_FILTER_NONE = 0,
    NGPU_MIPMAP_FILTER_NEAREST,
    NGPU_MIPMAP_FILTER_LINEAR,
    NGPU_NB_MIPMAP,
    NGPU_MIPMAP_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_filter {
    NGPU_FILTER_NEAREST = 0,
    NGPU_FILTER_LINEAR,
    NGPU_NB_FILTER,
    NGPU_FILTER_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_wrap {
    NGPU_WRAP_CLAMP_TO_EDGE = 0,
    NGPU_WRAP_MIRRORED_REPEAT,
    NGPU_WRAP_REPEAT,
    NGPU_NB_WRAP,
    NGPU_WRAP_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_texture_usage {
    NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT             = 1 << 0,
    NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT             = 1 << 1,
    NGPU_TEXTURE_USAGE_SAMPLED_BIT                  = 1 << 2,
    NGPU_TEXTURE_USAGE_STORAGE_BIT                  = 1 << 3,
    NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         = 1 << 4,
    NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 1 << 5,
    NGPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT     = 1 << 6,
    NGPU_TEXTURE_USAGE_MAX_ENUM                     = 0x7FFFFFFF
};

enum ngpu_texture_type {
    NGPU_TEXTURE_TYPE_2D = 0,
    NGPU_TEXTURE_TYPE_2D_ARRAY,
    NGPU_TEXTURE_TYPE_3D,
    NGPU_TEXTURE_TYPE_CUBE,
    NGPU_TEXTURE_TYPE_NB,
    NGPU_TEXTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_texture_params {
    enum ngpu_texture_type type;
    enum ngpu_format format;
    int32_t width;
    int32_t height;
    int32_t depth;
    int32_t samples;
    enum ngpu_filter min_filter;
    enum ngpu_filter mag_filter;
    enum ngpu_mipmap_filter mipmap_filter;
    enum ngpu_wrap wrap_s;
    enum ngpu_wrap wrap_t;
    enum ngpu_wrap wrap_r;
    uint32_t usage;
};

struct ngpu_texture {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    struct ngpu_texture_params params;
};

struct ngpu_texture_transfer_params {
    int32_t pixels_per_row;
    int32_t x, y, z;
    int32_t width, height, depth;
    int32_t base_layer;
    int32_t layer_count;
};

NGLI_RC_CHECK_STRUCT(ngpu_texture);

struct ngpu_texture *ngpu_texture_create(struct ngpu_ctx *gpu_ctx);
int ngpu_texture_init(struct ngpu_texture *s, const struct ngpu_texture_params *params);
int ngpu_texture_upload(struct ngpu_texture *s, const uint8_t *data, int linesize);
int ngpu_texture_upload_with_params(struct ngpu_texture *s, const uint8_t *data, const struct ngpu_texture_transfer_params *transfer_params);
int ngpu_texture_generate_mipmap(struct ngpu_texture *s);
void ngpu_texture_freep(struct ngpu_texture **sp);

#endif
