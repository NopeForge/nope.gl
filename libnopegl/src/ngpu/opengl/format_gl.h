/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_FORMAT_GL_H
#define NGPU_FORMAT_GL_H

#include <stdint.h>

#include "glincludes.h"
#include "ngpu/format.h"

struct glcontext;

struct ngpu_format_gl {
    GLenum format;
    GLenum internal_format;
    GLenum type;
    uint32_t features;
};

void ngpu_format_gl_init(struct glcontext *gl);
const struct ngpu_format_gl *ngpu_format_get_gl_texture_format(struct glcontext *gl, enum ngpu_format format);

#endif
