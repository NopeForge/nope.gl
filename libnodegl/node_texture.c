/*
 * Copyright 2016 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

static const struct param_choices minfilter_choices = {
    .name = "min_filter",
    .consts = {
        {"nearest",                GL_NEAREST,                .desc=NGLI_DOCSTRING("nearest filtering")},
        {"linear",                 GL_LINEAR,                 .desc=NGLI_DOCSTRING("linear filtering")},
        {"nearest_mipmap_nearest", GL_NEAREST_MIPMAP_NEAREST, .desc=NGLI_DOCSTRING("nearest filtering, nearest mipmap filtering")},
        {"linear_mipmap_nearest",  GL_LINEAR_MIPMAP_NEAREST,  .desc=NGLI_DOCSTRING("linear filtering, nearest mipmap filtering")},
        {"nearest_mipmap_linear",  GL_NEAREST_MIPMAP_LINEAR,  .desc=NGLI_DOCSTRING("nearest filtering, linear mipmap filtering")},
        {"linear_mipmap_linear",   GL_LINEAR_MIPMAP_LINEAR,   .desc=NGLI_DOCSTRING("linear filtering, linear mipmap filtering")},
        {NULL}
    }
};

static const struct param_choices magfilter_choices = {
    .name = "mag_filter",
    .consts = {
        {"nearest", GL_NEAREST, .desc=NGLI_DOCSTRING("nearest filtering")},
        {"linear",  GL_LINEAR,  .desc=NGLI_DOCSTRING("linear filtering")},
        {NULL}
    }
};

static const struct param_choices wrap_choices = {
    .name = "wrap",
    .consts = {
        {"clamp_to_edge",   GL_CLAMP_TO_EDGE,   .desc=NGLI_DOCSTRING("clamp to edge wrapping")},
        {"mirrored_repeat", GL_MIRRORED_REPEAT, .desc=NGLI_DOCSTRING("mirrored repeat wrapping")},
        {"repeat",          GL_REPEAT,          .desc=NGLI_DOCSTRING("repeat pattern wrapping")},
        {NULL}
    }
};

static const struct param_choices access_choices = {
    .name = "access",
    .consts = {
        {"read_only",  GL_READ_ONLY,  .desc=NGLI_DOCSTRING("read only")},
        {"write_only", GL_WRITE_ONLY, .desc=NGLI_DOCSTRING("write only")},
        {"read_write", GL_READ_WRITE, .desc=NGLI_DOCSTRING("read-write")},
        {NULL}
    }
};

#define DECLARE_FORMAT_PARAM(format, name, doc) \
    {name, format, .desc=NGLI_DOCSTRING(doc)},

static const struct param_choices format_choices = {
    .name = "format",
    .consts = {
        NGLI_FORMATS(DECLARE_FORMAT_PARAM)
        {NULL}
    }
};

#define BUFFER_NODES                \
    NGL_NODE_ANIMATEDBUFFERFLOAT,   \
    NGL_NODE_ANIMATEDBUFFERVEC2,    \
    NGL_NODE_ANIMATEDBUFFERVEC3,    \
    NGL_NODE_ANIMATEDBUFFERVEC4,    \
    NGL_NODE_BUFFERBYTE,            \
    NGL_NODE_BUFFERBVEC2,           \
    NGL_NODE_BUFFERBVEC3,           \
    NGL_NODE_BUFFERBVEC4,           \
    NGL_NODE_BUFFERINT,             \
    NGL_NODE_BUFFERIVEC2,           \
    NGL_NODE_BUFFERIVEC3,           \
    NGL_NODE_BUFFERIVEC4,           \
    NGL_NODE_BUFFERSHORT,           \
    NGL_NODE_BUFFERSVEC2,           \
    NGL_NODE_BUFFERSVEC3,           \
    NGL_NODE_BUFFERSVEC4,           \
    NGL_NODE_BUFFERUBYTE,           \
    NGL_NODE_BUFFERUBVEC2,          \
    NGL_NODE_BUFFERUBVEC3,          \
    NGL_NODE_BUFFERUBVEC4,          \
    NGL_NODE_BUFFERUINT,            \
    NGL_NODE_BUFFERUIVEC2,          \
    NGL_NODE_BUFFERUIVEC3,          \
    NGL_NODE_BUFFERUIVEC4,          \
    NGL_NODE_BUFFERUSHORT,          \
    NGL_NODE_BUFFERUSVEC2,          \
    NGL_NODE_BUFFERUSVEC3,          \
    NGL_NODE_BUFFERUSVEC4,          \
    NGL_NODE_BUFFERFLOAT,           \
    NGL_NODE_BUFFERVEC2,            \
    NGL_NODE_BUFFERVEC3,            \
    NGL_NODE_BUFFERVEC4,            \


#define DATA_SRC_TYPES_LIST_2D (const int[]){NGL_NODE_MEDIA,                   \
                                             NGL_NODE_HUD,                     \
                                             BUFFER_NODES                      \
                                             -1}

#define DATA_SRC_TYPES_LIST_3D (const int[]){BUFFER_NODES                      \
                                             -1}


#define OFFSET(x) offsetof(struct texture, x)
static const struct node_param texture2d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(data_format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(min_filter), {.i64=GL_NEAREST}, .choices=&minfilter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(mag_filter), {.i64=GL_NEAREST}, .choices=&magfilter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(wrap_s), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(wrap_t), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_2D,
                 .desc=NGLI_DOCSTRING("data source")},
    {"access", PARAM_TYPE_SELECT, OFFSET(access), {.i64=GL_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"direct_rendering", PARAM_TYPE_BOOL, OFFSET(direct_rendering), {.i64=-1},
                         .desc=NGLI_DOCSTRING("whether direct rendering is enabled or not for media playback")},
    {NULL}
};

static const struct node_param texture3d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(data_format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"depth", PARAM_TYPE_INT, OFFSET(depth), {.i64=0},
               .desc=NGLI_DOCSTRING("depth of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(min_filter), {.i64=GL_NEAREST}, .choices=&minfilter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(mag_filter), {.i64=GL_NEAREST}, .choices=&magfilter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(wrap_s), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(wrap_t), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(wrap_r), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {"access", PARAM_TYPE_SELECT, OFFSET(access), {.i64=GL_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {NULL}
};

int ngli_format_get_gl_format_type(struct glcontext *gl, int data_format,
                                   GLint *formatp, GLint *internal_formatp, GLenum *typep)
{
    static const struct entry {
        GLint format;
        GLint internal_format;
        GLenum type;
    } format_map[] = {
        [NGLI_FORMAT_UNDEFINED]            = {0,                  0,                     0},
        [NGLI_FORMAT_R8_UNORM]             = {GL_RED,             GL_R8,                 GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8_SNORM]             = {GL_RED,             GL_R8_SNORM,           GL_BYTE},
        [NGLI_FORMAT_R8_UINT]              = {GL_RED_INTEGER,     GL_R8UI,               GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8_SINT]              = {GL_RED_INTEGER,     GL_R8I,                GL_BYTE},
        [NGLI_FORMAT_R8G8_UNORM]           = {GL_RG,              GL_RG8,                GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8_SNORM]           = {GL_RG,              GL_RG8_SNORM,          GL_BYTE},
        [NGLI_FORMAT_R8G8_UINT]            = {GL_RG_INTEGER,      GL_RG8UI,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8_SINT]            = {GL_RG_INTEGER,      GL_RG8I,               GL_BYTE},
        [NGLI_FORMAT_R8G8B8_UNORM]         = {GL_RGB,             GL_RGB8,               GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8_SNORM]         = {GL_RGB,             GL_RGB8_SNORM,         GL_BYTE},
        [NGLI_FORMAT_R8G8B8_UINT]          = {GL_RGB_INTEGER,     GL_RGB8UI,             GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8_SINT]          = {GL_RGB_INTEGER,     GL_RGB8I,              GL_BYTE},
        [NGLI_FORMAT_R8G8B8_SRGB]          = {GL_RGB,             GL_SRGB8,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8A8_UNORM]       = {GL_RGBA,            GL_RGBA8,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8A8_SNORM]       = {GL_RGBA,            GL_RGBA8_SNORM,        GL_BYTE},
        [NGLI_FORMAT_R8G8B8A8_UINT]        = {GL_RGBA_INTEGER,    GL_RGBA8UI,            GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8A8_SINT]        = {GL_RGBA_INTEGER,    GL_RGBA8I,             GL_BYTE},
        [NGLI_FORMAT_R8G8B8A8_SRGB]        = {GL_RGBA,            GL_SRGB8_ALPHA8,       GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_B8G8R8A8_UNORM]       = {GL_BGRA,            GL_RGBA8,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_B8G8R8A8_SNORM]       = {GL_BGRA,            GL_RGBA8_SNORM,        GL_BYTE},
        [NGLI_FORMAT_B8G8R8A8_UINT]        = {GL_BGRA_INTEGER,    GL_RGBA8UI,            GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_B8G8R8A8_SINT]        = {GL_BGRA_INTEGER,    GL_RGBA8I,             GL_BYTE},
        [NGLI_FORMAT_R16_UNORM]            = {GL_RED,             GL_R16,                GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16_SNORM]            = {GL_RED,             GL_R16_SNORM,          GL_SHORT},
        [NGLI_FORMAT_R16_UINT]             = {GL_RED_INTEGER,     GL_R16UI,              GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16_SINT]             = {GL_RED_INTEGER,     GL_R16I,               GL_SHORT},
        [NGLI_FORMAT_R16_SFLOAT]           = {GL_RED,             GL_R16F,               GL_HALF_FLOAT},
        [NGLI_FORMAT_R16G16_UNORM]         = {GL_RG,              GL_RG16,               GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16_SNORM]         = {GL_RG,              GL_RG16_SNORM,         GL_SHORT},
        [NGLI_FORMAT_R16G16_UINT]          = {GL_RG_INTEGER,      GL_RG16UI,             GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16_SINT]          = {GL_RG_INTEGER,      GL_RG16I,              GL_SHORT},
        [NGLI_FORMAT_R16G16_SFLOAT]        = {GL_RG,              GL_RG16F,              GL_HALF_FLOAT},
        [NGLI_FORMAT_R16G16B16_UNORM]      = {GL_RGB,             GL_RGB16,              GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16_SNORM]      = {GL_RGB,             GL_RGB16_SNORM,        GL_SHORT},
        [NGLI_FORMAT_R16G16B16_UINT]       = {GL_RGB_INTEGER,     GL_RGB16UI,            GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16_SINT]       = {GL_RGB_INTEGER,     GL_RGB16I,             GL_SHORT},
        [NGLI_FORMAT_R16G16B16_SFLOAT]     = {GL_RGB,             GL_RGB16F,             GL_HALF_FLOAT},
        [NGLI_FORMAT_R16G16B16A16_UNORM]   = {GL_RGBA,            GL_RGBA16,             GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16A16_SNORM]   = {GL_RGBA,            GL_RGBA16_SNORM,       GL_SHORT},
        [NGLI_FORMAT_R16G16B16A16_UINT]    = {GL_RGBA_INTEGER,    GL_RGBA16UI,           GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16A16_SINT]    = {GL_RGBA_INTEGER,    GL_RGBA16I,            GL_SHORT},
        [NGLI_FORMAT_R16G16B16A16_SFLOAT]  = {GL_RGBA,            GL_RGBA16F,            GL_HALF_FLOAT},
        [NGLI_FORMAT_R32_UINT]             = {GL_RED_INTEGER,     GL_R32UI,              GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32_SINT]             = {GL_RED_INTEGER,     GL_R32I,               GL_INT},
        [NGLI_FORMAT_R32_SFLOAT]           = {GL_RED,             GL_R32F,               GL_FLOAT},
        [NGLI_FORMAT_R32G32_UINT]          = {GL_RG_INTEGER,      GL_RG32UI,             GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32G32_SINT]          = {GL_RG_INTEGER,      GL_RG32I,              GL_INT},
        [NGLI_FORMAT_R32G32_SFLOAT]        = {GL_RG,              GL_RG32F,              GL_FLOAT},
        [NGLI_FORMAT_R32G32B32_UINT]       = {GL_RGB_INTEGER,     GL_RGB32UI,            GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32G32B32_SINT]       = {GL_RGB_INTEGER,     GL_RGB32I,             GL_INT},
        [NGLI_FORMAT_R32G32B32_SFLOAT]     = {GL_RGB,             GL_RGB32F,             GL_FLOAT},
        [NGLI_FORMAT_R32G32B32A32_UINT]    = {GL_RGBA_INTEGER,    GL_RGBA32UI,           GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32G32B32A32_SINT]    = {GL_RGBA_INTEGER,    GL_RGBA32I,            GL_INT},
        [NGLI_FORMAT_R32G32B32A32_SFLOAT]  = {GL_RGBA,            GL_RGBA32F,            GL_FLOAT},
        [NGLI_FORMAT_D16_UNORM]            = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16,  GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_X8_D24_UNORM_PACK32]  = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT24,  GL_UNSIGNED_INT},
        [NGLI_FORMAT_D32_SFLOAT]           = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT32F, GL_FLOAT},
        [NGLI_FORMAT_D24_UNORM_S8_UINT]    = {GL_DEPTH_STENCIL,   GL_DEPTH24_STENCIL8,   GL_UNSIGNED_INT_24_8},
        [NGLI_FORMAT_D32_SFLOAT_S8_UINT]   = {GL_DEPTH_STENCIL,   GL_DEPTH32F_STENCIL8,  GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
    };

    ngli_assert(data_format >= 0 && data_format < NGLI_ARRAY_NB(format_map));
    const struct entry *entry = &format_map[data_format];

    ngli_assert(data_format == NGLI_FORMAT_UNDEFINED ||
               (entry->format && entry->internal_format && entry->type));

    GLint format = entry->format;
    GLint internal_format;

    if (gl->backend == NGL_BACKEND_OPENGLES && gl->version < 300) {
        if (format == GL_RED)
            format = GL_LUMINANCE;
        else if (format == GL_RG)
            format = GL_LUMINANCE_ALPHA;
        internal_format = format == GL_BGRA ? GL_RGBA : format;
    } else {
        internal_format = entry->internal_format;
    }

    if (formatp)
        *formatp = format;
    if (internal_formatp)
        *internal_formatp = internal_format;
    if (typep)
        *typep = entry->type;

    return 0;
}

static void tex_image(const struct glcontext *gl, const struct texture *s,
                      const uint8_t *data)
{
    switch (s->target) {
        case GL_TEXTURE_2D:
            if (s->width && s->height)
                ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format,
                                  s->width, s->height, 0,
                                  s->format, s->type, data);
            break;
        case GL_TEXTURE_3D:
            if (s->width && s->height && s->depth)
                ngli_glTexImage3D(gl, GL_TEXTURE_3D, 0, s->internal_format,
                                  s->width, s->height, s->depth, 0,
                                  s->format, s->type, data);
            break;
    }
}

static void tex_sub_image(const struct glcontext *gl, const struct texture *s,
                          const uint8_t *data)
{
    switch (s->target) {
        case GL_TEXTURE_2D:
            if (s->width && s->height)
                ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0,
                                     0, 0, /* x/y offsets */
                                     s->width, s->height,
                                     s->format, s->type, data);
            break;
        case GL_TEXTURE_3D:
            if (s->width && s->height && s->depth)
                ngli_glTexSubImage3D(gl, GL_TEXTURE_3D, 0,
                                     0, 0, 0, /* x/y/z offsets */
                                     s->width, s->height, s->depth,
                                     s->format, s->type, data);
            break;
    }
}

static void tex_storage(const struct glcontext *gl, const struct texture *s)
{
    switch (s->target) {
        case GL_TEXTURE_2D: {
            int mipmap_levels = 1;
            if ((s->width >= 0 && s->height >= 0)           &&
                (s->min_filter == GL_NEAREST_MIPMAP_NEAREST ||
                 s->min_filter == GL_NEAREST_MIPMAP_LINEAR  ||
                 s->min_filter == GL_LINEAR_MIPMAP_NEAREST  ||
                 s->min_filter == GL_LINEAR_MIPMAP_LINEAR)) {
                 while ((s->width | s->height) >> mipmap_levels)
                     mipmap_levels += 1;
            }
            ngli_glTexStorage2D(gl, s->target, mipmap_levels, s->internal_format, s->width, s->height);
            break;
        }
        case GL_TEXTURE_3D:
            ngli_glTexStorage3D(gl, s->target, 1, s->internal_format, s->width, s->height, s->depth);
            break;
    }
}

static void tex_set_params(const struct glcontext *gl, const struct texture *s)
{
    ngli_glTexParameteri(gl, s->target, GL_TEXTURE_MIN_FILTER, s->min_filter);
    ngli_glTexParameteri(gl, s->target, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    ngli_glTexParameteri(gl, s->target, GL_TEXTURE_WRAP_S, s->wrap_s);
    ngli_glTexParameteri(gl, s->target, GL_TEXTURE_WRAP_T, s->wrap_t);
    if (s->target == GL_TEXTURE_3D)
        ngli_glTexParameteri(gl, s->target, GL_TEXTURE_WRAP_R, s->wrap_r);
}

int ngli_texture_update_data(struct ngl_node *node,
                             int width, int height, int depth,
                             const uint8_t *data)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;
    int ret = 0;

    if (!width || !height || (node->class->id == NGL_NODE_TEXTURE3D && !depth))
        return ret;

    int update_dimensions = !s->id || s->width != width || s->height != height || s->depth != depth;

    s->width = width;
    s->height = height;
    s->depth = depth;

    if (s->immutable) {
        if (update_dimensions) {
            ret = 1;

            if (s->id) {
                ngli_glDeleteTextures(gl, 1, &s->id);
            }

            ngli_glGenTextures(gl, 1, &s->id);
            ngli_glBindTexture(gl, s->target, s->id);
            tex_set_params(gl, s);
            tex_storage(gl, s);
        } else {
            ngli_glBindTexture(gl, s->target, s->id);
        }

        if (data) {
            tex_sub_image(gl, s, data);
        }
    } else {
        if (!s->id) {
            ret = 1;

            ngli_glGenTextures(gl, 1, &s->id);
            ngli_glBindTexture(gl, s->target, s->id);
            tex_set_params(gl, s);
        } else {
            ngli_glBindTexture(gl, s->target, s->id);
        }

        if (update_dimensions) {
            tex_image(gl, s, data);
        } else if (data) {
            tex_sub_image(gl, s, data);
        }
    }

    switch (s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glGenerateMipmap(gl, s->target);
        break;
    }

    ngli_glBindTexture(gl, s->target, 0);

    s->layout = NGLI_TEXTURE_LAYOUT_DEFAULT;
    s->planes[0].id = s->id;
    s->planes[0].target = s->target;

    return ret;
}

static int texture_prefetch(struct ngl_node *node, GLenum local_target)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;

    s->target = local_target;
    if (gl->features & NGLI_FEATURE_TEXTURE_STORAGE)
        s->immutable = 1;

    ngli_mat4_identity(s->coordinates_matrix);

    const uint8_t *data = NULL;

    if (s->data_src) {
        switch (s->data_src->class->id) {
        case NGL_NODE_HUD:
            s->data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
            break;
        case NGL_NODE_MEDIA:
            break;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
        case NGL_NODE_BUFFERBYTE:
        case NGL_NODE_BUFFERBVEC2:
        case NGL_NODE_BUFFERBVEC3:
        case NGL_NODE_BUFFERBVEC4:
        case NGL_NODE_BUFFERINT:
        case NGL_NODE_BUFFERIVEC2:
        case NGL_NODE_BUFFERIVEC3:
        case NGL_NODE_BUFFERIVEC4:
        case NGL_NODE_BUFFERSHORT:
        case NGL_NODE_BUFFERSVEC2:
        case NGL_NODE_BUFFERSVEC3:
        case NGL_NODE_BUFFERSVEC4:
        case NGL_NODE_BUFFERUBYTE:
        case NGL_NODE_BUFFERUBVEC2:
        case NGL_NODE_BUFFERUBVEC3:
        case NGL_NODE_BUFFERUBVEC4:
        case NGL_NODE_BUFFERUINT:
        case NGL_NODE_BUFFERUIVEC2:
        case NGL_NODE_BUFFERUIVEC3:
        case NGL_NODE_BUFFERUIVEC4:
        case NGL_NODE_BUFFERUSHORT:
        case NGL_NODE_BUFFERUSVEC2:
        case NGL_NODE_BUFFERUSVEC3:
        case NGL_NODE_BUFFERUSVEC4:
        case NGL_NODE_BUFFERFLOAT:
        case NGL_NODE_BUFFERVEC2:
        case NGL_NODE_BUFFERVEC3:
        case NGL_NODE_BUFFERVEC4: {
            struct buffer *buffer = s->data_src->priv_data;

            if (local_target == GL_TEXTURE_2D) {
                if (buffer->count != s->width * s->height) {
                    LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%d),"
                        " assuming %dx1", s->width, s->height,
                        buffer->count, buffer->count);
                    s->width = buffer->count;
                    s->height = 1;
                }
            } else if (local_target == GL_TEXTURE_3D) {
                if (buffer->count != s->width * s->height * s->depth) {
                    LOG(ERROR, "dimensions (%dx%dx%d) do not match buffer count (%d),"
                        " assuming %dx1x1", s->width, s->height, s->depth,
                        buffer->count, buffer->count);
                    s->width = buffer->count;
                    s->height = s->depth = 1;
                }
            }
            data = buffer->data;
            s->data_format = buffer->data_format;
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    ngli_texture_update_data(node, s->width, s->height, s->depth, data);

    return 0;
}

#define TEXTURE_PREFETCH(dim)                               \
static int texture##dim##d_prefetch(struct ngl_node *node)  \
{                                                           \
    return texture_prefetch(node, GL_TEXTURE_##dim##D);     \
}

TEXTURE_PREFETCH(2)
TEXTURE_PREFETCH(3)

static void handle_hud_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct hud *hud = s->data_src->priv_data;
    const int width = hud->data_w;
    const int height = hud->data_h;
    const uint8_t *data = hud->data_buf;

    ngli_texture_update_data(node, width, height, 0, data);
}

static void handle_media_frame(struct ngl_node *node)
{
    int ret = ngli_hwupload_upload_frame(node);
    if (ret < 0)
        LOG(ERROR, "could not map media frame");
}

static void handle_buffer_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct buffer *buffer = s->data_src->priv_data;
    const uint8_t *data = buffer->data;

    ngli_texture_update_data(node, s->width, s->height, s->depth, data);
}

static int texture_update(struct ngl_node *node, double t)
{
    struct texture *s = node->priv_data;

    if (!s->data_src)
        return 0;

    int ret = ngli_node_update(s->data_src, t);
    if (ret < 0)
        return ret;

    switch (s->data_src->class->id) {
        case NGL_NODE_HUD:
            handle_hud_frame(node);
            break;
        case NGL_NODE_MEDIA:
            handle_media_frame(node);
            break;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
            ret = ngli_node_update(s->data_src, t);
            if (ret < 0)
                return ret;
            handle_buffer_frame(node);
            break;
    }

    return 0;
}

static void texture_release(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;

    ngli_hwupload_uninit(node);

    ngli_glDeleteTextures(gl, 1, &s->id);
    s->id = 0;

    s->layout = NGLI_TEXTURE_LAYOUT_NONE;
    memset(&s->planes, 0, sizeof(s->planes));
}

static int texture3d_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    if (!(gl->features & NGLI_FEATURE_TEXTURE_3D)) {
        LOG(ERROR, "context does not support 3D textures");
        return -1;
    }
    return 0;
}

const struct node_class ngli_texture2d_class = {
    .id        = NGL_NODE_TEXTURE2D,
    .name      = "Texture2D",
    .prefetch  = texture2d_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture),
    .params    = texture2d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texture3d_class = {
    .id        = NGL_NODE_TEXTURE3D,
    .name      = "Texture3D",
    .init      = texture3d_init,
    .prefetch  = texture3d_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture),
    .params    = texture3d_params,
    .file      = __FILE__,
};
