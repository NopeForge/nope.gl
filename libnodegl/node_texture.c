/*
 * Copyright 2016 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include "format.h"
#include "glincludes.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "texture.h"

const struct param_choices ngli_mipmap_filter_choices = {
    .name = "mipmap_filter",
    .consts = {
        {"none",    NGLI_MIPMAP_FILTER_NONE,    .desc=NGLI_DOCSTRING("no mipmap generation")},
        {"nearest", NGLI_MIPMAP_FILTER_NEAREST, .desc=NGLI_DOCSTRING("nearest filtering")},
        {"linear",  NGLI_MIPMAP_FILTER_LINEAR,  .desc=NGLI_DOCSTRING("linear filtering")},
        {NULL}
    }
};

const struct param_choices ngli_filter_choices = {
    .name = "filter",
    .consts = {
        {"nearest", NGLI_FILTER_NEAREST, .desc=NGLI_DOCSTRING("nearest filtering")},
        {"linear",  NGLI_FILTER_LINEAR,  .desc=NGLI_DOCSTRING("linear filtering")},
        {NULL}
    }
};

static const struct param_choices wrap_choices = {
    .name = "wrap",
    .consts = {
        {"clamp_to_edge",   NGLI_WRAP_CLAMP_TO_EDGE,   .desc=NGLI_DOCSTRING("clamp to edge wrapping")},
        {"mirrored_repeat", NGLI_WRAP_MIRRORED_REPEAT, .desc=NGLI_DOCSTRING("mirrored repeat wrapping")},
        {"repeat",          NGLI_WRAP_REPEAT,          .desc=NGLI_DOCSTRING("repeat pattern wrapping")},
        {NULL}
    }
};

static const struct param_choices access_choices = {
    .name = "access",
    .consts = {
        {"read",  NGLI_ACCESS_READ_BIT,   .desc=NGLI_DOCSTRING("read")},
        {"write", NGLI_ACCESS_WRITE_BIT,  .desc=NGLI_DOCSTRING("write")},
        {NULL}
    }
};

static const struct param_choices format_choices = {
    .name = "format",
    .consts = {
        {"undefined",            NGLI_FORMAT_UNDEFINED,           .desc=NGLI_DOCSTRING("undefined")},
        {"r8_unorm",             NGLI_FORMAT_R8_UNORM,            .desc=NGLI_DOCSTRING("8-bit unsigned normalized R component")},
        {"r8_snorm",             NGLI_FORMAT_R8_SNORM,            .desc=NGLI_DOCSTRING("8-bit signed normalized R component")},
        {"r8_uint",              NGLI_FORMAT_R8_UINT,             .desc=NGLI_DOCSTRING("8-bit unsigned integer R component")},
        {"r8_sint",              NGLI_FORMAT_R8_SINT,             .desc=NGLI_DOCSTRING("8-bit signed integer R component")},
        {"r8g8_unorm",           NGLI_FORMAT_R8G8_UNORM,          .desc=NGLI_DOCSTRING("8-bit unsigned normalized RG components")},
        {"r8g8_snorm",           NGLI_FORMAT_R8G8_SNORM,          .desc=NGLI_DOCSTRING("8-bit signed normalized RG components")},
        {"r8g8_uint",            NGLI_FORMAT_R8G8_UINT,           .desc=NGLI_DOCSTRING("8-bit unsigned integer RG components")},
        {"r8g8_sint",            NGLI_FORMAT_R8G8_SINT,           .desc=NGLI_DOCSTRING("8-bit signed normalized RG components")},
        {"r8g8b8_unorm",         NGLI_FORMAT_R8G8B8_UNORM,        .desc=NGLI_DOCSTRING("8-bit unsigned normalized RGB components")},
        {"r8g8b8_snorm",         NGLI_FORMAT_R8G8B8_SNORM,        .desc=NGLI_DOCSTRING("8-bit signed normalized RGB components")},
        {"r8g8b8_uint",          NGLI_FORMAT_R8G8B8_UINT,         .desc=NGLI_DOCSTRING("8-bit unsigned integer RGB components")},
        {"r8g8b8_sint",          NGLI_FORMAT_R8G8B8_SINT,         .desc=NGLI_DOCSTRING("8-bit signed integer RGB components")},
        {"r8g8b8_srgb",          NGLI_FORMAT_R8G8B8_SRGB,         .desc=NGLI_DOCSTRING("8-bit unsigned normalized sRGB components")},
        {"r8g8b8a8_unorm",       NGLI_FORMAT_R8G8B8A8_UNORM,      .desc=NGLI_DOCSTRING("8-bit unsigned normalized RGBA components")},
        {"r8g8b8a8_snorm",       NGLI_FORMAT_R8G8B8A8_SNORM,      .desc=NGLI_DOCSTRING("8-bit signed normalized RGBA components")},
        {"r8g8b8a8_uint",        NGLI_FORMAT_R8G8B8A8_UINT,       .desc=NGLI_DOCSTRING("8-bit unsigned integer RGBA components")},
        {"r8g8b8a8_sint",        NGLI_FORMAT_R8G8B8A8_SINT,       .desc=NGLI_DOCSTRING("8-bit signed integer RGBA components")},
        {"r8g8b8a8_srgb",        NGLI_FORMAT_R8G8B8A8_SRGB,       .desc=NGLI_DOCSTRING("8-bit unsigned normalized RGBA components")},
        {"b8g8r8a8_unorm",       NGLI_FORMAT_B8G8R8A8_UNORM,      .desc=NGLI_DOCSTRING("8-bit unsigned normalized BGRA components")},
        {"b8g8r8a8_snorm",       NGLI_FORMAT_B8G8R8A8_SNORM,      .desc=NGLI_DOCSTRING("8-bit signed normalized BGRA components")},
        {"b8g8r8a8_uint",        NGLI_FORMAT_B8G8R8A8_UINT,       .desc=NGLI_DOCSTRING("8-bit unsigned integer BGRA components")},
        {"b8g8r8a8_sint",        NGLI_FORMAT_B8G8R8A8_SINT,       .desc=NGLI_DOCSTRING("8-bit signed integer BGRA components")},
        {"r16_unorm",            NGLI_FORMAT_R16_UNORM,           .desc=NGLI_DOCSTRING("16-bit unsigned normalized R component")},
        {"r16_snorm",            NGLI_FORMAT_R16_SNORM,           .desc=NGLI_DOCSTRING("16-bit signed normalized R component")},
        {"r16_uint",             NGLI_FORMAT_R16_UINT,            .desc=NGLI_DOCSTRING("16-bit unsigned integer R component")},
        {"r16_sint",             NGLI_FORMAT_R16_SINT,            .desc=NGLI_DOCSTRING("16-bit signed integer R component")},
        {"r16_sfloat",           NGLI_FORMAT_R16_SFLOAT,          .desc=NGLI_DOCSTRING("16-bit signed float R component")},
        {"r16g16_unorm",         NGLI_FORMAT_R16G16_UNORM,        .desc=NGLI_DOCSTRING("16-bit unsigned normalized RG components")},
        {"r16g16_snorm",         NGLI_FORMAT_R16G16_SNORM,        .desc=NGLI_DOCSTRING("16-bit signed normalized RG components")},
        {"r16g16_uint",          NGLI_FORMAT_R16G16_UINT,         .desc=NGLI_DOCSTRING("16-bit unsigned integer RG components")},
        {"r16g16_sint",          NGLI_FORMAT_R16G16_SINT,         .desc=NGLI_DOCSTRING("16-bit signed integer RG components")},
        {"r16g16_sfloat",        NGLI_FORMAT_R16G16_SFLOAT,       .desc=NGLI_DOCSTRING("16-bit signed float RG components")},
        {"r16g16b16_unorm",      NGLI_FORMAT_R16G16B16_UNORM,     .desc=NGLI_DOCSTRING("16-bit unsigned normalized RGB components")},
        {"r16g16b16_snorm",      NGLI_FORMAT_R16G16B16_SNORM,     .desc=NGLI_DOCSTRING("16-bit signed normalized RGB components")},
        {"r16g16b16_uint",       NGLI_FORMAT_R16G16B16_UINT,      .desc=NGLI_DOCSTRING("16-bit unsigned integer RGB components")},
        {"r16g16b16_sint",       NGLI_FORMAT_R16G16B16_SINT,      .desc=NGLI_DOCSTRING("16-bit signed integer RGB components")},
        {"r16g16b16_sfloat",     NGLI_FORMAT_R16G16B16_SFLOAT,    .desc=NGLI_DOCSTRING("16-bit signed float RGB components")},
        {"r16g16b16a16_unorm",   NGLI_FORMAT_R16G16B16A16_UNORM,  .desc=NGLI_DOCSTRING("16-bit unsigned normalized RGBA components")},
        {"r16g16b16a16_snorm",   NGLI_FORMAT_R16G16B16A16_SNORM,  .desc=NGLI_DOCSTRING("16-bit signed normalized RGBA components")},
        {"r16g16b16a16_uint",    NGLI_FORMAT_R16G16B16A16_UINT,   .desc=NGLI_DOCSTRING("16-bit unsigned integer RGBA components")},
        {"r16g16b16a16_sint",    NGLI_FORMAT_R16G16B16A16_SINT,   .desc=NGLI_DOCSTRING("16-bit signed integer RGBA components")},
        {"r16g16b16a16_sfloat",  NGLI_FORMAT_R16G16B16A16_SFLOAT, .desc=NGLI_DOCSTRING("16-bit signed float RGBA components")},
        {"r32_uint",             NGLI_FORMAT_R32_UINT,            .desc=NGLI_DOCSTRING("32-bit unsigned integer R component")},
        {"r32_sint",             NGLI_FORMAT_R32_SINT,            .desc=NGLI_DOCSTRING("32-bit signed integer R component")},
        {"r32_sfloat",           NGLI_FORMAT_R32_SFLOAT,          .desc=NGLI_DOCSTRING("32-bit signed float R component")},
        {"r32g32_uint",          NGLI_FORMAT_R32G32_UINT,         .desc=NGLI_DOCSTRING("32-bit unsigned integer RG components")},
        {"r32g32_sint",          NGLI_FORMAT_R32G32_SINT,         .desc=NGLI_DOCSTRING("32-bit signed integer RG components")},
        {"r32g32_sfloat",        NGLI_FORMAT_R32G32_SFLOAT,       .desc=NGLI_DOCSTRING("32-bit signed float RG components")},
        {"r32g32b32_uint",       NGLI_FORMAT_R32G32B32_UINT,      .desc=NGLI_DOCSTRING("32-bit unsigned integer RGB components")},
        {"r32g32b32_sint",       NGLI_FORMAT_R32G32B32_SINT,      .desc=NGLI_DOCSTRING("32-bit signed integer RGB components")},
        {"r32g32b32_sfloat",     NGLI_FORMAT_R32G32B32_SFLOAT,    .desc=NGLI_DOCSTRING("32-bit signed float RGB components")},
        {"r32g32b32a32_uint",    NGLI_FORMAT_R32G32B32A32_UINT,   .desc=NGLI_DOCSTRING("32-bit unsigned integer RGBA components")},
        {"r32g32b32a32_sint",    NGLI_FORMAT_R32G32B32A32_SINT,   .desc=NGLI_DOCSTRING("32-bit signed integer RGBA components")},
        {"r32g32b32a32_sfloat",  NGLI_FORMAT_R32G32B32A32_SFLOAT, .desc=NGLI_DOCSTRING("32-bit signed float RGBA components")},
        {"d16_unorm",            NGLI_FORMAT_D16_UNORM,           .desc=NGLI_DOCSTRING("16-bit unsigned normalized depth component")},
        {"d24_unorm",            NGLI_FORMAT_X8_D24_UNORM_PACK32, .desc=NGLI_DOCSTRING("32-bit packed format that has 24-bit unsigned "
                                                                                       "normalized depth component + 8-bit of unused data")},
        {"d32_sfloat",           NGLI_FORMAT_D32_SFLOAT,          .desc=NGLI_DOCSTRING("32-bit signed float depth component")},
        {"d24_unorm_s8_uint",    NGLI_FORMAT_D24_UNORM_S8_UINT,   .desc=NGLI_DOCSTRING("32-bit packed format that has 24-bit unsigned "
                                                                                       "normalized depth component + 8-bit unsigned "
                                                                                       "integer stencil component")},
        {"d32_sfloat_s8_uint",   NGLI_FORMAT_D32_SFLOAT_S8_UINT,  .desc=NGLI_DOCSTRING("64-bit packed format that has 32-bit signed "
                                                                                       "float depth component + 8-bit unsigned integer "
                                                                                       "stencil component + 24-bit of unused data")},
        {"s8_uint",              NGLI_FORMAT_S8_UINT,             .desc=NGLI_DOCSTRING("8-bit unsigned integer stencil component")},
        {NULL}
    }
};

#define BUFFER_NODES                \
    NGL_NODE_ANIMATEDBUFFERFLOAT,   \
    NGL_NODE_ANIMATEDBUFFERVEC2,    \
    NGL_NODE_ANIMATEDBUFFERVEC3,    \
    NGL_NODE_ANIMATEDBUFFERVEC4,    \
    NGL_NODE_BUFFERBYTE,            \
    NGL_NODE_BUFFERBVEC2,           \
    NGL_NODE_BUFFERBVEC3,           \
    NGL_NODE_BUFFERBVEC4,           \
    NGL_NODE_BUFFERINT,             \
    NGL_NODE_BUFFERIVEC2,           \
    NGL_NODE_BUFFERIVEC3,           \
    NGL_NODE_BUFFERIVEC4,           \
    NGL_NODE_BUFFERSHORT,           \
    NGL_NODE_BUFFERSVEC2,           \
    NGL_NODE_BUFFERSVEC3,           \
    NGL_NODE_BUFFERSVEC4,           \
    NGL_NODE_BUFFERUBYTE,           \
    NGL_NODE_BUFFERUBVEC2,          \
    NGL_NODE_BUFFERUBVEC3,          \
    NGL_NODE_BUFFERUBVEC4,          \
    NGL_NODE_BUFFERUINT,            \
    NGL_NODE_BUFFERUIVEC2,          \
    NGL_NODE_BUFFERUIVEC3,          \
    NGL_NODE_BUFFERUIVEC4,          \
    NGL_NODE_BUFFERUSHORT,          \
    NGL_NODE_BUFFERUSVEC2,          \
    NGL_NODE_BUFFERUSVEC3,          \
    NGL_NODE_BUFFERUSVEC4,          \
    NGL_NODE_BUFFERFLOAT,           \
    NGL_NODE_BUFFERVEC2,            \
    NGL_NODE_BUFFERVEC3,            \
    NGL_NODE_BUFFERVEC4,            \


#define DATA_SRC_TYPES_LIST_2D (const int[]){NGL_NODE_MEDIA,                   \
                                             BUFFER_NODES                      \
                                             -1}

#define DATA_SRC_TYPES_LIST_3D (const int[]){BUFFER_NODES                      \
                                             -1}


#define OFFSET(x) offsetof(struct texture_priv, x)
static const struct node_param texture2d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(params.format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(params.height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i64=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"access", PARAM_TYPE_FLAGS, OFFSET(params.access), {.i64=NGLI_ACCESS_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_2D,
                 .desc=NGLI_DOCSTRING("data source")},
    {"direct_rendering", PARAM_TYPE_BOOL, OFFSET(direct_rendering), {.i64=1},
                         .desc=NGLI_DOCSTRING("whether direct rendering is allowed or not for media playback")},
    {NULL}
};

static const struct node_param texture3d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(params.format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(params.height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"depth", PARAM_TYPE_INT, OFFSET(params.depth), {.i64=0},
               .desc=NGLI_DOCSTRING("depth of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i64=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"access", PARAM_TYPE_FLAGS, OFFSET(params.access), {.i64=NGLI_ACCESS_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static const struct node_param texturecube_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(params.format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"size", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
             .desc=NGLI_DOCSTRING("width and height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i64=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"access", PARAM_TYPE_FLAGS, OFFSET(params.access), {.i64=NGLI_ACCESS_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static int texture_prefetch(struct ngl_node *node, int dimensions, int cubemap)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct texture_params *params = &s->params;

    params->dimensions = dimensions;
    if (cubemap) {
        params->height = params->width;
        params->depth = 6;
        params->cubemap = 1;
    }

    if (gl->features & NGLI_FEATURE_TEXTURE_STORAGE)
        params->immutable = 1;

    const uint8_t *data = NULL;

    if (s->data_src) {
        switch (s->data_src->class->id) {
        case NGL_NODE_MEDIA:
            return 0;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
        case NGL_NODE_BUFFERBYTE:
        case NGL_NODE_BUFFERBVEC2:
        case NGL_NODE_BUFFERBVEC3:
        case NGL_NODE_BUFFERBVEC4:
        case NGL_NODE_BUFFERINT:
        case NGL_NODE_BUFFERIVEC2:
        case NGL_NODE_BUFFERIVEC3:
        case NGL_NODE_BUFFERIVEC4:
        case NGL_NODE_BUFFERSHORT:
        case NGL_NODE_BUFFERSVEC2:
        case NGL_NODE_BUFFERSVEC3:
        case NGL_NODE_BUFFERSVEC4:
        case NGL_NODE_BUFFERUBYTE:
        case NGL_NODE_BUFFERUBVEC2:
        case NGL_NODE_BUFFERUBVEC3:
        case NGL_NODE_BUFFERUBVEC4:
        case NGL_NODE_BUFFERUINT:
        case NGL_NODE_BUFFERUIVEC2:
        case NGL_NODE_BUFFERUIVEC3:
        case NGL_NODE_BUFFERUIVEC4:
        case NGL_NODE_BUFFERUSHORT:
        case NGL_NODE_BUFFERUSVEC2:
        case NGL_NODE_BUFFERUSVEC3:
        case NGL_NODE_BUFFERUSVEC4:
        case NGL_NODE_BUFFERFLOAT:
        case NGL_NODE_BUFFERVEC2:
        case NGL_NODE_BUFFERVEC3:
        case NGL_NODE_BUFFERVEC4: {
            struct buffer_priv *buffer = s->data_src->priv_data;

            if (params->dimensions == 2) {
                if (buffer->count != params->width * params->height) {
                    LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%d),"
                        " assuming %dx1", params->width, params->height,
                        buffer->count, buffer->count);
                    params->width = buffer->count;
                    params->height = 1;
                }
            } else if (params->dimensions == 3) {
                if (buffer->count != params->width * params->height * params->depth) {
                    LOG(ERROR, "dimensions (%dx%dx%d) do not match buffer count (%d),"
                        " assuming %dx1x1", params->width, params->height, params->depth,
                        buffer->count, buffer->count);
                    params->width = buffer->count;
                    params->height = params->depth = 1;
                }
            }
            data = buffer->data;
            params->format = buffer->data_format;
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    int ret = ngli_texture_init(&s->texture, ctx, params);
    if (ret < 0)
        return ret;

    ret = ngli_texture_upload(&s->texture, data, 0);
    if (ret < 0)
        return ret;

    struct image_params image_params = {
        .width = params->width,
        .height = params->height,
        .depth = params->depth,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
    };
    struct texture *planes[] = {&s->texture};
    ngli_image_init(&s->image, &image_params, planes);

    return 0;
}

#define TEXTURE_PREFETCH(name, dim, cubemap)                \
static int texture##name##_prefetch(struct ngl_node *node)  \
{                                                           \
    return texture_prefetch(node, dim, cubemap);            \
}

TEXTURE_PREFETCH(2d,   2, 0)
TEXTURE_PREFETCH(3d,   3, 0)
TEXTURE_PREFETCH(cube, 3, 1)

static void handle_media_frame(struct ngl_node *node)
{
    int ret = ngli_hwupload_upload_frame(node);
    if (ret < 0)
        LOG(ERROR, "could not map media frame");
}

static void handle_buffer_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct buffer_priv *buffer = s->data_src->priv_data;
    const uint8_t *data = buffer->data;
    struct texture *t = &s->texture;

    ngli_texture_upload(t, data, 0);
}

static int texture_update(struct ngl_node *node, double t)
{
    struct texture_priv *s = node->priv_data;

    if (!s->data_src)
        return 0;

    int ret = ngli_node_update(s->data_src, t);
    if (ret < 0)
        return ret;

    switch (s->data_src->class->id) {
        case NGL_NODE_MEDIA:
            handle_media_frame(node);
            break;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
            ret = ngli_node_update(s->data_src, t);
            if (ret < 0)
                return ret;
            handle_buffer_frame(node);
            break;
    }

    return 0;
}

static void texture_release(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;

    ngli_hwupload_uninit(node);
    ngli_texture_reset(&s->texture);
    ngli_image_reset(&s->image);
}

static int texture2d_init(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    s->supported_image_layouts = s->direct_rendering ? -1 : (1 << NGLI_IMAGE_LAYOUT_DEFAULT);
    return 0;
}

static int texture3d_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    if (!(gl->features & NGLI_FEATURE_TEXTURE_3D)) {
        LOG(ERROR, "context does not support 3D textures");
        return NGL_ERROR_UNSUPPORTED;
    }
    return 0;
}

static int texturecube_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    if (!(gl->features & NGLI_FEATURE_TEXTURE_CUBE_MAP)) {
        LOG(ERROR, "context does not support cube map textures");
        return NGL_ERROR_UNSUPPORTED;
    }
    return 0;
}

const struct node_class ngli_texture2d_class = {
    .id        = NGL_NODE_TEXTURE2D,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture2D",
    .init      = texture2d_init,
    .prefetch  = texture2d_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texture2d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texture3d_class = {
    .id        = NGL_NODE_TEXTURE3D,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture3D",
    .init      = texture3d_init,
    .prefetch  = texture3d_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texture3d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texturecube_class = {
    .id        = NGL_NODE_TEXTURECUBE,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "TextureCube",
    .init      = texturecube_init,
    .prefetch  = texturecube_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texturecube_params,
    .file      = __FILE__,
};
