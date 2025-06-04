/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2019-2022 GoPro Inc.
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

#ifndef NGPU_TYPE_H
#define NGPU_TYPE_H

enum ngpu_precision {
    NGPU_PRECISION_AUTO,
    NGPU_PRECISION_HIGH,
    NGPU_PRECISION_MEDIUM,
    NGPU_PRECISION_LOW,
    NGPU_PRECISION_NB,
    NGPU_PRECISION_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_type {
    NGPU_TYPE_NONE,
    NGPU_TYPE_I32,
    NGPU_TYPE_IVEC2,
    NGPU_TYPE_IVEC3,
    NGPU_TYPE_IVEC4,
    NGPU_TYPE_U32,
    NGPU_TYPE_UVEC2,
    NGPU_TYPE_UVEC3,
    NGPU_TYPE_UVEC4,
    NGPU_TYPE_F32,
    NGPU_TYPE_VEC2,
    NGPU_TYPE_VEC3,
    NGPU_TYPE_VEC4,
    NGPU_TYPE_MAT3,
    NGPU_TYPE_MAT4,
    NGPU_TYPE_BOOL,
    NGPU_TYPE_SAMPLER_2D,
    NGPU_TYPE_SAMPLER_2D_ARRAY,
    NGPU_TYPE_SAMPLER_2D_RECT,
    NGPU_TYPE_SAMPLER_3D,
    NGPU_TYPE_SAMPLER_CUBE,
    NGPU_TYPE_SAMPLER_EXTERNAL_OES,
    NGPU_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT,
    NGPU_TYPE_IMAGE_2D,
    NGPU_TYPE_IMAGE_2D_ARRAY,
    NGPU_TYPE_IMAGE_3D,
    NGPU_TYPE_IMAGE_CUBE,
    NGPU_TYPE_UNIFORM_BUFFER,
    NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC,
    NGPU_TYPE_STORAGE_BUFFER,
    NGPU_TYPE_STORAGE_BUFFER_DYNAMIC,
    NGPU_TYPE_NB,
    NGPU_TYPE_MAX_ENUM = 0x7FFFFFFF
};

const char *ngpu_type_get_name(enum ngpu_type type);

#endif
