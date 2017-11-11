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

#define DATA_SRC_TYPES_LIST (const int[]){NGL_NODE_MEDIA,                   \
                                          NGL_NODE_FPS,                     \
                                          NGL_NODE_ANIMATEDBUFFERFLOAT,     \
                                          NGL_NODE_ANIMATEDBUFFERVEC2,      \
                                          NGL_NODE_ANIMATEDBUFFERVEC3,      \
                                          NGL_NODE_ANIMATEDBUFFERVEC4,      \
                                          NGL_NODE_BUFFERBYTE,              \
                                          NGL_NODE_BUFFERBVEC2,             \
                                          NGL_NODE_BUFFERBVEC3,             \
                                          NGL_NODE_BUFFERBVEC4,             \
                                          NGL_NODE_BUFFERINT,               \
                                          NGL_NODE_BUFFERIVEC2,             \
                                          NGL_NODE_BUFFERIVEC3,             \
                                          NGL_NODE_BUFFERIVEC4,             \
                                          NGL_NODE_BUFFERSHORT,             \
                                          NGL_NODE_BUFFERSVEC2,             \
                                          NGL_NODE_BUFFERSVEC3,             \
                                          NGL_NODE_BUFFERSVEC4,             \
                                          NGL_NODE_BUFFERUBYTE,             \
                                          NGL_NODE_BUFFERUBVEC2,            \
                                          NGL_NODE_BUFFERUBVEC3,            \
                                          NGL_NODE_BUFFERUBVEC4,            \
                                          NGL_NODE_BUFFERUINT,              \
                                          NGL_NODE_BUFFERUIVEC2,            \
                                          NGL_NODE_BUFFERUIVEC3,            \
                                          NGL_NODE_BUFFERUIVEC4,            \
                                          NGL_NODE_BUFFERUSHORT,            \
                                          NGL_NODE_BUFFERUSVEC2,            \
                                          NGL_NODE_BUFFERUSVEC3,            \
                                          NGL_NODE_BUFFERUSVEC4,            \
                                          NGL_NODE_BUFFERFLOAT,             \
                                          NGL_NODE_BUFFERVEC2,              \
                                          NGL_NODE_BUFFERVEC3,              \
                                          NGL_NODE_BUFFERVEC4,              \
                                          -1}

#define OFFSET(x) offsetof(struct texture, x)
static const struct node_param texture2d_params[] = {
    {"format", PARAM_TYPE_INT, OFFSET(format), {.i64=GL_RGBA}},
    {"internal_format", PARAM_TYPE_INT, OFFSET(internal_format), {.i64=GL_RGBA}},
    {"type", PARAM_TYPE_INT, OFFSET(type), {.i64=GL_UNSIGNED_BYTE}},
    {"width", PARAM_TYPE_INT, OFFSET(width), {.i64=0}},
    {"height", PARAM_TYPE_INT, OFFSET(height), {.i64=0}},
    {"min_filter", PARAM_TYPE_INT, OFFSET(min_filter), {.i64=GL_NEAREST}},
    {"mag_filter", PARAM_TYPE_INT, OFFSET(mag_filter), {.i64=GL_NEAREST}},
    {"wrap_s", PARAM_TYPE_INT, OFFSET(wrap_s), {.i64=GL_CLAMP_TO_EDGE}},
    {"wrap_t", PARAM_TYPE_INT, OFFSET(wrap_t), {.i64=GL_CLAMP_TO_EDGE}},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST},
    {"access", PARAM_TYPE_INT, OFFSET(access), {.i64=GL_READ_WRITE}},
    {"direct_rendering", PARAM_TYPE_INT, OFFSET(direct_rendering), {.i64=-1}},
    {"immutable", PARAM_TYPE_INT, OFFSET(immutable), {.i64=0}},
    {NULL}
};

GLenum ngli_texture_get_sized_internal_format(struct glcontext *glcontext, GLenum internal_format, GLenum type)
{
    if (glcontext->es && glcontext->major_version == 2)
        return internal_format;

    GLenum format = 0;
    switch (internal_format) {
    case GL_RED:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_R8;
        else if (type == GL_BYTE)           format = GL_R8_SNORM;
        else if (type == GL_HALF_FLOAT)     format = GL_R16F;
        else if (type == GL_FLOAT)          format = GL_R32F;
        break;
    case GL_RED_INTEGER:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_R8UI;
        else if (type == GL_UNSIGNED_SHORT) format = GL_R16UI;
        else if (type == GL_UNSIGNED_INT)   format = GL_R32UI;
        else if (type == GL_BYTE)           format = GL_R8I;
        else if (type == GL_SHORT)          format = GL_R16I;
        else if (type == GL_INT)            format = GL_R32I;
        break;
    case GL_RG:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_RG8;
        else if (type == GL_BYTE)           format = GL_RG8_SNORM;
        else if (type == GL_HALF_FLOAT)     format = GL_RG16F;
        else if (type == GL_FLOAT)          format = GL_RG32F;
        break;
    case GL_RG_INTEGER:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_RG8UI;
        else if (type == GL_UNSIGNED_SHORT) format = GL_RG16UI;
        else if (type == GL_UNSIGNED_INT)   format = GL_RG32UI;
        else if (type == GL_BYTE)           format = GL_RG8I;
        else if (type == GL_SHORT)          format = GL_RG16I;
        else if (type == GL_INT)            format = GL_RG32I;
        break;
    case GL_RGB:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_RGB8;
        else if (type == GL_BYTE)           format = GL_RGB8_SNORM;
        else if (type == GL_HALF_FLOAT)     format = GL_RGB16F;
        else if (type == GL_FLOAT)          format = GL_RGB32F;
        break;
    case GL_RGB_INTEGER:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_RGB8UI;
        else if (type == GL_UNSIGNED_SHORT) format = GL_RGB16UI;
        else if (type == GL_UNSIGNED_INT)   format = GL_RGB32UI;
        else if (type == GL_BYTE)           format = GL_RGB8I;
        else if (type == GL_SHORT)          format = GL_RGB16I;
        else if (type == GL_INT)            format = GL_RGB32I;
        break;
    case GL_RGBA:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_RGBA8;
        else if (type == GL_BYTE)           format = GL_RGBA8_SNORM;
        else if (type == GL_HALF_FLOAT)     format = GL_RGBA16F;
        else if (type == GL_FLOAT)          format = GL_RGBA32F;
        break;
    case GL_RGBA_INTEGER:
        if      (type == GL_UNSIGNED_BYTE)  format = GL_RGBA8UI;
        else if (type == GL_UNSIGNED_SHORT) format = GL_RGBA16UI;
        else if (type == GL_UNSIGNED_INT)   format = GL_RGBA32UI;
        else if (type == GL_BYTE)           format = GL_RGBA8I;
        else if (type == GL_SHORT)          format = GL_RGBA16I;
        else if (type == GL_INT)            format = GL_RGBA32I;
        break;
    case GL_DEPTH_COMPONENT:
        if      (type == GL_UNSIGNED_SHORT) format = GL_DEPTH_COMPONENT16;
        else if (type == GL_UNSIGNED_INT)   format = GL_DEPTH_COMPONENT24;
        else if (type == GL_FLOAT)          format = GL_DEPTH_COMPONENT32F;
        break;
    case GL_DEPTH_STENCIL:
        if      (type == GL_UNSIGNED_INT_24_8)               format = GL_DEPTH24_STENCIL8;
        else if (type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV)  format = GL_DEPTH32F_STENCIL8;
        break;
    }

    if (!format) {
        LOG(WARNING,
            "could not deduce sized internal format from format (0x%x) and type (0x%x)",
            internal_format,
            type);
        format = internal_format;
    }

    return format;
}

int ngli_texture_update_local_texture(struct ngl_node *node, int width, int height, const uint8_t *data)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;
    int ret = 0;

    if (!width || !height)
        return ret;

    int update_dimensions = !s->local_id || s->width != width || s->height != height;

    s->width = width;
    s->height = height;

    if (s->immutable) {
        if (update_dimensions) {
            ret = 1;

            if (s->local_id) {
                ngli_glDeleteTextures(gl, 1, &s->local_id);
            }

            s->local_target = GL_TEXTURE_2D;
            ngli_glGenTextures(gl, 1, &s->local_id);
            ngli_glBindTexture(gl, s->local_target, s->local_id);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_MIN_FILTER, s->min_filter);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_MAG_FILTER, s->mag_filter);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_WRAP_S, s->wrap_s);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_WRAP_T, s->wrap_t);

            GLenum format = ngli_texture_get_sized_internal_format(glcontext,
                                                                   s->internal_format,
                                                                   s->type);
            ngli_glTexStorage2D(gl, s->local_target, 1, format, s->width, s->height);
        } else {
            ngli_glBindTexture(gl, s->local_target, s->local_id);
        }

        if (data) {
            ngli_glTexSubImage2D(gl,
                                 s->local_target,
                                 0,
                                 0,
                                 0,
                                 s->width,
                                 s->height,
                                 s->format,
                                 s->type,
                                 data);
        }
    } else {
        if (!s->local_id) {
            ret = 1;

            s->local_target = GL_TEXTURE_2D;
            ngli_glGenTextures(gl, 1, &s->local_id);
            ngli_glBindTexture(gl, s->local_target, s->local_id);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_MIN_FILTER, s->min_filter);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_MAG_FILTER, s->mag_filter);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_WRAP_S, s->wrap_s);
            ngli_glTexParameteri(gl, s->local_target, GL_TEXTURE_WRAP_T, s->wrap_t);
        }

        ngli_glBindTexture(gl, s->local_target, s->local_id);
        if (update_dimensions) {
            ngli_glTexImage2D(gl, s->local_target,
                              0,
                              s->internal_format,
                              s->width,
                              s->height,
                              0,
                              s->format,
                              s->type,
                              data);
        } else {
            if (data) {
                ngli_glTexSubImage2D(gl,
                                     s->local_target,
                                     0,
                                     0,
                                     0,
                                     s->width,
                                     s->height,
                                     s->format,
                                     s->type,
                                     data);
            }
        }
    }

    switch (s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glGenerateMipmap(gl, s->local_target);
        break;
    }

    ngli_glBindTexture(gl, s->local_target, 0);

    s->id = s->local_id;
    s->target = s->local_target;

    return ret;
}

static int texture2d_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    struct texture *s = node->priv_data;

    if (!glcontext->has_texture_storage_compatibility && s->immutable) {
        LOG(ERROR, "context does not support texture storage");
        return -1;
    }

    s->target = GL_TEXTURE_2D;

    ngli_mat4_identity(s->coordinates_matrix);

    if (s->external_id) {
        s->id = s->external_id;
        s->target = s->external_target;
    }

    if (s->id)
        return 0;

    const uint8_t *data = NULL;

    if (s->data_src) {
        int ret = ngli_node_init(s->data_src);
        if (ret < 0)
            return ret;

        switch (s->data_src->class->id) {
        case NGL_NODE_FPS:
            s->format = glcontext->gl_1comp;
            s->internal_format = ngli_texture_get_sized_internal_format(glcontext,
                                                                        glcontext->gl_1comp,
                                                                        GL_UNSIGNED_BYTE);
            s->type = GL_UNSIGNED_BYTE;
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
            if (buffer->count != s->width * s->height) {
                LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%d),"
                    " assuming %dx1", s->width, s->height,
                    buffer->count, buffer->count);
                s->width = buffer->count;
                s->height = 1;
            }
            data = buffer->data;
            s->type = buffer->comp_type;
            switch (buffer->data_comp) {
            case 1: s->internal_format = s->format = GL_RED;  break;
            case 2: s->internal_format = s->format = GL_RG;   break;
            case 3: s->internal_format = s->format = GL_RGB;  break;
            case 4: s->internal_format = s->format = GL_RGBA; break;
            default: ngli_assert(0);
            }
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    ngli_texture_update_local_texture(node, s->width, s->height, data);

    return 0;
}

static void handle_fps_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct fps *fps = s->data_src->priv_data;
    const int width = fps->data_w;
    const int height = fps->data_h;
    const uint8_t *data = fps->data_buf;

    ngli_texture_update_local_texture(node, width, height, data);
}

static void handle_media_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;

    if (media->frame) {
        ngli_hwupload_upload_frame(node, media->frame);

        sxplayer_release_frame(media->frame);
        media->frame = NULL;
    }
}

static void handle_buffer_frame(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct buffer *buffer = s->data_src->priv_data;
    const uint8_t *data = buffer->data;

    ngli_texture_update_local_texture(node, s->width, s->height, data);
}

static int texture2d_update(struct ngl_node *node, double t)
{
    struct texture *s = node->priv_data;

    if (!s->data_src)
        return 0;

    int ret = ngli_node_update(s->data_src, t);
    if (ret < 0)
        return ret;

    switch (s->data_src->class->id) {
        case NGL_NODE_FPS:
            handle_fps_frame(node);
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

static void texture2d_release(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    ngli_hwupload_uninit(node);

    ngli_glDeleteTextures(gl, 1, &s->local_id);
    s->id = s->local_id = 0;
}

const struct node_class ngli_texture2d_class = {
    .id        = NGL_NODE_TEXTURE2D,
    .name      = "Texture2D",
    .prefetch  = texture2d_prefetch,
    .update    = texture2d_update,
    .release   = texture2d_release,
    .priv_size = sizeof(struct texture),
    .params    = texture2d_params,
};
