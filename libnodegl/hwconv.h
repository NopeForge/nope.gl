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

#ifndef HWCONV_H
#define HWCONV_H

#include "rendertarget.h"
#include "glincludes.h"
#include "glcontext.h"
#include "image.h"
#include "program.h"
#include "texture.h"

struct hwconv {
    struct ngl_ctx *ctx;
    enum image_layout src_layout;

    struct rendertarget rt;
    struct texture color_attachment;
    struct program program;

    GLuint vao_id;
    GLuint vertices_id;
    GLint position_location;
    GLint texture_locations[2];
    GLint texture_matrix_location;
    GLint texture_dimensions_location;
};

int ngli_hwconv_init(struct hwconv *hwconv, struct ngl_ctx *ctx,
                     const struct texture *dst_texture,
                     enum image_layout src_layout);

int ngli_hwconv_convert(struct hwconv *hwconv, const struct texture *planes, const float *matrix);
void ngli_hwconv_reset(struct hwconv *texconv);

#endif
