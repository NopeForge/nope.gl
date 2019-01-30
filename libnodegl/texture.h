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

#ifndef TEXTURE_H
#define TEXTURE_H

#include "glincludes.h"
#include "glcontext.h"

int ngli_texture_filter_has_mipmap(GLint filter);
int ngli_texture_filter_has_linear_filtering(GLint filter);

#define NGLI_TEXTURE_PARAM_DEFAULTS {          \
    .dimensions = 2,                           \
    .format = NGLI_FORMAT_UNDEFINED,           \
    .min_filter = GL_NEAREST,                  \
    .mag_filter = GL_NEAREST,                  \
    .wrap_s = GL_CLAMP_TO_EDGE,                \
    .wrap_t = GL_CLAMP_TO_EDGE,                \
    .wrap_r = GL_CLAMP_TO_EDGE,                \
    .access = GL_READ_WRITE                    \
}

#define NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY (1 << 0)

struct texture_params {
    int dimensions;
    int format;
    int width;
    int height;
    int depth;
    int samples;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
    GLint wrap_r;
    GLenum access;
    int immutable;
    int usage;
    int external_storage;
    int external_oes;
    int rectangle;
};

struct texture {
    struct glcontext *gl;
    struct texture_params params;
    int wrapped;
    int external_storage;

    GLenum target;
    GLuint id;
    GLint format;
    GLint internal_format;
    GLenum format_type;
};

int ngli_texture_init(struct texture *s,
                      struct glcontext *gl,
                      const struct texture_params *params);

int ngli_texture_wrap(struct texture *s,
                      struct glcontext *gl,
                      const struct texture_params *params,
                      GLuint id);

void ngli_texture_set_id(struct texture *s, GLuint id);
void ngli_texture_set_dimensions(struct texture *s, int width, int height, int depth);

int ngli_texture_has_mipmap(const struct texture *s);
int ngli_texture_has_linear_filtering(const struct texture *s);
int ngli_texture_match_dimensions(const struct texture *s, int width, int height, int depth);

int ngli_texture_upload(struct texture *s, const uint8_t *data);
int ngli_texture_generate_mipmap(struct texture *s);

void ngli_texture_reset(struct texture *s);

#endif
