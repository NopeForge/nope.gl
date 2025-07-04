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

#ifndef NGPU_RENDERTARGET_H
#define NGPU_RENDERTARGET_H

#include "limits.h"
#include "utils/utils.h"
#include "texture.h"
#include "utils/refcount.h"

enum ngpu_load_op {
    NGPU_LOAD_OP_LOAD,
    NGPU_LOAD_OP_CLEAR,
    NGPU_LOAD_OP_DONT_CARE,
    NGPU_LOAD_OP_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_store_op {
    NGPU_STORE_OP_STORE,
    NGPU_STORE_OP_DONT_CARE,
    NGPU_STORE_OP_MAX_ENUM = 0x7FFFFFFF
};

struct ngpu_rendertarget_layout_entry {
    enum ngpu_format format;
    int resolve;
};

struct ngpu_rendertarget_layout {
    uint32_t samples;
    size_t nb_colors;
    struct ngpu_rendertarget_layout_entry colors[NGPU_MAX_COLOR_ATTACHMENTS];
    struct ngpu_rendertarget_layout_entry depth_stencil;
};

struct ngpu_attachment {
    struct ngpu_texture *attachment;
    uint32_t attachment_layer;
    struct ngpu_texture *resolve_target;
    uint32_t resolve_target_layer;
    enum ngpu_load_op load_op;
    float clear_value[4];
    enum ngpu_store_op store_op;
};

struct ngpu_rendertarget_params {
    uint32_t width;
    uint32_t height;
    size_t nb_colors;
    struct ngpu_attachment colors[NGPU_MAX_COLOR_ATTACHMENTS];
    struct ngpu_attachment depth_stencil;
};

struct ngpu_rendertarget {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    struct ngpu_rendertarget_params params;
    uint32_t width;
    uint32_t height;
    struct ngpu_rendertarget_layout layout;
};

NGLI_RC_CHECK_STRUCT(ngpu_rendertarget);

struct ngpu_rendertarget *ngpu_rendertarget_create(struct ngpu_ctx *gpu_ctx);
int ngpu_rendertarget_init(struct ngpu_rendertarget *s, const struct ngpu_rendertarget_params *params);
void ngpu_rendertarget_freep(struct ngpu_rendertarget **sp);

#endif
