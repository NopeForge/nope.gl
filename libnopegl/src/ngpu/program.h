/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_PROGRAM_H
#define NGPU_PROGRAM_H

#include "utils/hmap.h"

struct ngpu_ctx;

#define MAX_ID_LEN 128

struct ngpu_program_variable_info {
    int binding;
    int location;
};

enum ngpu_program_shader {
    NGPU_PROGRAM_SHADER_VERT,
    NGPU_PROGRAM_SHADER_FRAG,
    NGPU_PROGRAM_SHADER_COMP,
    NGPU_PROGRAM_SHADER_NB,
    NGPU_PROGRAM_SHADER_MAX_ENUM = 0x7FFFFFFF
};

enum ngpu_program_stage {
    NGPU_PROGRAM_STAGE_VERTEX_BIT   = 1U << NGPU_PROGRAM_SHADER_VERT,
    NGPU_PROGRAM_STAGE_FRAGMENT_BIT = 1U << NGPU_PROGRAM_SHADER_FRAG,
    NGPU_PROGRAM_STAGE_COMPUTE_BIT  = 1U << NGPU_PROGRAM_SHADER_COMP,
    NGPU_PROGRAM_STAGE_MAX_ENUM     = 0x7FFFFFFF
};

struct ngpu_program_params {
    const char *label;
    const char *vertex;
    const char *fragment;
    const char *compute;
};

struct ngpu_program {
    struct ngpu_ctx *gpu_ctx;
    struct hmap *uniforms;
    struct hmap *attributes;
    struct hmap *buffer_blocks;
};

struct ngpu_program *ngpu_program_create(struct ngpu_ctx *gpu_ctx);
int ngpu_program_init(struct ngpu_program *s, const struct ngpu_program_params *params);
void ngpu_program_freep(struct ngpu_program **sp);

#endif
