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

#ifndef NGPU_TEXTURE_GL_H
#define NGPU_TEXTURE_GL_H

#include "glincludes.h"
#include "ngpu/texture.h"

GLint ngpu_texture_get_gl_min_filter(enum ngpu_filter min_filter, enum ngpu_mipmap_filter mipmap_filter);
GLint ngpu_texture_get_gl_mag_filter(enum ngpu_filter mag_filter);
GLint ngpu_texture_get_gl_wrap(enum ngpu_wrap wrap);

struct ngpu_texture_gl_wrap_params {
    const struct ngpu_texture_params *params;
    GLuint texture;
    GLuint target;
};

struct ngpu_texture_gl {
    struct ngpu_texture parent;
    GLenum target;
    GLuint id;
    GLenum format;
    GLenum internal_format;
    GLenum format_type;
    int wrapped;
    size_t bytes_per_pixel;
    uint32_t array_layers;
    GLbitfield barriers;
};

struct ngpu_texture *ngpu_texture_gl_create(struct ngpu_ctx *gpu_ctx);
int ngpu_texture_gl_init(struct ngpu_texture *s, const struct ngpu_texture_params *params);
int ngpu_texture_gl_wrap(struct ngpu_texture *s, const struct ngpu_texture_gl_wrap_params *wrap_params);
void ngpu_texture_gl_set_id(struct ngpu_texture *s, GLuint id);
void ngpu_texture_gl_set_dimensions(struct ngpu_texture *s, uint32_t width, uint32_t height, uint32_t depth);
int ngpu_texture_gl_upload(struct ngpu_texture *s, const uint8_t *data, int linesize);
int ngpu_texture_gl_upload_with_params(struct ngpu_texture *s, const uint8_t *data, const struct ngpu_texture_transfer_params *transfer_params);
int ngpu_texture_gl_generate_mipmap(struct ngpu_texture *s);
void ngpu_texture_gl_freep(struct ngpu_texture **sp);

#endif
