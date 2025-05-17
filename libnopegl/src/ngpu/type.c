/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2022 GoPro Inc.
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

#include "ngpu/type.h"

/* This needs to match the GLSL type name */
static const char * const type_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_I32]                         = "int",
    [NGPU_TYPE_IVEC2]                       = "ivec2",
    [NGPU_TYPE_IVEC3]                       = "ivec3",
    [NGPU_TYPE_IVEC4]                       = "ivec4",
    [NGPU_TYPE_U32]                         = "uint",
    [NGPU_TYPE_UVEC2]                       = "uvec2",
    [NGPU_TYPE_UVEC3]                       = "uvec3",
    [NGPU_TYPE_UVEC4]                       = "uvec4",
    [NGPU_TYPE_F32]                         = "float",
    [NGPU_TYPE_VEC2]                        = "vec2",
    [NGPU_TYPE_VEC3]                        = "vec3",
    [NGPU_TYPE_VEC4]                        = "vec4",
    [NGPU_TYPE_MAT3]                        = "mat3",
    [NGPU_TYPE_MAT4]                        = "mat4",
    [NGPU_TYPE_BOOL]                        = "bool",
    [NGPU_TYPE_SAMPLER_2D]                  = "sampler2D",
    [NGPU_TYPE_SAMPLER_2D_ARRAY]            = "sampler2DArray",
    [NGPU_TYPE_SAMPLER_2D_RECT]             = "sampler2DRect",
    [NGPU_TYPE_SAMPLER_3D]                  = "sampler3D",
    [NGPU_TYPE_SAMPLER_CUBE]                = "samplerCube",
    [NGPU_TYPE_SAMPLER_EXTERNAL_OES]        = "samplerExternalOES",
    [NGPU_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT] = "__samplerExternal2DY2YEXT",
    [NGPU_TYPE_IMAGE_2D]                    = "image2D",
    [NGPU_TYPE_IMAGE_2D_ARRAY]              = "image2DArray",
    [NGPU_TYPE_IMAGE_3D]                    = "image3D",
    [NGPU_TYPE_IMAGE_CUBE]                  = "imageCube",
    [NGPU_TYPE_UNIFORM_BUFFER]              = "uniform",
    [NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC]      = "uniform",
    [NGPU_TYPE_STORAGE_BUFFER]              = "buffer",
    [NGPU_TYPE_STORAGE_BUFFER_DYNAMIC]      = "buffer",
};

const char *ngpu_type_get_name(int type)
{
    return type_map[type];
}
