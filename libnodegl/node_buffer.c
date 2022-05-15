/*
 * Copyright 2017 GoPro Inc.
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

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"
#include "type.h"
#include "utils.h"

struct buffer_opts {
    int count;
    uint8_t *data;
    int data_size;
    char *filename;
    struct ngl_node *block;
    int block_field;
};

struct buffer_priv {
    struct buffer_info buf;
    FILE *fp;
};

NGLI_STATIC_ASSERT(buffer_info_is_first, offsetof(struct buffer_priv, buf) == 0);

#define OFFSET(x) offsetof(struct buffer_opts, x)
static const struct node_param buffer_params[] = {
    {"count",  NGLI_PARAM_TYPE_I32,    OFFSET(count),
               .desc=NGLI_DOCSTRING("number of elements")},
    {"data",   NGLI_PARAM_TYPE_DATA,   OFFSET(data),
               .desc=NGLI_DOCSTRING("buffer of `count` elements")},
    {"filename", NGLI_PARAM_TYPE_STR,  OFFSET(filename),
               .desc=NGLI_DOCSTRING("filename from which the buffer will be read, cannot be used with `data`")},
    {"block",  NGLI_PARAM_TYPE_NODE,    OFFSET(block),
               .node_types=(const int[]){NGL_NODE_BLOCK, -1},
               .desc=NGLI_DOCSTRING("reference a field from the given block")},
    {"block_field", NGLI_PARAM_TYPE_I32, OFFSET(block_field),
                    .desc=NGLI_DOCSTRING("field index in `block`")},
    {NULL}
};

int ngli_node_buffer_ref(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct buffer_info *s = node->priv_data;

    if (s->block)
        return ngli_node_block_ref(s->block);

    if (s->buffer_refcount++ == 0) {
        s->buffer = ngli_buffer_create(gpu_ctx);
        if (!s->buffer)
            return NGL_ERROR_MEMORY;
        s->buffer_last_upload_time = -1.;
    }

    return 0;
}

int ngli_node_buffer_init(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;

    if (s->block)
        return ngli_node_block_init(s->block);

    ngli_assert(s->buffer);

    if (s->buffer->size)
        return 0;

    int ret = ngli_buffer_init(s->buffer, s->data_size, s->usage);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(s->buffer, s->data, s->data_size, 0);
    if (ret < 0)
        return ret;

    return 0;
}

void ngli_node_buffer_unref(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;

    if (s->block) {
        ngli_node_block_unref(s->block);
        return;
    }

    ngli_assert(s->buffer_refcount);
    if (s->buffer_refcount-- == 1)
        ngli_buffer_freep(&s->buffer);
}

int ngli_node_buffer_upload(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;

    if (s->block)
        return ngli_node_block_upload(s->block);

    if (s->dynamic && s->buffer_last_upload_time != node->last_update_time) {
        int ret = ngli_buffer_upload(s->buffer, s->data, s->data_size, 0);
        if (ret < 0)
            return ret;
        s->buffer_last_upload_time = node->last_update_time;
    }

    return 0;
}

int ngli_node_buffer_get_cpu_size(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;
    return s->block ? 0 : s->data_size;
}

int ngli_node_buffer_get_gpu_size(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;
    return s->block ? 0 : s->data_size * (s->buffer_refcount > 0);
}

static int buffer_init_from_data(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;
    struct buffer_layout *layout = &s->buf.layout;

    layout->count = layout->count ? layout->count : o->data_size / layout->stride;
    if (o->data_size != layout->count * layout->stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            layout->count,
            layout->stride,
            o->data_size);
        return NGL_ERROR_INVALID_ARG;
    }

    s->buf.data      = o->data;
    s->buf.data_size = o->data_size;
    return 0;
}

static int buffer_init_from_filename(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;
    struct buffer_layout *layout = &s->buf.layout;

    int64_t size;
    int ret = ngli_get_filesize(o->filename, &size);
    if (ret < 0)
        return ret;

    if (size > INT_MAX) {
        LOG(ERROR, "'%s' size (%" PRId64 ") exceeds supported limit (%d)", o->filename, size, INT_MAX);
        return NGL_ERROR_UNSUPPORTED;
    }

    s->buf.data_size = size;
    layout->count = layout->count ? layout->count : s->buf.data_size / layout->stride;

    if (s->buf.data_size != layout->count * layout->stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            layout->count,
            layout->stride,
            s->buf.data_size);
        return NGL_ERROR_INVALID_DATA;
    }

    s->buf.data = ngli_calloc(layout->count, layout->stride);
    if (!s->buf.data)
        return NGL_ERROR_MEMORY;

    s->fp = fopen(o->filename, "rb");
    if (!s->fp) {
        LOG(ERROR, "could not open '%s'", o->filename);
        return NGL_ERROR_IO;
    }

    size_t n = fread(s->buf.data, 1, s->buf.data_size, s->fp);
    if (n != s->buf.data_size) {
        LOG(ERROR, "could not read '%s'", o->filename);
        return NGL_ERROR_IO;
    }

    if (n != s->buf.data_size) {
        LOG(ERROR, "read %zd bytes does not match expected size of %d bytes", n, s->buf.data_size);
        return NGL_ERROR_IO;
    }

    return 0;
}

static int buffer_init_from_count(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    struct buffer_layout *layout = &s->buf.layout;

    layout->count = layout->count ? layout->count : 1;
    s->buf.data_size = layout->count * layout->stride;
    s->buf.data = ngli_calloc(layout->count, layout->stride);
    if (!s->buf.data)
        return NGL_ERROR_MEMORY;

    return 0;
}

static int buffer_init_from_block(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    struct buffer_layout *layout = &s->buf.layout;
    const struct buffer_opts *o = node->opts;

    const struct block_priv *block_priv = o->block->priv_data;
    const struct block *block = &block_priv->block;
    const struct block_field *fields = ngli_darray_data(&block->fields);
    const int nb_fields = ngli_darray_count(&block->fields);

    if (o->block_field < 0 || o->block_field >= nb_fields) {
        LOG(ERROR, "invalid field id %d; %s has %d fields",
            o->block_field, o->block->label, nb_fields);
        return NGL_ERROR_INVALID_ARG;
    }

    const struct block_field *fi = &fields[o->block_field];
    if (layout->type != fi->type) {
        LOG(ERROR, "%s[%d] of type %s mismatches %s local type",
            o->block->label, o->block_field, ngli_type_get_name(fi->type), ngli_type_get_name(layout->type));
        return NGL_ERROR_INVALID_ARG;
    }

    if (layout->count > fi->count) {
        LOG(ERROR, "block buffer reference count can not be larger than target buffer count (%d > %d)",
            layout->count, fi->count);
        return NGL_ERROR_INVALID_ARG;
    }
    layout->count = layout->count ? layout->count : fi->count;
    s->buf.data = block_priv->data + fi->offset;
    layout->stride = fi->stride;
    s->buf.data_size = layout->count * layout->stride;

    return 0;
}

static int buffer_init(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;
    struct buffer_layout *layout = &s->buf.layout;

    layout->count      = o->count;
    s->buf.block       = o->block;
    s->buf.block_field = o->block_field;

    if (o->data && o->filename) {
        LOG(ERROR,
            "data and filename option cannot be set at the same time");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->block && (o->data || o->filename)) {
        LOG(ERROR, "block option can not be set with data or filename");
        return NGL_ERROR_INVALID_ARG;
    }

    if (node->cls->id == NGL_NODE_BUFFERMAT4) {
        layout->comp = 4 * 4;
        layout->stride = layout->comp * sizeof(float);
    } else {
        layout->comp = ngli_format_get_nb_comp(layout->format);
        layout->stride = ngli_format_get_bytes_per_pixel(layout->format);
    }

    s->buf.usage = NGLI_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (o->data)
        return buffer_init_from_data(node);
    if (o->filename)
        return buffer_init_from_filename(node);
    if (o->block)
        return buffer_init_from_block(node);

    return buffer_init_from_count(node);
}

static void buffer_uninit(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;

    if (o->filename) {
        ngli_freep(&s->buf.data);
        s->buf.data_size = 0;

        if (s->fp) {
            int ret = fclose(s->fp);
            if (ret < 0) {
                LOG(ERROR, "could not properly close '%s'", o->filename);
            }
        }
    }
}

#define DEFINE_BUFFER_CLASS(class_id, class_name, type_name, dformat, dtype) \
static int buffer##type_name##_init(struct ngl_node *node)      \
{                                                               \
    struct buffer_priv *s = node->priv_data;                    \
    s->buf.layout.format = dformat;                             \
    s->buf.layout.type = dtype;                                 \
    return buffer_init(node);                                   \
}                                                               \
                                                                \
const struct node_class ngli_buffer##type_name##_class = {      \
    .id        = class_id,                                      \
    .category  = NGLI_NODE_CATEGORY_BUFFER,                     \
    .name      = class_name,                                    \
    .init      = buffer##type_name##_init,                      \
    .uninit    = buffer_uninit,                                 \
    .opts_size = sizeof(struct buffer_opts),                    \
    .priv_size = sizeof(struct buffer_priv),                    \
    .params    = buffer_params,                                 \
    .params_id = "Buffer",                                      \
    .file      = __FILE__,                                      \
};

DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBYTE,   "BufferByte",   byte,   NGLI_FORMAT_R8_SNORM,            NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC2,  "BufferBVec2",  bvec2,  NGLI_FORMAT_R8G8_SNORM,          NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC3,  "BufferBVec3",  bvec3,  NGLI_FORMAT_R8G8B8_SNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC4,  "BufferBVec4",  bvec4,  NGLI_FORMAT_R8G8B8A8_SNORM,      NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT,    "BufferInt",    int,    NGLI_FORMAT_R32_SINT,            NGLI_TYPE_INT)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT64,  "BufferInt64",  int64,  NGLI_FORMAT_R64_SINT,            NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC2,  "BufferIVec2",  ivec2,  NGLI_FORMAT_R32G32_SINT,         NGLI_TYPE_IVEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC3,  "BufferIVec3",  ivec3,  NGLI_FORMAT_R32G32B32_SINT,      NGLI_TYPE_IVEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC4,  "BufferIVec4",  ivec4,  NGLI_FORMAT_R32G32B32A32_SINT,   NGLI_TYPE_IVEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSHORT,  "BufferShort",  short,  NGLI_FORMAT_R16_SNORM,           NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC2,  "BufferSVec2",  svec2,  NGLI_FORMAT_R16G16_SNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC3,  "BufferSVec3",  svec3,  NGLI_FORMAT_R16G16B16_SNORM,     NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC4,  "BufferSVec4",  svec4,  NGLI_FORMAT_R16G16B16A16_SNORM,  NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBYTE,  "BufferUByte",  ubyte,  NGLI_FORMAT_R8_UNORM,            NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC2, "BufferUBVec2", ubvec2, NGLI_FORMAT_R8G8_UNORM,          NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC3, "BufferUBVec3", ubvec3, NGLI_FORMAT_R8G8B8_UNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC4, "BufferUBVec4", ubvec4, NGLI_FORMAT_R8G8B8A8_UNORM,      NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUINT,   "BufferUInt",   uint,   NGLI_FORMAT_R32_UINT,            NGLI_TYPE_UINT)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC2, "BufferUIVec2", uivec2, NGLI_FORMAT_R32G32_UINT,         NGLI_TYPE_UIVEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC3, "BufferUIVec3", uivec3, NGLI_FORMAT_R32G32B32_UINT,      NGLI_TYPE_UIVEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC4, "BufferUIVec4", uivec4, NGLI_FORMAT_R32G32B32A32_UINT,   NGLI_TYPE_UIVEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSHORT, "BufferUShort", ushort, NGLI_FORMAT_R16_UNORM,           NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC2, "BufferUSVec2", usvec2, NGLI_FORMAT_R16G16_UNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC3, "BufferUSVec3", usvec3, NGLI_FORMAT_R16G16B16_UNORM,     NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC4, "BufferUSVec4", usvec4, NGLI_FORMAT_R16G16B16A16_UNORM,  NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERFLOAT,  "BufferFloat",  float,  NGLI_FORMAT_R32_SFLOAT,          NGLI_TYPE_FLOAT)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC2,   "BufferVec2",   vec2,   NGLI_FORMAT_R32G32_SFLOAT,       NGLI_TYPE_VEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC3,   "BufferVec3",   vec3,   NGLI_FORMAT_R32G32B32_SFLOAT,    NGLI_TYPE_VEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC4,   "BufferVec4",   vec4,   NGLI_FORMAT_R32G32B32A32_SFLOAT, NGLI_TYPE_VEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERMAT4,   "BufferMat4",   mat4,   NGLI_FORMAT_R32G32B32A32_SFLOAT, NGLI_TYPE_MAT4)
