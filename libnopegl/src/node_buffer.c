/*
 * Copyright 2017-2022 GoPro Inc.
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "internal.h"
#include "log.h"
#include "ngpu/buffer.h"
#include "ngpu/type.h"
#include "node_block.h"
#include "node_buffer.h"
#include "nopegl.h"
#include "utils/memory.h"
#include "utils/file.h"
#include "utils/utils.h"

struct buffer_opts {
    uint32_t count;
    uint8_t *data;
    size_t data_size;
    char *filename;
    struct ngl_node *block;
    char *block_field;
};

struct buffer_priv {
    struct buffer_info buf;
    FILE *fp;
};

NGLI_STATIC_ASSERT(buffer_info_is_first, offsetof(struct buffer_priv, buf) == 0);

#define OFFSET(x) offsetof(struct buffer_opts, x)
static const struct node_param buffer_params[] = {
    {"count",  NGLI_PARAM_TYPE_U32,    OFFSET(count),
               .desc=NGLI_DOCSTRING("number of elements")},
    {"data",   NGLI_PARAM_TYPE_DATA,   OFFSET(data),
               .desc=NGLI_DOCSTRING("buffer of `count` elements")},
    {"filename", NGLI_PARAM_TYPE_STR,  OFFSET(filename),
               .flags=NGLI_PARAM_FLAG_FILEPATH,
               .desc=NGLI_DOCSTRING("filename from which the buffer will be read, cannot be used with `data`")},
    {"block",  NGLI_PARAM_TYPE_NODE,    OFFSET(block),
               .node_types=(const uint32_t[]){NGL_NODE_BLOCK, NGLI_NODE_NONE},
               .desc=NGLI_DOCSTRING("reference a field from the given block")},
    {"block_field", NGLI_PARAM_TYPE_STR, OFFSET(block_field),
                    .desc=NGLI_DOCSTRING("field name in `block`")},
    {NULL}
};

void ngli_node_buffer_extend_usage(struct ngl_node *node, uint32_t usage)
{
    struct buffer_info *s = node->priv_data;

    if (s->block) {
        ngli_node_block_extend_usage(s->block, usage);
        return;
    }
    s->usage |= usage;
}

size_t ngli_node_buffer_get_cpu_size(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;
    return s->block ? 0 : s->data_size;
}

size_t ngli_node_buffer_get_gpu_size(struct ngl_node *node)
{
    struct buffer_info *s = node->priv_data;
    return s->block || !(s->flags & NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD) ? 0 : s->data_size;
}

static int buffer_init_from_data(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;
    struct buffer_layout *layout = &s->buf.layout;

    layout->count = layout->count ? layout->count : o->data_size / layout->stride;
    if (o->data_size != layout->count * layout->stride) {
        LOG(ERROR,
            "element count (%zu) and data stride (%zu) does not match data size (%zu)",
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

    if (size > SIZE_MAX) {
        LOG(ERROR, "'%s' size (%" PRId64 ") exceeds supported limit (%zu)", o->filename, size, SIZE_MAX);
        return NGL_ERROR_UNSUPPORTED;
    }

    s->buf.data_size = (size_t)size;
    layout->count = layout->count ? layout->count : s->buf.data_size / layout->stride;

    if (s->buf.data_size != layout->count * layout->stride) {
        LOG(ERROR,
            "element count (%zu) and data stride (%zu) does not match data size (%zu)",
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
        LOG(ERROR, "read %zu bytes does not match expected size of %zu bytes", n, s->buf.data_size);
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

static const struct ngpu_block_field *get_block_field(const struct darray *fields_array, const char *name)
{
    const struct ngpu_block_field *fields = ngli_darray_data(fields_array);
    for (size_t i = 0; i < ngli_darray_count(fields_array); i++) {
        const struct ngpu_block_field *field = &fields[i];
        if (!strcmp(field->name, name))
            return field;
    }
    return NULL;
}

static int buffer_init_from_block(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    struct buffer_layout *layout = &s->buf.layout;
    const struct buffer_opts *o = node->opts;

    const struct block_info *block_info = o->block->priv_data;
    const struct ngpu_block_desc *block = &block_info->block;

    if (!o->block_field) {
        LOG(ERROR, "`block_field` must be set when setting a block");
        return NGL_ERROR_INVALID_USAGE;
    }

    const struct ngpu_block_field *fi = get_block_field(&block->fields, o->block_field);
    if (!fi) {
        LOG(ERROR, "field %s not found in %s", o->block_field, o->block->label);
        return NGL_ERROR_NOT_FOUND;
    }

    if (layout->type != fi->type) {
        LOG(ERROR, "%s.%s of type %s mismatches %s local type",
            o->block->label, o->block_field, ngpu_type_get_name(fi->type), ngpu_type_get_name(layout->type));
        return NGL_ERROR_INVALID_ARG;
    }

    if (layout->count > fi->count) {
        LOG(ERROR, "block buffer reference count can not be larger than target buffer count (%zu > %zu)",
            layout->count, fi->count);
        return NGL_ERROR_INVALID_ARG;
    }
    layout->count = layout->count ? layout->count : fi->count;
    s->buf.data = block_info->data + fi->offset;
    layout->stride = fi->stride;
    layout->offset = fi->offset;
    s->buf.data_size = layout->count * layout->stride;

    return 0;
}

static int buffer_init_from_type(struct ngl_node *node)
{
    const struct buffer_opts *o = node->opts;

    if (o->data)
        return buffer_init_from_data(node);
    if (o->filename)
        return buffer_init_from_filename(node);
    if (o->block)
        return buffer_init_from_block(node);

    return buffer_init_from_count(node);
}

static int buffer_init(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;
    struct buffer_layout *layout = &s->buf.layout;

    layout->count = o->count;
    s->buf.block  = o->block;

    if (o->data && o->filename) {
        LOG(ERROR, "data and filename option cannot be set at the same time");
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
        layout->comp = ngpu_format_get_nb_comp(layout->format);
        layout->stride = ngpu_format_get_bytes_per_pixel(layout->format);
    }

    s->buf.usage = NGPU_BUFFER_USAGE_TRANSFER_DST_BIT;

    int ret = buffer_init_from_type(node);
    if (ret < 0)
        return ret;

    if (s->buf.block) {
        const struct block_info *block_info = s->buf.block->priv_data;
        s->buf.buffer = block_info->buffer;
    } else {
        s->buf.buffer = ngpu_buffer_create(node->ctx->gpu_ctx);
        if (!s->buf.buffer)
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int buffer_prepare(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    struct buffer_info *info = &s->buf;

    if (info->block)
        return ngli_node_prepare(s->buf.block);

    if (!(info->flags & NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD))
        return 0;

    ngli_assert(info->buffer);

    if (info->buffer->size)
        return 0;

    int ret = ngpu_buffer_init(info->buffer, info->data_size, info->usage);
    if (ret < 0)
        return ret;

    ret = ngpu_buffer_upload(info->buffer, info->data, 0, info->data_size);
    if (ret < 0)
        return ret;

    return ngli_node_prepare_children(node);
}

static void buffer_uninit(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct buffer_opts *o = node->opts;

    if (s->buf.block)
        s->buf.buffer = NULL;
    else
        ngpu_buffer_freep(&s->buf.buffer);

    if (!o->data && !o->block)
        ngli_freep(&s->buf.data);

    if (o->filename) {
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
    .prepare   = buffer_prepare,                                \
    .update    = ngli_node_update_children,                     \
    .uninit    = buffer_uninit,                                 \
    .opts_size = sizeof(struct buffer_opts),                    \
    .priv_size = sizeof(struct buffer_priv),                    \
    .params    = buffer_params,                                 \
    .params_id = "Buffer",                                      \
    .file      = __FILE__,                                      \
};

DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBYTE, "BufferByte", byte, NGPU_FORMAT_R8_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC2, "BufferBVec2", bvec2, NGPU_FORMAT_R8G8_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC3, "BufferBVec3", bvec3, NGPU_FORMAT_R8G8B8_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC4, "BufferBVec4", bvec4, NGPU_FORMAT_R8G8B8A8_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT, "BufferInt", int, NGPU_FORMAT_R32_SINT, NGPU_TYPE_I32)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT64, "BufferInt64", int64, NGPU_FORMAT_R64_SINT, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC2, "BufferIVec2", ivec2, NGPU_FORMAT_R32G32_SINT, NGPU_TYPE_IVEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC3, "BufferIVec3", ivec3, NGPU_FORMAT_R32G32B32_SINT, NGPU_TYPE_IVEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC4, "BufferIVec4", ivec4, NGPU_FORMAT_R32G32B32A32_SINT, NGPU_TYPE_IVEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSHORT, "BufferShort", short, NGPU_FORMAT_R16_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC2, "BufferSVec2", svec2, NGPU_FORMAT_R16G16_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC3, "BufferSVec3", svec3, NGPU_FORMAT_R16G16B16_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC4, "BufferSVec4", svec4, NGPU_FORMAT_R16G16B16A16_SNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBYTE, "BufferUByte", ubyte, NGPU_FORMAT_R8_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC2, "BufferUBVec2", ubvec2, NGPU_FORMAT_R8G8_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC3, "BufferUBVec3", ubvec3, NGPU_FORMAT_R8G8B8_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC4, "BufferUBVec4", ubvec4, NGPU_FORMAT_R8G8B8A8_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUINT, "BufferUInt", uint, NGPU_FORMAT_R32_UINT, NGPU_TYPE_U32)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC2, "BufferUIVec2", uivec2, NGPU_FORMAT_R32G32_UINT, NGPU_TYPE_UVEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC3, "BufferUIVec3", uivec3, NGPU_FORMAT_R32G32B32_UINT, NGPU_TYPE_UVEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC4, "BufferUIVec4", uivec4, NGPU_FORMAT_R32G32B32A32_UINT, NGPU_TYPE_UVEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSHORT, "BufferUShort", ushort, NGPU_FORMAT_R16_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC2, "BufferUSVec2", usvec2, NGPU_FORMAT_R16G16_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC3, "BufferUSVec3", usvec3, NGPU_FORMAT_R16G16B16_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC4, "BufferUSVec4", usvec4, NGPU_FORMAT_R16G16B16A16_UNORM, NGPU_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERFLOAT, "BufferFloat", float, NGPU_FORMAT_R32_SFLOAT, NGPU_TYPE_F32)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC2, "BufferVec2", vec2, NGPU_FORMAT_R32G32_SFLOAT, NGPU_TYPE_VEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC3, "BufferVec3", vec3, NGPU_FORMAT_R32G32B32_SFLOAT, NGPU_TYPE_VEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC4, "BufferVec4", vec4, NGPU_FORMAT_R32G32B32A32_SFLOAT, NGPU_TYPE_VEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERMAT4, "BufferMat4", mat4, NGPU_FORMAT_R32G32B32A32_SFLOAT, NGPU_TYPE_MAT4)
