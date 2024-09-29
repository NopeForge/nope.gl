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

#ifndef GPU_RENDERTARGET_H
#define GPU_RENDERTARGET_H

#include "gpu_limits.h"
#include "gpu_texture.h"
#include "utils.h"

enum {
    NGLI_GPU_LOAD_OP_LOAD,
    NGLI_GPU_LOAD_OP_CLEAR,
    NGLI_GPU_LOAD_OP_DONT_CARE,
};

enum {
    NGLI_GPU_STORE_OP_STORE,
    NGLI_GPU_STORE_OP_DONT_CARE,
};

struct gpu_rendertarget_layout_entry {
    int format;
    int resolve;
};

struct gpu_rendertarget_layout {
    int32_t samples;
    size_t nb_colors;
    struct gpu_rendertarget_layout_entry colors[NGLI_GPU_MAX_COLOR_ATTACHMENTS];
    struct gpu_rendertarget_layout_entry depth_stencil;
};

struct gpu_attachment {
    struct gpu_texture *attachment;
    int32_t attachment_layer;
    struct gpu_texture *resolve_target;
    int32_t resolve_target_layer;
    int load_op;
    float clear_value[4];
    int store_op;
};

struct gpu_rendertarget_params {
    int32_t width;
    int32_t height;
    size_t nb_colors;
    struct gpu_attachment colors[NGLI_GPU_MAX_COLOR_ATTACHMENTS];
    struct gpu_attachment depth_stencil;
};

struct gpu_rendertarget {
    struct ngli_rc rc;
    struct gpu_ctx *gpu_ctx;
    struct gpu_rendertarget_params params;
    int32_t width;
    int32_t height;
    struct gpu_rendertarget_layout layout;
};

NGLI_RC_CHECK_STRUCT(gpu_rendertarget);

struct gpu_rendertarget *ngli_gpu_rendertarget_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_rendertarget_init(struct gpu_rendertarget *s, const struct gpu_rendertarget_params *params);
void ngli_gpu_rendertarget_freep(struct gpu_rendertarget **sp);

#endif
