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

#ifndef HWCONV_H
#define HWCONV_H

#include "image.h"
#include "ngpu/buffer.h"
#include "ngpu/rendertarget.h"
#include "ngpu/texture.h"
#include "pipeline_compat.h"
#include "ngpu/pgcraft.h"

struct ngl_ctx;

struct hwconv {
    struct ngl_ctx *ctx;
    struct image_params src_params;

    struct ngpu_rendertarget *rt;
    struct ngpu_pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
};

int ngli_hwconv_init(struct hwconv *hwconv, struct ngl_ctx *ctx,
                     const struct image *dst_image,
                     const struct image_params *src_params);

int ngli_hwconv_convert_image(struct hwconv *hwconv, const struct image *image);
void ngli_hwconv_reset(struct hwconv *texconv);

#endif
