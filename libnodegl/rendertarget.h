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

#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "darray.h"
#include "glcontext.h"
#include "texture.h"

#define NGLI_MAX_COLOR_ATTACHMENTS 8

struct rendertarget_params {
    int width;
    int height;
    int nb_colors;
    struct texture *colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct texture *depth_stencil;
};

struct rendertarget {
    struct ngl_ctx *ctx;
    int width;
    int height;
    int nb_color_attachments;

    GLuint id;
    GLuint prev_id;
    GLenum draw_buffers[NGLI_MAX_COLOR_ATTACHMENTS];
    GLenum blit_draw_buffers[NGLI_MAX_COLOR_ATTACHMENTS*(NGLI_MAX_COLOR_ATTACHMENTS+1)/2];
    void (*blit)(struct rendertarget *s, struct rendertarget *dst, int vflip);
};

int ngli_rendertarget_init(struct rendertarget *s, struct ngl_ctx *ctx, const struct rendertarget_params *params);
void ngli_rendertarget_blit(struct rendertarget *s, struct rendertarget *dst, int vflip);
void ngli_rendertarget_read_pixels(struct rendertarget *s, uint8_t *data);
void ngli_rendertarget_reset(struct rendertarget *s);

#endif
