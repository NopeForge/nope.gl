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

#include "format.h"
#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "texture.h"

const struct param_choices ngli_minfilter_choices = {
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

const struct param_choices ngli_magfilter_choices = {
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

#define DECLARE_FORMAT_PARAM(format, size, name, doc) \
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


#define OFFSET(x) offsetof(struct texture_priv, x)
static const struct node_param texture2d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(params.format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(params.height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=GL_NEAREST}, .choices=&ngli_minfilter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=GL_NEAREST}, .choices=&ngli_magfilter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"access", PARAM_TYPE_SELECT, OFFSET(params.access), {.i64=GL_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_2D,
                 .desc=NGLI_DOCSTRING("data source")},
    {"direct_rendering", PARAM_TYPE_BOOL, OFFSET(direct_rendering), {.i64=-1},
                         .desc=NGLI_DOCSTRING("whether direct rendering is enabled or not for media playback")},
    {NULL}
};

static const struct node_param texture3d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(params.format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(params.height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"depth", PARAM_TYPE_INT, OFFSET(params.depth), {.i64=0},
               .desc=NGLI_DOCSTRING("depth of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=GL_NEAREST}, .choices=&ngli_minfilter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=GL_NEAREST}, .choices=&ngli_magfilter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"access", PARAM_TYPE_SELECT, OFFSET(params.access), {.i64=GL_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static const struct node_param texturecube_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(params.format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"size", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
             .desc=NGLI_DOCSTRING("width and height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=GL_NEAREST}, .choices=&ngli_minfilter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=GL_NEAREST}, .choices=&ngli_magfilter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i64=GL_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"access", PARAM_TYPE_SELECT, OFFSET(params.access), {.i64=GL_READ_WRITE}, .choices=&access_choices,
               .desc=NGLI_DOCSTRING("texture access (only honored by the `Compute` node)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static int texture_prefetch(struct ngl_node *node, int dimensions, int cubemap)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture_priv *s = node->priv_data;
    struct texture_params *params = &s->params;

    params->dimensions = dimensions;
    if (cubemap) {
        params->height = params->width;
        params->depth = 6;
        params->cubemap = 1;
    }

    if (gl->features & NGLI_FEATURE_TEXTURE_STORAGE)
        params->immutable = 1;

    const uint8_t *data = NULL;

    if (s->data_src) {
        switch (s->data_src->class->id) {
        case NGL_NODE_HUD: {
            struct texture_priv *s = node->priv_data;
            struct hud_priv *hud = s->data_src->priv_data;
            params->format = NGLI_FORMAT_R8G8B8A8_UNORM;
            params->width = hud->canvas.w;
            params->height = hud->canvas.h;
            break;
        }
        case NGL_NODE_MEDIA:
            return 0;
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
            struct buffer_priv *buffer = s->data_src->priv_data;

            if (params->dimensions == 2) {
                if (buffer->count != params->width * params->height) {
                    LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%d),"
                        " assuming %dx1", params->width, params->height,
                        buffer->count, buffer->count);
                    params->width = buffer->count;
                    params->height = 1;
                }
            } else if (params->dimensions == 3) {
                if (buffer->count != params->width * params->height * params->depth) {
                    LOG(ERROR, "dimensions (%dx%dx%d) do not match buffer count (%d),"
                        " assuming %dx1x1", params->width, params->height, params->depth,
                        buffer->count, buffer->count);
                    params->width = buffer->count;
                    params->height = params->depth = 1;
                }
            }
            data = buffer->data;
            params->format = buffer->data_format;
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    int ret = ngli_texture_init(&s->texture, gl, params);
    if (ret < 0)
        return ret;

    ret = ngli_texture_upload(&s->texture, data, 0);
    if (ret < 0)
        return ret;

    ngli_image_init(&s->image, NGLI_IMAGE_LAYOUT_DEFAULT, &s->texture);

    return 0;
}

#define TEXTURE_PREFETCH(name, dim, cubemap)                \
static int texture##name##_prefetch(struct ngl_node *node)  \
{                                                           \
    return texture_prefetch(node, dim, cubemap);            \
}

TEXTURE_PREFETCH(2d,   2, 0)
TEXTURE_PREFETCH(3d,   3, 0)
TEXTURE_PREFETCH(cube, 3, 1)

static void handle_hud_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct texture_params *params = &s->params;
    struct hud_priv *hud = s->data_src->priv_data;
    const uint8_t *data = hud->canvas.buf;

    params->width = hud->canvas.w;
    params->height = hud->canvas.h;

    ngli_texture_upload(&s->texture, data, 0);
}

static void handle_media_frame(struct ngl_node *node)
{
    int ret = ngli_hwupload_upload_frame(node);
    if (ret < 0)
        LOG(ERROR, "could not map media frame");
}

static void handle_buffer_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct buffer_priv *buffer = s->data_src->priv_data;
    const uint8_t *data = buffer->data;
    struct texture *t = &s->texture;

    ngli_texture_upload(t, data, 0);
}

static int texture_update(struct ngl_node *node, double t)
{
    struct texture_priv *s = node->priv_data;

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
    struct texture_priv *s = node->priv_data;

    ngli_hwupload_uninit(node);
    ngli_texture_reset(&s->texture);
    ngli_image_reset(&s->image);
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

static int texturecube_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    if (!(gl->features & NGLI_FEATURE_TEXTURE_CUBE_MAP)) {
        LOG(ERROR, "context does not support cube map textures");
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
    .priv_size = sizeof(struct texture_priv),
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
    .priv_size = sizeof(struct texture_priv),
    .params    = texture3d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texturecube_class = {
    .id        = NGL_NODE_TEXTURECUBE,
    .name      = "TextureCube",
    .init      = texturecube_init,
    .prefetch  = texturecube_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texturecube_params,
    .file      = __FILE__,
};
