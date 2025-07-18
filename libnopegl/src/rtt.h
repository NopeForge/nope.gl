/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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

#ifndef RTT_H
#define RTT_H

#include <stdint.h>

#include "image.h"
#include "ngpu/limits.h"
#include "ngpu/rendertarget.h"

struct ngl_ctx;
struct rtt_ctx;

struct rtt_params {
    uint32_t width;
    uint32_t height;
    uint32_t samples;
    int nb_interruptions;

    size_t nb_colors;
    struct ngpu_attachment colors[NGPU_MAX_COLOR_ATTACHMENTS];

    enum ngpu_format depth_stencil_format;
    struct ngpu_attachment depth_stencil;
};

struct rtt_ctx *ngli_rtt_create(struct ngl_ctx *ctx);
int ngli_rtt_init(struct rtt_ctx *s, const struct rtt_params *params);
int ngli_rtt_from_texture_params(struct rtt_ctx *s, const struct ngpu_texture_params *params);
void ngli_rtt_get_dimensions(struct rtt_ctx *s, uint32_t *width, uint32_t *height);
struct ngpu_texture *ngli_rtt_get_texture(struct rtt_ctx *s, size_t index);
struct image *ngli_rtt_get_image(struct rtt_ctx *s, size_t index);
void ngli_rtt_begin(struct rtt_ctx *s);
void ngli_rtt_end(struct rtt_ctx *s);
void ngli_rtt_freep(struct rtt_ctx **sp);

#endif
