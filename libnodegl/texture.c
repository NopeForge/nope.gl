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

#include <string.h>

#include "log.h"
#include "utils.h"
#include "format.h"
#include "glincludes.h"
#include "glcontext.h"
#include "texture.h"

int ngli_texture_filter_has_mipmap(GLint filter)
{
    switch (filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        return 1;
    default:
        return 0;
    }
}

int ngli_texture_filter_has_linear_filtering(GLint filter)
{
    switch (filter) {
    case GL_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        return 1;
    default:
        return 0;
    }
}

static void texture_set_image(struct texture *s, const uint8_t *data)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    switch (s->target) {
    case GL_TEXTURE_2D:
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data);
        break;
    case GL_TEXTURE_3D:
        ngli_glTexImage3D(gl, GL_TEXTURE_3D, 0, s->internal_format, params->width, params->height, params->depth, 0, s->format, s->format_type, data);
        break;
    case GL_TEXTURE_CUBE_MAP: {
        const int face_size = data ? s->bytes_per_pixel * params->width * params->height : 0;
        ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data);
        ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data + face_size);
        ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data + face_size * 2);
        ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data + face_size * 3);
        ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data + face_size * 4);
        ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, s->internal_format, params->width, params->height, 0, s->format, s->format_type, data + face_size * 5);
        break;
    }
    }
}

static void texture2d_set_sub_image(struct texture *s, const uint8_t *data)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, 0, params->width, params->height, s->format, s->format_type, data);
}

static void texture3d_set_sub_image(struct texture *s, const uint8_t *data)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    ngli_glTexSubImage3D(gl, GL_TEXTURE_3D, 0, 0, 0, 0, params->width, params->height, params->depth, s->format, s->format_type, data);
}

static void texturecube_set_sub_image(struct texture *s, const uint8_t *data)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    const int face_size = data ? s->bytes_per_pixel * params->width * params->height : 0;
    ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, params->width, params->height, s->format, s->format_type, data);
    ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, params->width, params->height, s->format, s->format_type, data + face_size);
    ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, 0, 0, params->width, params->height, s->format, s->format_type, data + face_size * 2);
    ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 0, 0, params->width, params->height, s->format, s->format_type, data + face_size * 3);
    ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, params->width, params->height, s->format, s->format_type, data + face_size * 4);
    ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, 0, 0, params->width, params->height, s->format, s->format_type, data + face_size * 5);
}

static void texture_set_sub_image(struct texture *s, const uint8_t *data)
{
    switch (s->target) {
    case GL_TEXTURE_2D:
        texture2d_set_sub_image(s, data);
        break;
    case GL_TEXTURE_3D:
        texture3d_set_sub_image(s, data);
        break;
    case GL_TEXTURE_CUBE_MAP:
        texturecube_set_sub_image(s, data);
        break;
    }
}

static void texture_set_storage(struct texture *s)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    switch (s->target) {
    case GL_TEXTURE_2D: {
        int mipmap_levels = 1;
        if (ngli_texture_has_mipmap(s))
            while ((params->width | params->height) >> mipmap_levels)
                mipmap_levels += 1;
        ngli_glTexStorage2D(gl, s->target, mipmap_levels, s->internal_format, params->width, params->height);
        break;
    }
    case GL_TEXTURE_3D:
        ngli_glTexStorage3D(gl, s->target, 1, s->internal_format, params->width, params->height, params->depth);
        break;
    case GL_TEXTURE_CUBE_MAP:
        /* glTexStorage2D automatically accomodates for 6 faces when using the cubemap target */
        ngli_glTexStorage2D(gl, s->target, 1, s->internal_format, params->width, params->height);
        break;
    }
}

static int renderbuffer_check_samples(struct texture *s)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    int max_samples = gl->max_samples;
    if (gl->features & NGLI_FEATURE_INTERNALFORMAT_QUERY)
        ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, s->format, GL_SAMPLES, 1, &max_samples);

    if (params->samples > max_samples) {
        LOG(WARNING, "renderbuffer format 0x%x does not support samples %d (maximum %d)",
            s->format, params->samples, max_samples);
        return -1;
    }

    return 0;
}

static void renderbuffer_set_storage(struct texture *s)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    if (params->samples > 0)
        ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, params->samples, s->format, params->width, params->height);
    else
        ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, s->format, params->width, params->height);
}

static int texture_init_fields(struct texture *s)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    if (params->usage & NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY) {
        s->target = GL_RENDERBUFFER;
        int ret = ngli_format_get_gl_renderbuffer_format(gl, params->format, &s->format);
        if (ret < 0)
            return ret;
        s->internal_format = s->format;

        ret = renderbuffer_check_samples(s);
        if (ret < 0)
            return ret;
        return 0;
    }

    /* TODO: add multisample support for textures */
    ngli_assert(!params->samples);

    if (params->dimensions == 2)
        s->target = GL_TEXTURE_2D;
    else if (params->dimensions == 3)
        s->target = GL_TEXTURE_3D;
    else
        ngli_assert(0);

    if (params->cubemap) {
        ngli_assert(params->dimensions == 3);
        s->target = GL_TEXTURE_CUBE_MAP;
    }

    if (params->external_oes) {
        ngli_assert(params->dimensions == 2);
        s->target = GL_TEXTURE_EXTERNAL_OES;
    } else if (params->rectangle) {
        ngli_assert(params->dimensions == 2);
        s->target = GL_TEXTURE_RECTANGLE;
    }

    int ret = ngli_format_get_gl_texture_format(gl,
                                                params->format,
                                                &s->format,
                                                &s->internal_format,
                                                &s->format_type);
    if (ret < 0)
        return ret;

    s->bytes_per_pixel = ngli_format_get_bytes_per_pixel(params->format);

    if (params->external_storage || params->external_oes)
        s->external_storage = 1;

    return 0;
}

static int is_pow2(int x)
{
    return x && !(x & (x - 1));
}

int ngli_texture_init(struct texture *s,
                      struct glcontext *gl,
                      const struct texture_params *params)
{
    s->gl = gl;
    s->params = *params;

    int ret = texture_init_fields(s);
    if (ret < 0)
        return ret;

    if (s->target == GL_RENDERBUFFER) {
        ngli_glGenRenderbuffers(gl, 1, &s->id);
        ngli_glBindRenderbuffer(gl, s->target, s->id);
        renderbuffer_set_storage(s);
    } else {
        ngli_glGenTextures(gl, 1, &s->id);
        ngli_glBindTexture(gl, s->target, s->id);
        GLint min_filter = params->min_filter;
        if (!(gl->features & NGLI_FEATURE_TEXTURE_NPOT) &&
            (!is_pow2(params->width) || !is_pow2(params->height))) {
            LOG(WARNING, "context does not support non-power of two textures, "
                "mipmapping will be disabled");
            min_filter = ngli_texture_filter_has_linear_filtering(params->min_filter) ? GL_LINEAR : GL_NEAREST;
        }
        ngli_glTexParameteri(gl, s->target, GL_TEXTURE_MIN_FILTER, min_filter);
        ngli_glTexParameteri(gl, s->target, GL_TEXTURE_MAG_FILTER, params->mag_filter);
        ngli_glTexParameteri(gl, s->target, GL_TEXTURE_WRAP_S, params->wrap_s);
        ngli_glTexParameteri(gl, s->target, GL_TEXTURE_WRAP_T, params->wrap_t);
        if (s->target == GL_TEXTURE_3D || s->target == GL_TEXTURE_CUBE_MAP)
            ngli_glTexParameteri(gl, s->target, GL_TEXTURE_WRAP_R, params->wrap_r);

        if (!s->external_storage) {
            if (!params->width || !params->height ||
                (params->dimensions == 3 && !params->depth)) {
                LOG(ERROR, "invalid texture dimensions %dx%dx%d",
                    params->width, params->height, params->depth);
                ngli_texture_reset(s);
                return -1;
            }
            if (params->immutable) {
                texture_set_storage(s);
            } else {
                texture_set_image(s, NULL);
            }
        }
    }

    return 0;
}

int ngli_texture_wrap(struct texture *s,
                      struct glcontext *gl,
                      const struct texture_params *params,
                      GLuint texture)
{
    s->gl = gl;
    s->params = *params;

    int ret = texture_init_fields(s);
    if (ret < 0)
        return ret;

    s->id = texture;
    s->wrapped = 1;
    s->external_storage = 1;

    return 0;
}

void ngli_texture_set_id(struct texture *s, GLuint id)
{
    /* only wrapped textures can update their id with this function */
    ngli_assert(s->wrapped);

    s->id = id;
}

void ngli_texture_set_dimensions(struct texture *s, int width, int height, int depth)
{
    /* only textures with external storage can update their dimensions with this function */
    ngli_assert(s->external_storage);

    struct texture_params *params = &s->params;
    params->width = width;
    params->height = height;
    params->depth = depth;
}

int ngli_texture_has_mipmap(const struct texture *s)
{
    return ngli_texture_filter_has_mipmap(s->params.min_filter);
}

int ngli_texture_has_linear_filtering(const struct texture *s)
{
    return ngli_texture_filter_has_linear_filtering(s->params.min_filter);
}

int ngli_texture_match_dimensions(const struct texture *s, int width, int height, int depth)
{
    const struct texture_params *params = &s->params;
    return params->width == width && params->height == height && params->depth == depth;
}

int ngli_texture_upload(struct texture *s, const uint8_t *data)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    /* texture with external storage (including wrapped textures and render
     * buffers) cannot update their content with this function */
    ngli_assert(!s->external_storage && !(params->usage & NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY));

    ngli_glBindTexture(gl, s->target, s->id);
    if (data) {
        texture_set_sub_image(s, data);
        if (ngli_texture_has_mipmap(s))
            ngli_glGenerateMipmap(gl, s->target);
    }
    ngli_glBindTexture(gl, s->target, 0);

    return 0;
}

int ngli_texture_generate_mipmap(struct texture *s)
{
    struct glcontext *gl = s->gl;
    const struct texture_params *params = &s->params;

    ngli_assert(!(params->usage & NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY));

    ngli_glBindTexture(gl, s->target, s->id);
    ngli_glGenerateMipmap(gl, s->target);
    return 0;
}

void ngli_texture_reset(struct texture *s)
{
    struct glcontext *gl = s->gl;
    if (!gl)
        return;

    if (!s->wrapped) {
        if (s->target == GL_RENDERBUFFER)
            ngli_glDeleteRenderbuffers(gl, 1, &s->id);
        else
            ngli_glDeleteTextures(gl, 1, &s->id);
    }

    memset(s, 0, sizeof(*s));
}
