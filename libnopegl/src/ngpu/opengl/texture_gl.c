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

#include <string.h>

#include "ctx_gl.h"
#include "format_gl.h"
#include "glcontext.h"
#include "glincludes.h"
#include "log.h"
#include "memory.h"
#include "texture_gl.h"
#include "utils.h"

static const GLint gl_filter_map[NGPU_NB_FILTER][NGPU_NB_MIPMAP] = {
    [NGPU_FILTER_NEAREST] = {
        [NGPU_MIPMAP_FILTER_NONE]    = GL_NEAREST,
        [NGPU_MIPMAP_FILTER_NEAREST] = GL_NEAREST_MIPMAP_NEAREST,
        [NGPU_MIPMAP_FILTER_LINEAR]  = GL_NEAREST_MIPMAP_LINEAR,
    },
    [NGPU_FILTER_LINEAR] = {
        [NGPU_MIPMAP_FILTER_NONE]    = GL_LINEAR,
        [NGPU_MIPMAP_FILTER_NEAREST] = GL_LINEAR_MIPMAP_NEAREST,
        [NGPU_MIPMAP_FILTER_LINEAR]  = GL_LINEAR_MIPMAP_LINEAR,
    },
};

GLint ngpu_texture_get_gl_min_filter(int min_filter, int mipmap_filter)
{
    return gl_filter_map[min_filter][mipmap_filter];
}

GLint ngpu_texture_get_gl_mag_filter(int mag_filter)
{
    return gl_filter_map[mag_filter][NGPU_MIPMAP_FILTER_NONE];
}

static const GLint gl_wrap_map[NGPU_NB_WRAP] = {
    [NGPU_WRAP_CLAMP_TO_EDGE]   = GL_CLAMP_TO_EDGE,
    [NGPU_WRAP_MIRRORED_REPEAT] = GL_MIRRORED_REPEAT,
    [NGPU_WRAP_REPEAT]          = GL_REPEAT,
};

GLint ngpu_texture_get_gl_wrap(int wrap)
{
    return gl_wrap_map[wrap];
}

static GLbitfield get_gl_barriers(uint32_t usage)
{
    GLbitfield barriers = 0;
    if (usage & NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT)
        barriers |= GL_TEXTURE_UPDATE_BARRIER_BIT;
    if (usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT)
        barriers |= GL_TEXTURE_UPDATE_BARRIER_BIT;
    if (usage & NGPU_TEXTURE_USAGE_STORAGE_BIT)
        barriers |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    if (usage & NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)
        barriers |= GL_FRAMEBUFFER_BARRIER_BIT;
    return barriers;
}

static void texture_allocate(struct ngpu_texture *s)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_texture_params *params = &s->params;

    switch (s_priv->target) {
    case GL_TEXTURE_2D:
        gl->funcs.TexImage2D(s_priv->target, 0, s_priv->internal_format, params->width, params->height, 0, s_priv->format, s_priv->format_type, NULL);
        break;
    case GL_TEXTURE_2D_ARRAY:
        gl->funcs.TexImage3D(s_priv->target, 0, s_priv->internal_format, params->width, params->height, s_priv->array_layers, 0, s_priv->format, s_priv->format_type, NULL);
        break;
    case GL_TEXTURE_3D:
        gl->funcs.TexImage3D(s_priv->target, 0, s_priv->internal_format, params->width, params->height, params->depth, 0, s_priv->format, s_priv->format_type, NULL);
        break;
    case GL_TEXTURE_CUBE_MAP: {
        for (int face = 0; face < 6; face++) {
            gl->funcs.TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, s_priv->internal_format, params->width, params->height, 0, s_priv->format, s_priv->format_type, NULL);
        }
        break;
    }
    }
}

static int get_mipmap_levels(const struct ngpu_texture *s)
{
    const struct ngpu_texture_params *params = &s->params;

    int mipmap_levels = 1;
    if (params->mipmap_filter != NGPU_MIPMAP_FILTER_NONE)
        mipmap_levels = ngli_log2(params->width | params->height | 1);
    return mipmap_levels;
}

static void texture_allocate_storage(struct ngpu_texture *s)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_texture_params *params = &s->params;

    const int mipmap_levels = get_mipmap_levels(s);
    switch (s_priv->target) {
    case GL_TEXTURE_2D:
        gl->funcs.TexStorage2D(s_priv->target, mipmap_levels, s_priv->internal_format, params->width, params->height);
        break;
    case GL_TEXTURE_2D_ARRAY:
        gl->funcs.TexStorage3D(s_priv->target, mipmap_levels, s_priv->internal_format, params->width, params->height, s_priv->array_layers);
        break;
    case GL_TEXTURE_3D:
        gl->funcs.TexStorage3D(s_priv->target, 1, s_priv->internal_format, params->width, params->height, params->depth);
        break;
    case GL_TEXTURE_CUBE_MAP:
        /* glTexStorage2D automatically accomodates for 6 faces when using the cubemap target */
        gl->funcs.TexStorage2D(s_priv->target, mipmap_levels, s_priv->internal_format, params->width, params->height);
        break;
    }
}

static void texture_upload(struct ngpu_texture *s, const uint8_t *data, int linesize)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_texture_params *params = &s->params;

    if (!linesize)
        linesize = params->width;

    const int bytes_per_row = linesize * s_priv->bytes_per_pixel;
    const int alignment = NGLI_MIN(bytes_per_row & ~(bytes_per_row - 1), 8);
    gl->funcs.PixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    gl->funcs.PixelStorei(GL_UNPACK_ROW_LENGTH, linesize);

    switch (s_priv->target) {
    case GL_TEXTURE_2D:
        gl->funcs.TexSubImage2D(s_priv->target, 0, 0, 0,
                                params->width, params->height,
                                s_priv->format, s_priv->format_type, data);
        break;
    case GL_TEXTURE_2D_ARRAY:
        gl->funcs.TexSubImage3D(s_priv->target, 0, 0, 0, 0,
                                params->width, params->height, s_priv->array_layers,
                                s_priv->format, s_priv->format_type, data);
        break;
    case GL_TEXTURE_3D:
        gl->funcs.TexSubImage3D(s_priv->target, 0, 0, 0, 0,
                                params->width, params->height, params->depth,
                                s_priv->format, s_priv->format_type, data);
        break;
    case GL_TEXTURE_CUBE_MAP: {
        const int face_size = s_priv->bytes_per_pixel * linesize * params->height;
        for (int face = 0; face < 6; face++) {
            gl->funcs.TexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 0, 0,
                                    params->width, params->height,
                                    s_priv->format, s_priv->format_type, data);
            data += face_size;
        }
        break;
    }
    }

    gl->funcs.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->funcs.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

static int renderbuffer_check_samples(struct ngpu_texture *s)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_limits *limits = &gl->limits;
    const struct ngpu_texture_params *params = &s->params;

    int max_samples = limits->max_samples;
    if (gl->features & NGLI_FEATURE_GL_INTERNALFORMAT_QUERY)
        gl->funcs.GetInternalformativ(GL_RENDERBUFFER, s_priv->format, GL_SAMPLES, 1, &max_samples);

    if (params->samples > max_samples) {
        LOG(WARNING, "renderbuffer format 0x%x does not support samples %d (maximum %d)",
            s_priv->format, params->samples, max_samples);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    return 0;
}

static void renderbuffer_allocate_storage(struct ngpu_texture *s)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_texture_params *params = &s->params;

    if (params->samples > 0)
        gl->funcs.RenderbufferStorageMultisample(GL_RENDERBUFFER, params->samples, s_priv->format, params->width, params->height);
    else
        gl->funcs.RenderbufferStorage(GL_RENDERBUFFER, s_priv->format, params->width, params->height);
}

#define COLOR_USAGE NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT
#define DEPTH_USAGE NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
#define TRANSIENT_COLOR_USAGE (COLOR_USAGE | NGPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT)
#define TRANSIENT_DEPTH_USAGE (DEPTH_USAGE | NGPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT)

static int texture_init_fields(struct ngpu_texture *s, const struct ngpu_texture_params *params)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    s->params = *params;

    if (!s_priv->wrapped)
        ngli_assert(params->width && params->height);

    int32_t depth = 1;
    if (params->type == NGPU_TEXTURE_TYPE_3D) {
        if (!s_priv->wrapped)
            ngli_assert(params->depth);
        depth = params->depth;
    }
    s->params.depth = depth;

    s_priv->array_layers = 1;
    if (params->type == NGPU_TEXTURE_TYPE_CUBE) {
        s_priv->array_layers = 6;
    } else if (params->type == NGPU_TEXTURE_TYPE_2D_ARRAY) {
        s_priv->array_layers = params->depth;
    }

    if (!s_priv->wrapped &&
        (params->usage == COLOR_USAGE ||
         params->usage == DEPTH_USAGE ||
         params->usage == TRANSIENT_COLOR_USAGE ||
         params->usage == TRANSIENT_DEPTH_USAGE)) {
        const struct ngpu_format_gl *format_gl = ngpu_format_get_gl_texture_format(gl, params->format);

        s_priv->target = GL_RENDERBUFFER;
        s_priv->format = format_gl->internal_format;
        s_priv->internal_format = format_gl->internal_format;

        int ret = renderbuffer_check_samples(s);
        if (ret < 0)
            return ret;
        return 0;
    }

    /* TODO: add multisample support for textures */
    ngli_assert(!params->samples);

    if (params->type == NGPU_TEXTURE_TYPE_2D)
        s_priv->target = GL_TEXTURE_2D;
    else if (params->type == NGPU_TEXTURE_TYPE_2D_ARRAY)
        s_priv->target = GL_TEXTURE_2D_ARRAY;
    else if (params->type == NGPU_TEXTURE_TYPE_3D)
        s_priv->target = GL_TEXTURE_3D;
    else if (params->type == NGPU_TEXTURE_TYPE_CUBE)
        s_priv->target = GL_TEXTURE_CUBE_MAP;
    else
        ngli_assert(0);

    const struct ngpu_format_gl *format_gl = ngpu_format_get_gl_texture_format(gl, params->format);
    s_priv->format          = format_gl->format;
    s_priv->internal_format = format_gl->internal_format;
    s_priv->format_type     = format_gl->type;
    s_priv->bytes_per_pixel = ngpu_format_get_bytes_per_pixel(params->format);
    s_priv->barriers = get_gl_barriers(params->usage);

    return 0;
}

struct ngpu_texture *ngpu_texture_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_texture_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_texture *)s;
}

int ngpu_texture_gl_init(struct ngpu_texture *s, const struct ngpu_texture_params *params)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    int ret = texture_init_fields(s, params);
    if (ret < 0)
        return ret;

    if (s_priv->target == GL_RENDERBUFFER) {
        gl->funcs.GenRenderbuffers(1, &s_priv->id);
        gl->funcs.BindRenderbuffer(s_priv->target, s_priv->id);
        renderbuffer_allocate_storage(s);
        return 0;
    }

    gl->funcs.GenTextures(1, &s_priv->id);
    gl->funcs.BindTexture(s_priv->target, s_priv->id);
    const GLint min_filter = ngpu_texture_get_gl_min_filter(params->min_filter, s->params.mipmap_filter);
    const GLint mag_filter = ngpu_texture_get_gl_mag_filter(params->mag_filter);
    const GLint wrap_s = ngpu_texture_get_gl_wrap(params->wrap_s);
    const GLint wrap_t = ngpu_texture_get_gl_wrap(params->wrap_t);
    const GLint wrap_r = ngpu_texture_get_gl_wrap(params->wrap_r);
    gl->funcs.TexParameteri(s_priv->target, GL_TEXTURE_MIN_FILTER, min_filter);
    gl->funcs.TexParameteri(s_priv->target, GL_TEXTURE_MAG_FILTER, mag_filter);
    gl->funcs.TexParameteri(s_priv->target, GL_TEXTURE_WRAP_S, wrap_s);
    gl->funcs.TexParameteri(s_priv->target, GL_TEXTURE_WRAP_T, wrap_t);
    if (s_priv->target == GL_TEXTURE_2D_ARRAY ||
        s_priv->target == GL_TEXTURE_3D ||
        s_priv->target == GL_TEXTURE_CUBE_MAP)
        gl->funcs.TexParameteri(s_priv->target, GL_TEXTURE_WRAP_R, wrap_r);
    if (gl->features & NGLI_FEATURE_GL_TEXTURE_STORAGE) {
        texture_allocate_storage(s);
    } else {
        texture_allocate(s);
    }

    return 0;
}

int ngpu_texture_gl_wrap(struct ngpu_texture *s, const struct ngpu_texture_gl_wrap_params *wrap_params)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    s_priv->wrapped = 1;

    int ret = texture_init_fields(s, wrap_params->params);
    if (ret < 0)
        return ret;

    s_priv->id = wrap_params->texture;
    if (wrap_params->target)
        s_priv->target = wrap_params->target;

    return 0;
}

void ngpu_texture_gl_set_id(struct ngpu_texture *s, GLuint id)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;

    /* only wrapped textures can update their id with this function */
    ngli_assert(s_priv->wrapped);

    s_priv->id = id;
}

void ngpu_texture_gl_set_dimensions(struct ngpu_texture *s, int32_t width, int32_t height, int depth)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;

    /* only wrapped textures can update their dimensions with this function */
    ngli_assert(s_priv->wrapped);

    struct ngpu_texture_params *params = &s->params;
    params->width = width;
    params->height = height;
    params->depth = depth;
}

int ngpu_texture_gl_upload(struct ngpu_texture *s, const uint8_t *data, int linesize)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_texture_params *params = &s->params;

    /* Wrapped textures and render buffers cannot update their content with
     * this function */
    ngli_assert(!s_priv->wrapped);
    ngli_assert(params->usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT);

    gl->funcs.BindTexture(s_priv->target, s_priv->id);
    if (data) {
        texture_upload(s, data, linesize);
        if (params->mipmap_filter != NGPU_MIPMAP_FILTER_NONE)
            gl->funcs.GenerateMipmap(s_priv->target);
    }
    gl->funcs.BindTexture(s_priv->target, 0);

    return 0;
}

int ngpu_texture_gl_generate_mipmap(struct ngpu_texture *s)
{
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct ngpu_texture_params *params = &s->params;

    ngli_assert(params->usage & NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT);
    ngli_assert(params->usage & NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT);

    gl->funcs.BindTexture(s_priv->target, s_priv->id);
    gl->funcs.GenerateMipmap(s_priv->target);
    return 0;
}

void ngpu_texture_gl_freep(struct ngpu_texture **sp)
{
    if (!*sp)
        return;

    struct ngpu_texture *s = *sp;
    struct ngpu_texture_gl *s_priv = (struct ngpu_texture_gl *)s;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    if (!s_priv->wrapped) {
        if (s_priv->target == GL_RENDERBUFFER)
            gl->funcs.DeleteRenderbuffers(1, &s_priv->id);
        else
            gl->funcs.DeleteTextures(1, &s_priv->id);
    }

    ngli_freep(sp);
}
