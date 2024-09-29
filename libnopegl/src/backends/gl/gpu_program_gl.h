/*
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

#ifndef GPU_PROGRAM_GL_H
#define GPU_PROGRAM_GL_H

#include "glincludes.h"
#include "gpu_program.h"

struct gpu_ctx;

struct gpu_program_gl {
    struct gpu_program parent;
    GLuint id;
};

struct gpu_program *ngli_gpu_program_gl_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_program_gl_init(struct gpu_program *s, const struct gpu_program_params *params);
void ngli_gpu_program_gl_freep(struct gpu_program **sp);

#endif
