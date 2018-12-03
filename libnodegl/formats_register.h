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

#ifndef FORMATS_REGISTER_H
#define FORMATS_REGISTER_H

#define NGLI_FORMATS(action)                                                                                                   \
    action(NGLI_FORMAT_UNDEFINED,             0, "undefined",            "undefined")                                          \
    action(NGLI_FORMAT_R8_UNORM,              1, "r8_unorm",             "8-bit unsigned normalized R component")              \
    action(NGLI_FORMAT_R8_SNORM,              1, "r8_snorm",             "8-bit signed normalized R component")                \
    action(NGLI_FORMAT_R8_UINT,               1, "r8_uint",              "8-bit unsigned integer R component")                 \
    action(NGLI_FORMAT_R8_SINT,               1, "r8_sint",              "8-bit signed integer R component")                   \
    action(NGLI_FORMAT_R8G8_UNORM,            2, "r8g8_unorm",           "8-bit unsigned normalized RG components")            \
    action(NGLI_FORMAT_R8G8_SNORM,            2, "r8g8_snorm",           "8-bit signed normalized RG components")              \
    action(NGLI_FORMAT_R8G8_UINT,             2, "r8g8_uint",            "8-bit unsigned integer RG components")               \
    action(NGLI_FORMAT_R8G8_SINT,             2, "r8g8_sint",            "8-bit signed normalized RG components")              \
    action(NGLI_FORMAT_R8G8B8_UNORM,          3, "r8g8b8_unorm",         "8-bit unsigned normalized RGB components")           \
    action(NGLI_FORMAT_R8G8B8_SNORM,          3, "r8g8b8_snorm",         "8-bit signed normalized RGB components")             \
    action(NGLI_FORMAT_R8G8B8_UINT,           3, "r8g8b8_uint",          "8-bit unsigned integer RGB components")              \
    action(NGLI_FORMAT_R8G8B8_SINT,           3, "r8g8b8_sint",          "8-bit signed integer RGB components")                \
    action(NGLI_FORMAT_R8G8B8_SRGB,           3, "r8g8b8_srgb",          "8-bit unsigned normalized sRGB components")          \
    action(NGLI_FORMAT_R8G8B8A8_UNORM,        4, "r8g8b8a8_unorm",       "8-bit unsigned normalized RGBA components")          \
    action(NGLI_FORMAT_R8G8B8A8_SNORM,        4, "r8g8b8a8_snorm",       "8-bit signed normalized RGBA components")            \
    action(NGLI_FORMAT_R8G8B8A8_UINT,         4, "r8g8b8a8_uint",        "8-bit unsigned integer RGBA components")             \
    action(NGLI_FORMAT_R8G8B8A8_SINT,         4, "r8g8b8a8_sint",        "8-bit signed integer RGBA components")               \
    action(NGLI_FORMAT_R8G8B8A8_SRGB,         4, "r8g8b8a8_srgb",        "8-bit unsigned normalized RGBA components")          \
    action(NGLI_FORMAT_B8G8R8A8_UNORM,        4, "b8g8r8a8_unorm",       "8-bit unsigned normalized BGRA components")          \
    action(NGLI_FORMAT_B8G8R8A8_SNORM,        4, "b8g8r8a8_snorm",       "8-bit signed normalized BGRA components")            \
    action(NGLI_FORMAT_B8G8R8A8_UINT,         4, "b8g8r8a8_uint",        "8-bit unsigned integer BGRA components")             \
    action(NGLI_FORMAT_B8G8R8A8_SINT,         4, "b8g8r8a8_sint",        "8-bit signed integer BGRA components")               \
    action(NGLI_FORMAT_R16_UNORM,             2, "r16_unorm",            "16-bit unsigned normalized R component")             \
    action(NGLI_FORMAT_R16_SNORM,             2, "r16_snorm",            "16-bit signed normalized R component")               \
    action(NGLI_FORMAT_R16_UINT,              2, "r16_uint",             "16-bit unsigned integer R component")                \
    action(NGLI_FORMAT_R16_SINT,              2, "r16_sint",             "16-bit signed integer R component")                  \
    action(NGLI_FORMAT_R16_SFLOAT,            2, "r16_sfloat",           "16-bit signed float R component")                    \
    action(NGLI_FORMAT_R16G16_UNORM,          4, "r16g16_unorm",         "16-bit unsigned normalized RG components")           \
    action(NGLI_FORMAT_R16G16_SNORM,          4, "r16g16_snorm",         "16-bit signed normalized RG components")             \
    action(NGLI_FORMAT_R16G16_UINT,           4, "r16g16_uint",          "16-bit unsigned integer RG components")              \
    action(NGLI_FORMAT_R16G16_SINT,           4, "r16g16_sint",          "16-bit signed integer RG components")                \
    action(NGLI_FORMAT_R16G16_SFLOAT,         4, "r16g16_sfloat",        "16-bit signed float RG components")                  \
    action(NGLI_FORMAT_R16G16B16_UNORM,       6, "r16g16b16_unorm",      "16-bit unsigned normalized RGB components")          \
    action(NGLI_FORMAT_R16G16B16_SNORM,       6, "r16g16b16_snorm",      "16-bit signed normalized RGB components")            \
    action(NGLI_FORMAT_R16G16B16_UINT,        6, "r16g16b16_uint",       "16-bit unsigned integer RGB components")             \
    action(NGLI_FORMAT_R16G16B16_SINT,        6, "r16g16b16_sint",       "16-bit signed integer RGB components")               \
    action(NGLI_FORMAT_R16G16B16_SFLOAT,      6, "r16g16b16_sfloat",     "16-bit signed float RGB components")                 \
    action(NGLI_FORMAT_R16G16B16A16_UNORM,    8, "r16g16b16a16_unorm",   "16-bit unsigned normalized RGBA components")         \
    action(NGLI_FORMAT_R16G16B16A16_SNORM,    8, "r16g16b16a16_snorm",   "16-bit signed normalized RGBA components")           \
    action(NGLI_FORMAT_R16G16B16A16_UINT,     8, "r16g16b16a16_uint",    "16-bit unsigned integer RGBA components")            \
    action(NGLI_FORMAT_R16G16B16A16_SINT,     8, "r16g16b16a16_sint",    "16-bit signed integer RGBA components")              \
    action(NGLI_FORMAT_R16G16B16A16_SFLOAT,   8, "r16g16b16a16_sfloat",  "16-bit signed float RGBA components")                \
    action(NGLI_FORMAT_R32_UINT,              4, "r32_uint",             "32-bit unsigned integer R component")                \
    action(NGLI_FORMAT_R32_SINT,              4, "r32_sint",             "32-bit signed integer R component")                  \
    action(NGLI_FORMAT_R32_SFLOAT,            4, "r32_sfloat",           "32-bit signed float R component")                    \
    action(NGLI_FORMAT_R32G32_UINT,           8, "r32g32_uint",          "32-bit unsigned integer RG components")              \
    action(NGLI_FORMAT_R32G32_SINT,           8, "r32g32_sint",          "32-bit signed integer RG components")                \
    action(NGLI_FORMAT_R32G32_SFLOAT,         8, "r32g32_sfloat",        "32-bit signed float RG components")                  \
    action(NGLI_FORMAT_R32G32B32_UINT,       12, "r32g32b32_uint",       "32-bit unsigned integer RGB components")             \
    action(NGLI_FORMAT_R32G32B32_SINT,       12, "r32g32b32_sint",       "32-bit signed integer RGB components")               \
    action(NGLI_FORMAT_R32G32B32_SFLOAT,     12, "r32g32b32_sfloat",     "32-bit signed float RGB components")                 \
    action(NGLI_FORMAT_R32G32B32A32_UINT,    16, "r32g32b32a32_uint",    "32-bit unsigned integer RGBA components")            \
    action(NGLI_FORMAT_R32G32B32A32_SINT,    16, "r32g32b32a32_sint",    "32-bit signed integer RGBA components")              \
    action(NGLI_FORMAT_R32G32B32A32_SFLOAT,  16, "r32g32b32a32_sfloat",  "32-bit signed float RGBA components")                \
    action(NGLI_FORMAT_D16_UNORM,             2, "d16_unorm",            "16-bit unsigned normalized depth component")         \
    action(NGLI_FORMAT_X8_D24_UNORM_PACK32,   4, "d24_unorm",            "32-bit packed format that has 24-bit unsigned "      \
                                                                         "normalized depth component + 8-bit of unused data")  \
    action(NGLI_FORMAT_D32_SFLOAT,            4, "d32_sfloat",           "32-bit signed float depth component")                \
    action(NGLI_FORMAT_D24_UNORM_S8_UINT,     4, "d24_unorm_s8_uint",    "32-bit packed format that has 24-bit unsigned "      \
                                                                         "normalized depth component + 8-bit unsigned "        \
                                                                         "integer stencil component")                          \
    action(NGLI_FORMAT_D32_SFLOAT_S8_UINT,    8, "d32_sfloat_s8_uint",   "64-bit packed format that has 32-bit signed "        \
                                                                         "float depth component + 8-bit unsigned integer "     \
                                                                         "stencil component + 24-bit of unused data")          \

#endif
