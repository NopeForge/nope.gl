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

#ifndef GPU_TEXTURE_GL_H
#define GPU_TEXTURE_GL_H

#include "glincludes.h"
#include "gpu_texture.h"

GLint ngli_gpu_texture_get_gl_min_filter(int min_filter, int mipmap_filter);
GLint ngli_gpu_texture_get_gl_mag_filter(int mag_filter);
GLint ngli_gpu_texture_get_gl_wrap(int wrap);

struct gpu_texture_gl_wrap_params {
    const struct gpu_texture_params *params;
    GLuint texture;
    GLuint target;
};

struct gpu_texture_gl {
    struct gpu_texture parent;
    GLenum target;
    GLuint id;
    GLint format;
    GLint internal_format;
    GLenum format_type;
    int wrapped;
    int bytes_per_pixel;
    GLbitfield barriers;
};

struct gpu_texture *ngli_gpu_texture_gl_create(struct gpu_ctx *gpu_ctx);
int ngli_gpu_texture_gl_init(struct gpu_texture *s, const struct gpu_texture_params *params);
int ngli_gpu_texture_gl_wrap(struct gpu_texture *s, const struct gpu_texture_gl_wrap_params *wrap_params);
void ngli_gpu_texture_gl_set_id(struct gpu_texture *s, GLuint id);
void ngli_gpu_texture_gl_set_dimensions(struct gpu_texture *s, int32_t width, int32_t height, int depth);
int ngli_gpu_texture_gl_upload(struct gpu_texture *s, const uint8_t *data, int linesize);
int ngli_gpu_texture_gl_generate_mipmap(struct gpu_texture *s);
void ngli_gpu_texture_gl_freep(struct gpu_texture **sp);

#endif
