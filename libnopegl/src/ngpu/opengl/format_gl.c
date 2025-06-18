/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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

#include <string.h>

#include "feature_gl.h"
#include "format_gl.h"
#include "glcontext.h"
#include "utils/utils.h"

#define S  NGPU_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
#define SL NGPU_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
#define C  NGPU_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
#define B  NGPU_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
#define DS NGPU_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT

static const struct ngpu_format_gl formats[NGPU_FORMAT_NB] = {
    [NGPU_FORMAT_UNDEFINED]            = {0, 0, 0, 0},

    [NGPU_FORMAT_R8_UNORM]             = {GL_RED,         GL_R8,       GL_UNSIGNED_BYTE, S|SL|C|B},
    [NGPU_FORMAT_R8_SNORM]             = {GL_RED,         GL_R8_SNORM, GL_BYTE,          S|SL|B},
    [NGPU_FORMAT_R8_UINT]              = {GL_RED_INTEGER, GL_R8UI,     GL_UNSIGNED_BYTE, S|C},
    [NGPU_FORMAT_R8_SINT]              = {GL_RED_INTEGER, GL_R8I,      GL_BYTE,          S|C},

    [NGPU_FORMAT_R8G8_UNORM]           = {GL_RG,         GL_RG8,       GL_UNSIGNED_BYTE, S|SL|C|B},
    [NGPU_FORMAT_R8G8_SNORM]           = {GL_RG,         GL_RG8_SNORM, GL_BYTE,          S|SL|B},
    [NGPU_FORMAT_R8G8_UINT]            = {GL_RG_INTEGER, GL_RG8UI,     GL_UNSIGNED_BYTE, S|C},
    [NGPU_FORMAT_R8G8_SINT]            = {GL_RG_INTEGER, GL_RG8I,      GL_BYTE,          S|C},

    [NGPU_FORMAT_R8G8B8_UNORM]         = {GL_RGB,         GL_RGB8,       GL_UNSIGNED_BYTE, S|SL|C|B},
    [NGPU_FORMAT_R8G8B8_SNORM]         = {GL_RGB,         GL_RGB8_SNORM, GL_BYTE,          S|SL},
    [NGPU_FORMAT_R8G8B8_UINT]          = {GL_RGB_INTEGER, GL_RGB8UI,     GL_UNSIGNED_BYTE, S|C},
    [NGPU_FORMAT_R8G8B8_SINT]          = {GL_RGB_INTEGER, GL_RGB8I,      GL_BYTE,          S|C},
    [NGPU_FORMAT_R8G8B8_SRGB]          = {GL_RGB,         GL_SRGB8,      GL_UNSIGNED_BYTE, S|SL},

    [NGPU_FORMAT_R8G8B8A8_UNORM]       = {GL_RGBA,         GL_RGBA8,        GL_UNSIGNED_BYTE, S|SL|C|B},
    [NGPU_FORMAT_R8G8B8A8_SNORM]       = {GL_RGBA,         GL_RGBA8_SNORM,  GL_BYTE,          S|SL},
    [NGPU_FORMAT_R8G8B8A8_UINT]        = {GL_RGBA_INTEGER, GL_RGBA8UI,      GL_UNSIGNED_BYTE, S|C},
    [NGPU_FORMAT_R8G8B8A8_SINT]        = {GL_RGBA_INTEGER, GL_RGBA8I,       GL_BYTE,          S|C},
    [NGPU_FORMAT_R8G8B8A8_SRGB]        = {GL_RGBA,         GL_SRGB8_ALPHA8, GL_UNSIGNED_BYTE, S|SL|C|B},

    [NGPU_FORMAT_B8G8R8A8_UNORM]       = {GL_BGRA,         GL_RGBA8,       GL_UNSIGNED_BYTE, 0},
    [NGPU_FORMAT_B8G8R8A8_SNORM]       = {GL_BGRA,         GL_RGBA8_SNORM, GL_BYTE,          0},
    [NGPU_FORMAT_B8G8R8A8_UINT]        = {GL_BGRA_INTEGER, GL_RGBA8UI,     GL_UNSIGNED_BYTE, 0},
    [NGPU_FORMAT_B8G8R8A8_SINT]        = {GL_BGRA_INTEGER, GL_RGBA8I,      GL_BYTE,          0},

    [NGPU_FORMAT_R16_UNORM]            = {GL_RED,          GL_R16,       GL_UNSIGNED_SHORT, 0},
    [NGPU_FORMAT_R16_SNORM]            = {GL_RED,          GL_R16_SNORM, GL_SHORT,          0},
    [NGPU_FORMAT_R16_UINT]             = {GL_RED_INTEGER,  GL_R16UI,     GL_UNSIGNED_SHORT, S|C},
    [NGPU_FORMAT_R16_SINT]             = {GL_RED_INTEGER,  GL_R16I,      GL_SHORT,          S|C},
    [NGPU_FORMAT_R16_SFLOAT]           = {GL_RED,          GL_R16F,      GL_HALF_FLOAT,     S|SL},

    [NGPU_FORMAT_R16G16_UNORM]         = {GL_RG,         GL_RG16,       GL_UNSIGNED_SHORT, 0},
    [NGPU_FORMAT_R16G16_SNORM]         = {GL_RG,         GL_RG16_SNORM, GL_SHORT,          0},
    [NGPU_FORMAT_R16G16_UINT]          = {GL_RG_INTEGER, GL_RG16UI,     GL_UNSIGNED_SHORT, S|C},
    [NGPU_FORMAT_R16G16_SINT]          = {GL_RG_INTEGER, GL_RG16I,      GL_SHORT,          S|C},
    [NGPU_FORMAT_R16G16_SFLOAT]        = {GL_RG,         GL_RG16F,      GL_HALF_FLOAT,     S|SL},

    [NGPU_FORMAT_R16G16B16_UNORM]      = {GL_RGB,         GL_RGB16,       GL_UNSIGNED_SHORT, 0},
    [NGPU_FORMAT_R16G16B16_SNORM]      = {GL_RGB,         GL_RGB16_SNORM, GL_SHORT,          0},
    [NGPU_FORMAT_R16G16B16_UINT]       = {GL_RGB_INTEGER, GL_RGB16UI,     GL_UNSIGNED_SHORT, S|C},
    [NGPU_FORMAT_R16G16B16_SINT]       = {GL_RGB_INTEGER, GL_RGB16I,      GL_SHORT,          S|C},
    [NGPU_FORMAT_R16G16B16_SFLOAT]     = {GL_RGB,         GL_RGB16F,      GL_HALF_FLOAT,     S|SL},

    [NGPU_FORMAT_R16G16B16A16_UNORM]   = {GL_RGBA,         GL_RGBA16,       GL_UNSIGNED_SHORT, 0},
    [NGPU_FORMAT_R16G16B16A16_SNORM]   = {GL_RGBA,         GL_RGBA16_SNORM, GL_SHORT,          0},
    [NGPU_FORMAT_R16G16B16A16_UINT]    = {GL_RGBA_INTEGER, GL_RGBA16UI,     GL_UNSIGNED_SHORT, S|C},
    [NGPU_FORMAT_R16G16B16A16_SINT]    = {GL_RGBA_INTEGER, GL_RGBA16I,      GL_SHORT,          S|C},
    [NGPU_FORMAT_R16G16B16A16_SFLOAT]  = {GL_RGBA,         GL_RGBA16F,      GL_HALF_FLOAT,     S|SL},

    [NGPU_FORMAT_R32_UINT]             = {GL_RED_INTEGER, GL_R32UI, GL_UNSIGNED_INT, S|C},
    [NGPU_FORMAT_R32_SINT]             = {GL_RED_INTEGER, GL_R32I,  GL_INT,          S|C},
    [NGPU_FORMAT_R32_SFLOAT]           = {GL_RED,         GL_R32F,  GL_FLOAT,        S},

    [NGPU_FORMAT_R32G32_UINT]          = {GL_RG_INTEGER, GL_RG32UI, GL_UNSIGNED_INT, S|C},
    [NGPU_FORMAT_R32G32_SINT]          = {GL_RG_INTEGER, GL_RG32I,  GL_INT,          S|C},
    [NGPU_FORMAT_R32G32_SFLOAT]        = {GL_RG,         GL_RG32F,  GL_FLOAT,        S},

    [NGPU_FORMAT_R32G32B32_UINT]       = {GL_RGB_INTEGER, GL_RGB32UI, GL_UNSIGNED_INT, S|C},
    [NGPU_FORMAT_R32G32B32_SINT]       = {GL_RGB_INTEGER, GL_RGB32I,  GL_INT,          S|C},
    [NGPU_FORMAT_R32G32B32_SFLOAT]     = {GL_RGB,         GL_RGB32F,  GL_FLOAT,        S},

    [NGPU_FORMAT_R32G32B32A32_UINT]    = {GL_RGBA_INTEGER,     GL_RGBA32UI, GL_UNSIGNED_INT, S|C},
    [NGPU_FORMAT_R32G32B32A32_SINT]    = {GL_RGBA_INTEGER,     GL_RGBA32I,  GL_INT,          S|C},
    [NGPU_FORMAT_R32G32B32A32_SFLOAT]  = {GL_RGBA,             GL_RGBA32F,  GL_FLOAT,        S},

    [NGPU_FORMAT_D16_UNORM]            = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16,  GL_UNSIGNED_SHORT,                 S|DS},
    [NGPU_FORMAT_X8_D24_UNORM_PACK32]  = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT24,  GL_UNSIGNED_INT,                   S|DS},
    [NGPU_FORMAT_D32_SFLOAT]           = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT32F, GL_FLOAT,                          S|DS},
    [NGPU_FORMAT_D24_UNORM_S8_UINT]    = {GL_DEPTH_STENCIL,   GL_DEPTH24_STENCIL8,   GL_UNSIGNED_INT_24_8,              S|DS},
    [NGPU_FORMAT_D32_SFLOAT_S8_UINT]   = {GL_DEPTH_STENCIL,   GL_DEPTH32F_STENCIL8,  GL_FLOAT_32_UNSIGNED_INT_24_8_REV, S|DS},
    [NGPU_FORMAT_S8_UINT]              = {GL_STENCIL_INDEX,   GL_STENCIL_INDEX8,     GL_UNSIGNED_BYTE,                  S|DS},
};

NGLI_STATIC_ASSERT(NGLI_FIELD_SIZEOF(struct glcontext, formats) == sizeof(formats), "formats size");

void ngpu_format_gl_init(struct glcontext *gl)
{
    memcpy(gl->formats, formats, sizeof(formats));

    if (gl->backend == NGL_BACKEND_OPENGL) {
        gl->formats[NGPU_FORMAT_B8G8R8A8_UNORM].features |= S|SL|C|B;
        gl->formats[NGPU_FORMAT_B8G8R8A8_SNORM].features |= S|SL|C|B;
        gl->formats[NGPU_FORMAT_B8G8R8A8_UINT].features  |= S|C;
        gl->formats[NGPU_FORMAT_B8G8R8A8_SINT].features  |= S|C;
    }

    if (gl->features & NGLI_FEATURE_GL_COLOR_BUFFER_HALF_FLOAT) {
        gl->formats[NGPU_FORMAT_R16_SFLOAT].features          |= C|B;
        gl->formats[NGPU_FORMAT_R16G16_SFLOAT].features       |= C|B;
        gl->formats[NGPU_FORMAT_R16G16B16A16_SFLOAT].features |= C|B;
    }

    if (gl->features & NGLI_FEATURE_GL_COLOR_BUFFER_FLOAT) {
        gl->formats[NGPU_FORMAT_R32_SFLOAT].features          |= C;
        gl->formats[NGPU_FORMAT_R32G32_SFLOAT].features       |= C;
        gl->formats[NGPU_FORMAT_R32G32B32A32_SFLOAT].features |= C;
    }

    if (gl->features & NGLI_FEATURE_GL_FLOAT_BLEND) {
        gl->formats[NGPU_FORMAT_R32_SFLOAT].features          |= B;
        gl->formats[NGPU_FORMAT_R32G32_SFLOAT].features       |= B;
        gl->formats[NGPU_FORMAT_R32G32B32A32_SFLOAT].features |= B;
    }

    if (gl->features & NGLI_FEATURE_GL_TEXTURE_FLOAT_LINEAR) {
        gl->formats[NGPU_FORMAT_R32_SFLOAT].features          |= SL;
        gl->formats[NGPU_FORMAT_R32G32_SFLOAT].features       |= SL;
        gl->formats[NGPU_FORMAT_R32G32B32A32_SFLOAT].features |= SL;
    }

    if (gl->features & NGLI_FEATURE_GL_TEXTURE_NORM16) {
        gl->formats[NGPU_FORMAT_R16_UNORM].features          |= S|SL|C|B;
        gl->formats[NGPU_FORMAT_R16_SNORM].features          |= S|SL;
        gl->formats[NGPU_FORMAT_R16G16_UNORM].features       |= S|SL|C|B;
        gl->formats[NGPU_FORMAT_R16G16_SNORM].features       |= S|SL;
        gl->formats[NGPU_FORMAT_R16G16B16A16_UNORM].features |= S|SL|C|B;
        gl->formats[NGPU_FORMAT_R16G16B16A16_SNORM].features |= S|SL;
    }
}

const struct ngpu_format_gl *ngpu_format_get_gl_texture_format(struct glcontext *gl, enum ngpu_format format)
{
    ngli_assert(format >= 0 && format < NGLI_ARRAY_NB(gl->formats));
    const struct ngpu_format_gl *format_gl = &gl->formats[format];

    if (format != NGPU_FORMAT_UNDEFINED)
        ngli_assert(format_gl->format && format_gl->internal_format && format_gl->type);

    return format_gl;
}
