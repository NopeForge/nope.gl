/*
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

#include "type.h"

/* This needs to match the GLSL type name */
static const char * const type_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_INT]                         = "int",
    [NGLI_TYPE_IVEC2]                       = "ivec2",
    [NGLI_TYPE_IVEC3]                       = "ivec3",
    [NGLI_TYPE_IVEC4]                       = "ivec4",
    [NGLI_TYPE_UINT]                        = "uint",
    [NGLI_TYPE_UIVEC2]                      = "uvec2",
    [NGLI_TYPE_UIVEC3]                      = "uvec3",
    [NGLI_TYPE_UIVEC4]                      = "uvec4",
    [NGLI_TYPE_FLOAT]                       = "float",
    [NGLI_TYPE_VEC2]                        = "vec2",
    [NGLI_TYPE_VEC3]                        = "vec3",
    [NGLI_TYPE_VEC4]                        = "vec4",
    [NGLI_TYPE_MAT3]                        = "mat3",
    [NGLI_TYPE_MAT4]                        = "mat4",
    [NGLI_TYPE_BOOL]                        = "bool",
    [NGLI_TYPE_SAMPLER_2D]                  = "sampler2D",
    [NGLI_TYPE_SAMPLER_2D_RECT]             = "sampler2DRect",
    [NGLI_TYPE_SAMPLER_3D]                  = "sampler3D",
    [NGLI_TYPE_SAMPLER_CUBE]                = "samplerCube",
    [NGLI_TYPE_SAMPLER_EXTERNAL_OES]        = "samplerExternalOES",
    [NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT] = "__samplerExternal2DY2YEXT",
    [NGLI_TYPE_IMAGE_2D]                    = "image2D",
    [NGLI_TYPE_UNIFORM_BUFFER]              = "uniform",
    [NGLI_TYPE_STORAGE_BUFFER]              = "buffer",
};

const char *ngli_type_get_name(int type)
{
    return type_map[type];
}
