/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail,com>
 * Copyright 2016-2022 GoPro Inc.
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

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <nopemd.h>

#include "format.h"
#include "gpu_ctx.h"
#include "hwmap.h"
#include "image.h"
#include "log.h"
#include "nopegl.h"
#include "internal.h"
#include "rtt.h"
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

/* These formats are not in format.h because they do not represent a native GPU format */
#define NGLI_FORMAT_AUTO_DEPTH         (NGLI_FORMAT_NB + 1)
#define NGLI_FORMAT_AUTO_DEPTH_STENCIL (NGLI_FORMAT_NB + 2)

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
        {"auto_depth",           NGLI_FORMAT_AUTO_DEPTH,          .desc=NGLI_DOCSTRING("select automatically the preferred depth format")},
        {"auto_depth_stencil",   NGLI_FORMAT_AUTO_DEPTH_STENCIL,  .desc=NGLI_DOCSTRING("select automatically the preferred depth + stencil format")},
        {NULL}
    }
};

#define BUFFER_NODES                \
    NGL_NODE_ANIMATEDBUFFERFLOAT,   \
    NGL_NODE_ANIMATEDBUFFERVEC2,    \
    NGL_NODE_ANIMATEDBUFFERVEC4,    \
    NGL_NODE_BUFFERBYTE,            \
    NGL_NODE_BUFFERBVEC2,           \
    NGL_NODE_BUFFERBVEC4,           \
    NGL_NODE_BUFFERINT,             \
    NGL_NODE_BUFFERIVEC2,           \
    NGL_NODE_BUFFERIVEC4,           \
    NGL_NODE_BUFFERSHORT,           \
    NGL_NODE_BUFFERSVEC2,           \
    NGL_NODE_BUFFERSVEC4,           \
    NGL_NODE_BUFFERUBYTE,           \
    NGL_NODE_BUFFERUBVEC2,          \
    NGL_NODE_BUFFERUBVEC4,          \
    NGL_NODE_BUFFERUINT,            \
    NGL_NODE_BUFFERUIVEC2,          \
    NGL_NODE_BUFFERUIVEC4,          \
    NGL_NODE_BUFFERUSHORT,          \
    NGL_NODE_BUFFERUSVEC2,          \
    NGL_NODE_BUFFERUSVEC4,          \
    NGL_NODE_BUFFERFLOAT,           \
    NGL_NODE_BUFFERVEC2,            \
    NGL_NODE_BUFFERVEC4,            \


#define DATA_SRC_TYPES_LIST_2D (const uint32_t[]){NGL_NODE_MEDIA,                   \
                                                  BUFFER_NODES                      \
                                                  NGLI_NODE_NONE}

#define DATA_SRC_TYPES_LIST_3D (const uint32_t[]){BUFFER_NODES                      \
                                                  NGLI_NODE_NONE}


#define OFFSET(x) offsetof(struct texture_opts, x)
static const struct node_param texture2d_params[] = {
    {"format", NGLI_PARAM_TYPE_SELECT, OFFSET(requested_format), {.i32=NGLI_FORMAT_R8G8B8A8_UNORM},
               .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", NGLI_PARAM_TYPE_I32, OFFSET(params.width), {.i32=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", NGLI_PARAM_TYPE_I32, OFFSET(params.height), {.i32=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"min_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i32=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"data_src", NGLI_PARAM_TYPE_NODE, OFFSET(data_src),
                 .desc=NGLI_DOCSTRING("data source")},
    {"direct_rendering", NGLI_PARAM_TYPE_BOOL, OFFSET(direct_rendering), {.i32=1},
                         .desc=NGLI_DOCSTRING("whether direct rendering is allowed or not for media playback")},
    {"clamp_video", NGLI_PARAM_TYPE_BOOL, OFFSET(clamp_video), {.i32=0},
                    .desc=NGLI_DOCSTRING("clamp ngl_texvideo() output to [0;1]")},
    {"clear_color", NGLI_PARAM_TYPE_VEC4, OFFSET(clear_color),
                    .desc=NGLI_DOCSTRING("color used to clear the texture when used as an implicit render target")},
    {"forward_transforms", NGLI_PARAM_TYPE_BOOL, OFFSET(forward_transforms), {.i32=0},
                           .desc=NGLI_DOCSTRING("enable forwarding of camera/model transformations when used as an implicit render target")},
    {NULL}
};

static const struct node_param texture2d_array_params[] = {
    {"format", NGLI_PARAM_TYPE_SELECT, OFFSET(requested_format), {.i32=NGLI_FORMAT_R8G8B8A8_UNORM},
               .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", NGLI_PARAM_TYPE_I32, OFFSET(params.width), {.i32=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", NGLI_PARAM_TYPE_I32, OFFSET(params.height), {.i32=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"depth", NGLI_PARAM_TYPE_I32, OFFSET(params.depth), {.i32=0},
               .desc=NGLI_DOCSTRING("depth of the texture")},
    {"min_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i32=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"data_src", NGLI_PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static const struct node_param texture3d_params[] = {
    {"format", NGLI_PARAM_TYPE_SELECT, OFFSET(requested_format), {.i32=NGLI_FORMAT_R8G8B8A8_UNORM},
               .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", NGLI_PARAM_TYPE_I32, OFFSET(params.width), {.i32=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", NGLI_PARAM_TYPE_I32, OFFSET(params.height), {.i32=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"depth", NGLI_PARAM_TYPE_I32, OFFSET(params.depth), {.i32=0},
               .desc=NGLI_DOCSTRING("depth of the texture")},
    {"min_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i32=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"data_src", NGLI_PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static const struct node_param texturecube_params[] = {
    {"format", NGLI_PARAM_TYPE_SELECT, OFFSET(requested_format), {.i32=NGLI_FORMAT_R8G8B8A8_UNORM},
               .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"size", NGLI_PARAM_TYPE_I32, OFFSET(params.width), {.i32=0},
             .desc=NGLI_DOCSTRING("width and height of the texture")},
    {"min_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i32=NGLI_FILTER_LINEAR}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", NGLI_PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i32=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", NGLI_PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i32=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"data_src", NGLI_PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static int texture_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;
    struct texture_params *params = &s->params;

    params->usage |= NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_SAMPLED_BIT;

    if (params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE)
        params->usage |= NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT;

    const uint8_t *data = NULL;

    if (o->data_src) {
        if (o->data_src->cls->id == NGL_NODE_MEDIA) {
            struct ngl_node *media = o->data_src;
            ngli_unused struct media_priv *media_priv = media->priv_data;
            const struct hwmap_params hwmap_params = {
                .label                 = node->label,
                .image_layouts         = s->supported_image_layouts,
                .texture_min_filter    = params->min_filter,
                .texture_mag_filter    = params->mag_filter,
                .texture_mipmap_filter = params->mipmap_filter,
                .texture_wrap_s        = params->wrap_s,
                .texture_wrap_t        = params->wrap_t,
                .texture_usage         = params->usage,
#if defined(TARGET_ANDROID)
                .android_surface       = media_priv->android_surface,
                .android_imagereader   = media_priv->android_imagereader,
#endif
            };
            return ngli_hwmap_init(&s->hwmap, ctx, &hwmap_params);
        } else if (o->data_src->cls->category == NGLI_NODE_CATEGORY_BUFFER) {
            struct buffer_info *buffer = o->data_src->priv_data;
            if (buffer->block) {
                LOG(ERROR, "buffers used as a texture data source referencing a block are not supported");
                return NGL_ERROR_UNSUPPORTED;
            }

            if (buffer->layout.type == NGLI_TYPE_VEC3) {
                LOG(ERROR, "3-components texture formats are not supported");
                return NGL_ERROR_UNSUPPORTED;
            }

            if (params->type == NGLI_TEXTURE_TYPE_2D) {
                if (buffer->layout.count != params->width * params->height) {
                    LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%zu),"
                        " assuming %zux1", params->width, params->height,
                        buffer->layout.count, buffer->layout.count);
                    if (buffer->layout.count > INT_MAX)
                        return NGL_ERROR_LIMIT_EXCEEDED;
                    params->width = (int)buffer->layout.count;
                    params->height = 1;
                }
            } else if (params->type == NGLI_TEXTURE_TYPE_3D) {
                if (buffer->layout.count != params->width * params->height * params->depth) {
                    LOG(ERROR, "dimensions (%dx%dx%d) do not match buffer count (%zu),"
                        " assuming %zux1x1", params->width, params->height, params->depth,
                        buffer->layout.count, buffer->layout.count);
                    if (buffer->layout.count > INT_MAX)
                        return NGL_ERROR_LIMIT_EXCEEDED;
                    params->width = (int)buffer->layout.count;
                    params->height = params->depth = 1;
                }
            }
            data = buffer->data;
            params->format = buffer->layout.format;
        }
    }

    if (s->params.width > 0 && s->params.height > 0) {
        s->texture = ngli_texture_create(gpu_ctx);
        if (!s->texture)
            return NGL_ERROR_MEMORY;

        int ret = ngli_texture_init(s->texture, params);
        if (ret < 0)
            return ret;

        ret = ngli_texture_upload(s->texture, data, 0);
        if (ret < 0)
            return ret;
    }

    const struct image_params image_params = {
        .width = params->width,
        .height = params->height,
        .depth = params->depth,
        .color_scale = 1.f,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
    };
    ngli_image_init(&s->image, &image_params, &s->texture);
    s->image.rev = s->image_rev++;

    if (s->rtt) {
        /* Transform the color textures coordinates so it matches how the
         * graphics context uv coordinate system works regarding render targets */
        ngli_gpu_ctx_get_rendertarget_uvcoord_matrix(gpu_ctx, s->image.coordinates_matrix);

        int depth_format = NGLI_FORMAT_UNDEFINED;
        if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_STENCIL)
            depth_format = ngli_gpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
        else if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_DEPTH)
            depth_format = ngli_gpu_ctx_get_preferred_depth_format(gpu_ctx);

        s->rtt_params = (struct rtt_params) {
            .width = params->width,
            .height = params->height,
            .nb_interruptions = s->renderpass_info.nb_interruptions,
            .nb_colors = 1,
            .colors[0] = {
                .attachment = s->texture,
                .load_op = NGLI_LOAD_OP_CLEAR,
                .store_op = NGLI_STORE_OP_STORE,
                .clear_value = {NGLI_ARG_VEC4(o->clear_color)},
            },
            .depth_stencil_format = depth_format,
        };

        if (s->params.width > 0 && s->params.height > 0) {
            s->rtt_ctx = ngli_rtt_create(node->ctx);
            if (!s->rtt_ctx)
                return NGL_ERROR_MEMORY;

            int ret = ngli_rtt_init(s->rtt_ctx, &s->rtt_params);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int handle_media_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;
    struct media_priv *media = o->data_src->priv_data;
    struct nmd_frame *frame = media->frame;
    if (!frame)
        return 0;

    /* Transfer frame ownership to hwmap and ensure it cannot be re-used
     * later on */
    media->frame = NULL;

    /* Reset destination image */
    ngli_image_reset(&s->image);

    int ret = ngli_hwmap_map_frame(&s->hwmap, frame, &s->image);

    /* Signal image change on new frame */
    s->image.rev = s->image_rev++;

    if (ret < 0) {
        LOG(ERROR, "could not map media frame");
        return ret;
    }

    return 0;
}

static int handle_buffer_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;
    struct buffer_info *buffer = o->data_src->priv_data;
    const uint8_t *data = buffer->data;

    int ret = ngli_texture_upload(s->texture, data, 0);
    if (ret < 0) {
        LOG(ERROR, "could not upload texture buffer");
        return ret;
    }

    return 0;
}

static int texture_update(struct ngl_node *node, double t)
{
    const struct texture_opts *o = node->opts;

    if (!o->data_src)
        return 0;

    int ret = ngli_node_update(o->data_src, t);
    if (ret < 0)
        return ret;

    switch (o->data_src->cls->id) {
        case NGL_NODE_MEDIA:
            /*
             * Tolerate media frames mapping/upload failures because they are
             * "likely" errors where we prefer to black-out part of the
             * presentation instead of hard-failing.
             */
            (void)handle_media_frame(node);
            break;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
            ret = handle_buffer_frame(node);
            if (ret < 0)
                return ret;
            break;
    }

    return 0;
}

static int rtt_resize(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct texture_priv *s = node->priv_data;

    const int32_t width = ctx->current_rendertarget->width;
    const int32_t height = ctx->current_rendertarget->height;
    if (s->params.width == width && s->params.height == height)
        return 0;

    struct texture *texture = NULL;
    struct rtt_ctx *rtt_ctx = NULL;

    texture = ngli_texture_create(ctx->gpu_ctx);
    if (!texture) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    struct texture_params texture_params = s->params;
    texture_params.width = width;
    texture_params.height = height;

    ret = ngli_texture_init(texture, &texture_params);
    if (ret < 0)
        goto fail;

    rtt_ctx = ngli_rtt_create(ctx);
    if (!rtt_ctx) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    struct rtt_params rtt_params = s->rtt_params;
    rtt_params.width  = width;
    rtt_params.height = height;
    rtt_params.colors[0].attachment = texture;

    ret = ngli_rtt_init(rtt_ctx, &rtt_params);
    if (ret < 0)
        goto fail;

    ngli_rtt_freep(&s->rtt_ctx);
    ngli_texture_freep(&s->texture);

    s->params = texture_params;
    s->texture = texture;
    s->image.params.width = width;
    s->image.params.height = height;
    s->image.planes[0] = texture;
    s->image.rev = s->image_rev++;
    s->rtt_params = rtt_params;
    s->rtt_ctx = rtt_ctx;

    return 0;

fail:
    ngli_texture_freep(&texture);
    ngli_rtt_freep(&rtt_ctx);

    LOG(ERROR, "failed to resize texture: %dx%d", width, height);
    return ret;
}

static void texture_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct texture_priv *s = node->priv_data;
    struct texture_opts *o = node->opts;

    if (!s->rtt)
        return;

    if (s->rtt_resizeable) {
        int ret = rtt_resize(node);
        if (ret < 0)
            return;
    }

    if (!o->forward_transforms) {
        if (!ngli_darray_push(&ctx->modelview_matrix_stack, ctx->default_modelview_matrix) ||
            !ngli_darray_push(&ctx->projection_matrix_stack, ctx->default_projection_matrix))
            return;
    }

    ngli_rtt_begin(s->rtt_ctx);
    ngli_node_draw(o->data_src);
    ngli_rtt_end(s->rtt_ctx);

    if (!o->forward_transforms) {
        ngli_darray_pop(&ctx->modelview_matrix_stack);
        ngli_darray_pop(&ctx->projection_matrix_stack);
    }
}

static void texture_release(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;

    ngli_rtt_freep(&s->rtt_ctx);
    ngli_hwmap_uninit(&s->hwmap);
    ngli_texture_freep(&s->texture);
    ngli_image_reset(&s->image);
    s->image.rev = s->image_rev++;
}

static int get_preferred_format(struct gpu_ctx *gpu_ctx, int format)
{
    switch (format) {
    case NGLI_FORMAT_AUTO_DEPTH:
        return ngli_gpu_ctx_get_preferred_depth_format(gpu_ctx);
    case NGLI_FORMAT_AUTO_DEPTH_STENCIL:
        return ngli_gpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
    default:
        return format;
    }
}

static int texture2d_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;

    s->params = o->params;

    const int max_dimension = gpu_ctx->limits.max_texture_dimension_2d;
    if (s->params.width  < 0 || s->params.width  > max_dimension ||
        s->params.height < 0 || s->params.height > max_dimension) {
        LOG(ERROR, "texture dimensions (%d,%d) are invalid or exceeds device limits (%d,%d)",
            s->params.width, s->params.height, max_dimension, max_dimension);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }
    s->params.type = NGLI_TEXTURE_TYPE_2D;
    s->params.format = get_preferred_format(gpu_ctx, o->requested_format);
    s->supported_image_layouts = o->direct_rendering ? -1 : (1 << NGLI_IMAGE_LAYOUT_DEFAULT);

    /*
     * On Android, the frame can only be uploaded once and each subsequent
     * upload will be a noop which results in an empty texture. This limitation
     * prevents us from sharing the Media node across multiple textures.
     */
    struct ngl_node *data_src = o->data_src;
    if (data_src && data_src->cls->id == NGL_NODE_MEDIA) {
        struct media_priv *media_priv = data_src->priv_data;
        if (media_priv->nb_parents++ > 0) {
            LOG(ERROR, "a media node (label=%s) can not be shared, "
                "the Texture should be shared instead", data_src->label);
            return NGL_ERROR_INVALID_USAGE;
        }
    } else if (data_src && data_src->cls->category != NGLI_NODE_CATEGORY_BUFFER) {
        s->rtt = 1;
        s->rtt_resizeable = (s->params.width == 0 && s->params.height == 0);

        ngli_node_get_renderpass_info(data_src, &s->renderpass_info);

        s->params.usage |= NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
        s->rendertarget_layout.colors[s->rendertarget_layout.nb_colors].format = s->params.format;
        s->rendertarget_layout.nb_colors++;

        int depth_format = NGLI_FORMAT_UNDEFINED;
        if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_STENCIL)
            depth_format = ngli_gpu_ctx_get_preferred_depth_stencil_format(gpu_ctx);
        else if (s->renderpass_info.features & NGLI_RENDERPASS_FEATURE_DEPTH)
            depth_format = ngli_gpu_ctx_get_preferred_depth_format(gpu_ctx);
        s->rendertarget_layout.depth_stencil.format = depth_format;
    }

    return 0;
}

static int texture2d_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct texture_priv *s = node->priv_data;

    if (!s->rtt)
        return 0;

    rnode->rendertarget_layout = s->rendertarget_layout;
    return ngli_node_prepare_children(node);

    return 0;
}

static int texture2d_array_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;

    s->params = o->params;

    const int max_dimension = gpu_ctx->limits.max_texture_dimension_2d;
    const int max_layers = gpu_ctx->limits.max_texture_array_layers;
    if (s->params.width  <= 0 || s->params.width  > max_dimension ||
        s->params.height <= 0 || s->params.height > max_dimension ||
        s->params.depth  <= 0 || s->params.depth  > max_layers) {
        LOG(ERROR, "texture dimensions (%d,%d,%d) are invalid or exceeds device limits (%d,%d,%d)",
            s->params.width, s->params.height, s->params.depth,
            max_dimension, max_dimension, max_layers);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }
    s->params.type = NGLI_TEXTURE_TYPE_2D_ARRAY;
    s->params.format = get_preferred_format(gpu_ctx, o->requested_format);

    return 0;
}

static int texture3d_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;

    s->params = o->params;

    const int max_dimension = gpu_ctx->limits.max_texture_dimension_3d;
    if (s->params.width  <= 0 || s->params.width  > max_dimension ||
        s->params.height <= 0 || s->params.height > max_dimension ||
        s->params.depth  <= 0 || s->params.depth  > max_dimension) {
        LOG(ERROR, "texture dimensions (%d,%d,%d) are invalid or exceeds device limits (%d,%d,%d)",
            s->params.width, s->params.height, s->params.depth,
            max_dimension, max_dimension, max_dimension);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }
    s->params.type = NGLI_TEXTURE_TYPE_3D;
    s->params.format = get_preferred_format(gpu_ctx, o->requested_format);

    return 0;
}

static int texturecube_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct texture_priv *s = node->priv_data;
    const struct texture_opts *o = node->opts;

    s->params = o->params;
    s->params.height = s->params.width;

    const int max_dimension = gpu_ctx->limits.max_texture_dimension_cube;
    if (s->params.width  <= 0 || s->params.width  > max_dimension ||
        s->params.height <= 0 || s->params.height > max_dimension) {
        LOG(ERROR, "texture dimensions (%d,%d) are invalid or exceeds device limits (%d,%d)",
            s->params.width, s->params.height, max_dimension, max_dimension);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }
    s->params.type = NGLI_TEXTURE_TYPE_CUBE;
    s->params.format = get_preferred_format(gpu_ctx, o->requested_format);

    return 0;
}

const struct node_class ngli_texture2d_class = {
    .id        = NGL_NODE_TEXTURE2D,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture2D",
    .init      = texture2d_init,
    .prepare   = texture2d_prepare,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .draw      = texture_draw,
    .release   = texture_release,
    .opts_size = sizeof(struct texture_opts),
    .priv_size = sizeof(struct texture_priv),
    .params    = texture2d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texture2darray_class = {
    .id        = NGL_NODE_TEXTURE2DARRAY,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture2DArray",
    .init      = texture2d_array_init,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .opts_size = sizeof(struct texture_opts),
    .priv_size = sizeof(struct texture_priv),
    .params    = texture2d_array_params,
    .file      = __FILE__,
};

const struct node_class ngli_texture3d_class = {
    .id        = NGL_NODE_TEXTURE3D,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture3D",
    .init      = texture3d_init,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .opts_size = sizeof(struct texture_opts),
    .priv_size = sizeof(struct texture_priv),
    .params    = texture3d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texturecube_class = {
    .id        = NGL_NODE_TEXTURECUBE,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "TextureCube",
    .init      = texturecube_init,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .opts_size = sizeof(struct texture_opts),
    .priv_size = sizeof(struct texture_priv),
    .params    = texturecube_params,
    .file      = __FILE__,
};
