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

#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "darray.h"
#include "gpu_limits.h"
#include "texture.h"

enum {
    NGLI_LOAD_OP_LOAD,
    NGLI_LOAD_OP_CLEAR,
    NGLI_LOAD_OP_DONT_CARE,
};

enum {
    NGLI_STORE_OP_STORE,
    NGLI_STORE_OP_DONT_CARE,
};

struct rendertarget_layout_entry {
    int format;
    int resolve;
};

struct rendertarget_layout {
    int32_t samples;
    size_t nb_colors;
    struct rendertarget_layout_entry colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct rendertarget_layout_entry depth_stencil;
};

struct attachment {
    struct texture *attachment;
    int32_t attachment_layer;
    struct texture *resolve_target;
    int32_t resolve_target_layer;
    int load_op;
    float clear_value[4];
    int store_op;
};

struct rendertarget_params {
    int32_t width;
    int32_t height;
    size_t nb_colors;
    struct attachment colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct attachment depth_stencil;
};

struct rendertarget {
    struct gpu_ctx *gpu_ctx;
    struct rendertarget_params params;
    int32_t width;
    int32_t height;
    struct rendertarget_layout layout;
};

struct rendertarget *ngli_rendertarget_create(struct gpu_ctx *gpu_ctx);
int ngli_rendertarget_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_freep(struct rendertarget **sp);

#endif
