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

#include "format.h"

static const struct {
    size_t nb_comp;
    size_t size;
} format_comp_sizes[NGPU_FORMAT_NB] = {
    [NGPU_FORMAT_R8_UNORM]            = {1, 1},
    [NGPU_FORMAT_R8_SNORM]            = {1, 1},
    [NGPU_FORMAT_R8_UINT]             = {1, 1},
    [NGPU_FORMAT_R8_SINT]             = {1, 1},
    [NGPU_FORMAT_R8G8_UNORM]          = {2, 1 + 1},
    [NGPU_FORMAT_R8G8_SNORM]          = {2, 1 + 1},
    [NGPU_FORMAT_R8G8_UINT]           = {2, 1 + 1},
    [NGPU_FORMAT_R8G8_SINT]           = {2, 1 + 1},
    [NGPU_FORMAT_R8G8B8_UNORM]        = {3, 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8_SNORM]        = {3, 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8_UINT]         = {3, 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8_SINT]         = {3, 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8_SRGB]         = {3, 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8A8_UNORM]      = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8A8_SNORM]      = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8A8_UINT]       = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8A8_SINT]       = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_R8G8B8A8_SRGB]       = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_B8G8R8A8_UNORM]      = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_B8G8R8A8_SNORM]      = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_B8G8R8A8_UINT]       = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_B8G8R8A8_SINT]       = {4, 1 + 1 + 1 + 1},
    [NGPU_FORMAT_R16_UNORM]           = {1, 2},
    [NGPU_FORMAT_R16_SNORM]           = {1, 2},
    [NGPU_FORMAT_R16_UINT]            = {1, 2},
    [NGPU_FORMAT_R16_SINT]            = {1, 2},
    [NGPU_FORMAT_R16_SFLOAT]          = {1, 2},
    [NGPU_FORMAT_R16G16_UNORM]        = {2, 2 + 2},
    [NGPU_FORMAT_R16G16_SNORM]        = {2, 2 + 2},
    [NGPU_FORMAT_R16G16_UINT]         = {2, 2 + 2},
    [NGPU_FORMAT_R16G16_SINT]         = {2, 2 + 2},
    [NGPU_FORMAT_R16G16_SFLOAT]       = {2, 2 + 2},
    [NGPU_FORMAT_R16G16B16_UNORM]     = {3, 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16_SNORM]     = {3, 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16_UINT]      = {3, 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16_SINT]      = {3, 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16_SFLOAT]    = {3, 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16A16_UNORM]  = {4, 2 + 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16A16_SNORM]  = {4, 2 + 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16A16_UINT]   = {4, 2 + 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16A16_SINT]   = {4, 2 + 2 + 2 + 2},
    [NGPU_FORMAT_R16G16B16A16_SFLOAT] = {4, 2 + 2 + 2 + 2},
    [NGPU_FORMAT_R32_UINT]            = {1, 4},
    [NGPU_FORMAT_R32_SINT]            = {1, 4},
    [NGPU_FORMAT_R64_SINT]            = {1, 8},
    [NGPU_FORMAT_R32_SFLOAT]          = {1, 4},
    [NGPU_FORMAT_R32G32_UINT]         = {2, 4 + 4},
    [NGPU_FORMAT_R32G32_SINT]         = {2, 4 + 4},
    [NGPU_FORMAT_R32G32_SFLOAT]       = {2, 4 + 4},
    [NGPU_FORMAT_R32G32B32_UINT]      = {3, 4 + 4 + 4},
    [NGPU_FORMAT_R32G32B32_SINT]      = {3, 4 + 4 + 4},
    [NGPU_FORMAT_R32G32B32_SFLOAT]    = {3, 4 + 4 + 4},
    [NGPU_FORMAT_R32G32B32A32_UINT]   = {4, 4 + 4 + 4 + 4},
    [NGPU_FORMAT_R32G32B32A32_SINT]   = {4, 4 + 4 + 4 + 4},
    [NGPU_FORMAT_R32G32B32A32_SFLOAT] = {4, 4 + 4 + 4 + 4},
    [NGPU_FORMAT_D16_UNORM]           = {1, 2},
    [NGPU_FORMAT_X8_D24_UNORM_PACK32] = {2, 3 + 1},
    [NGPU_FORMAT_D32_SFLOAT]          = {1, 4},
    [NGPU_FORMAT_D24_UNORM_S8_UINT]   = {2, 3 + 1},
    [NGPU_FORMAT_D32_SFLOAT_S8_UINT]  = {3, 4 + 1 + 3},
    [NGPU_FORMAT_S8_UINT]             = {1, 1},
};

size_t ngpu_format_get_bytes_per_pixel(enum ngpu_format format)
{
    return format_comp_sizes[format].size;
}

size_t ngpu_format_get_nb_comp(enum ngpu_format format)
{
    return format_comp_sizes[format].nb_comp;
}

int ngpu_format_has_depth(enum ngpu_format format)
{
    switch (format) {
    case NGPU_FORMAT_D16_UNORM:
    case NGPU_FORMAT_X8_D24_UNORM_PACK32:
    case NGPU_FORMAT_D32_SFLOAT:
    case NGPU_FORMAT_D24_UNORM_S8_UINT:
    case NGPU_FORMAT_D32_SFLOAT_S8_UINT:
        return 1;
    default:
        return 0;
    }
}

int ngpu_format_has_stencil(enum ngpu_format format)
{
    switch (format) {
    case NGPU_FORMAT_X8_D24_UNORM_PACK32:
    case NGPU_FORMAT_D24_UNORM_S8_UINT:
    case NGPU_FORMAT_D32_SFLOAT_S8_UINT:
    case NGPU_FORMAT_S8_UINT:
        return 1;
    default:
        return 0;
    }
}
