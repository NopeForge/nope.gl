/*
 * Copyright 2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_FORMAT_H
#define NGPU_FORMAT_H

#include <stddef.h>

enum ngpu_format {
    NGPU_FORMAT_UNDEFINED,
    NGPU_FORMAT_R8_UNORM,
    NGPU_FORMAT_R8_SNORM,
    NGPU_FORMAT_R8_UINT,
    NGPU_FORMAT_R8_SINT,
    NGPU_FORMAT_R8G8_UNORM,
    NGPU_FORMAT_R8G8_SNORM,
    NGPU_FORMAT_R8G8_UINT,
    NGPU_FORMAT_R8G8_SINT,
    NGPU_FORMAT_R8G8B8_UNORM,
    NGPU_FORMAT_R8G8B8_SNORM,
    NGPU_FORMAT_R8G8B8_UINT,
    NGPU_FORMAT_R8G8B8_SINT,
    NGPU_FORMAT_R8G8B8_SRGB,
    NGPU_FORMAT_R8G8B8A8_UNORM,
    NGPU_FORMAT_R8G8B8A8_SNORM,
    NGPU_FORMAT_R8G8B8A8_UINT,
    NGPU_FORMAT_R8G8B8A8_SINT,
    NGPU_FORMAT_R8G8B8A8_SRGB,
    NGPU_FORMAT_B8G8R8A8_UNORM,
    NGPU_FORMAT_B8G8R8A8_SNORM,
    NGPU_FORMAT_B8G8R8A8_UINT,
    NGPU_FORMAT_B8G8R8A8_SINT,
    NGPU_FORMAT_R16_UNORM,
    NGPU_FORMAT_R16_SNORM,
    NGPU_FORMAT_R16_UINT,
    NGPU_FORMAT_R16_SINT,
    NGPU_FORMAT_R16_SFLOAT,
    NGPU_FORMAT_R16G16_UNORM,
    NGPU_FORMAT_R16G16_SNORM,
    NGPU_FORMAT_R16G16_UINT,
    NGPU_FORMAT_R16G16_SINT,
    NGPU_FORMAT_R16G16_SFLOAT,
    NGPU_FORMAT_R16G16B16_UNORM,
    NGPU_FORMAT_R16G16B16_SNORM,
    NGPU_FORMAT_R16G16B16_UINT,
    NGPU_FORMAT_R16G16B16_SINT,
    NGPU_FORMAT_R16G16B16_SFLOAT,
    NGPU_FORMAT_R16G16B16A16_UNORM,
    NGPU_FORMAT_R16G16B16A16_SNORM,
    NGPU_FORMAT_R16G16B16A16_UINT,
    NGPU_FORMAT_R16G16B16A16_SINT,
    NGPU_FORMAT_R16G16B16A16_SFLOAT,
    NGPU_FORMAT_R32_UINT,
    NGPU_FORMAT_R32_SINT,
    NGPU_FORMAT_R32_SFLOAT,
    NGPU_FORMAT_R32G32_UINT,
    NGPU_FORMAT_R32G32_SINT,
    NGPU_FORMAT_R32G32_SFLOAT,
    NGPU_FORMAT_R32G32B32_UINT,
    NGPU_FORMAT_R32G32B32_SINT,
    NGPU_FORMAT_R32G32B32_SFLOAT,
    NGPU_FORMAT_R32G32B32A32_UINT,
    NGPU_FORMAT_R32G32B32A32_SINT,
    NGPU_FORMAT_R32G32B32A32_SFLOAT,
    NGPU_FORMAT_R64_SINT,
    NGPU_FORMAT_D16_UNORM,
    NGPU_FORMAT_X8_D24_UNORM_PACK32,
    NGPU_FORMAT_D32_SFLOAT,
    NGPU_FORMAT_D24_UNORM_S8_UINT,
    NGPU_FORMAT_D32_SFLOAT_S8_UINT,
    NGPU_FORMAT_S8_UINT,
    NGPU_FORMAT_NB,
    NGPU_FORMAT_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_format_feature {
    NGPU_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               = 1 << 0,
    NGPU_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT = 1 << 1,
    NGPU_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT            = 1 << 2,
    NGPU_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT      = 1 << 3,
    NGPU_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT    = 1 << 4,
    NGPU_FORMAT_FEATURE_MAX_ENUM = 0x7FFFFFFF
};

size_t ngpu_format_get_bytes_per_pixel(enum ngpu_format format);

size_t ngpu_format_get_nb_comp(enum ngpu_format format);

int ngpu_format_has_depth(enum ngpu_format format);

int ngpu_format_has_stencil(enum ngpu_format format);

#endif
