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

#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct buffer_priv, x)
static const struct node_param buffer_params[] = {
    {"count",  PARAM_TYPE_INT,    OFFSET(count),
               .desc=NGLI_DOCSTRING("number of elements")},
    {"data",   PARAM_TYPE_DATA,   OFFSET(data),
               .desc=NGLI_DOCSTRING("buffer of `count` elements")},
    {"filename", PARAM_TYPE_STR,  OFFSET(filename),
               .desc=NGLI_DOCSTRING("filename from which the buffer will be read, cannot be used with `data`")},
    {"stride", PARAM_TYPE_INT,    OFFSET(data_stride),
               .desc=NGLI_DOCSTRING("stride of 1 element, in bytes")},
    {"block",  PARAM_TYPE_NODE,    OFFSET(block),
               .node_types=(const int[]){NGL_NODE_BLOCK, -1},
               .desc=NGLI_DOCSTRING("reference a field from the given block")},
    {"block_field", PARAM_TYPE_INT, OFFSET(block_field),
                    .desc=NGLI_DOCSTRING("field index in `block`")},
    {NULL}
};

int ngli_node_buffer_ref(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct buffer_priv *s = node->priv_data;

    if (s->block)
        return ngli_node_block_ref(s->block);

    if (s->buffer_refcount++ == 0) {
        int ret = ngli_buffer_allocate(&s->buffer, gl, s->data_size, s->usage);
        if (ret < 0)
            return ret;

        ret = ngli_buffer_upload(&s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;

        s->buffer_last_upload_time = -1.;
    }

    return 0;
}

void ngli_node_buffer_unref(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->block)
        return ngli_node_block_unref(s->block);

    ngli_assert(s->buffer_refcount);
    if (s->buffer_refcount-- == 1)
        ngli_buffer_free(&s->buffer);
}

int ngli_node_buffer_upload(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->block)
        return ngli_node_block_upload(s->block);

    if (s->dynamic && s->buffer_last_upload_time != node->last_update_time) {
        int ret = ngli_buffer_upload(&s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;
        s->buffer_last_upload_time = node->last_update_time;
    }

    return 0;
}

static int buffer_init_from_data(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->count = s->count ? s->count : s->data_size / s->data_stride;
    if (s->data_size != s->count * s->data_stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            s->count,
            s->data_stride,
            s->data_size);
        return -1;
    }

    return 0;
}

static int buffer_init_from_filename(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->fd = open(s->filename, O_RDONLY);
    if (s->fd < 0) {
        LOG(ERROR, "could not open '%s'", s->filename);
        return -1;
    }

    off_t filesize = lseek(s->fd, 0, SEEK_END);
    off_t ret      = lseek(s->fd, 0, SEEK_SET);
    if (filesize < 0 || ret < 0) {
        LOG(ERROR, "could not seek in '%s'", s->filename);
        return -1;
    }
    s->data_size = filesize;
    s->count = s->count ? s->count : s->data_size / s->data_stride;

    if (s->data_size != s->count * s->data_stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            s->count,
            s->data_stride,
            s->data_size);
        return -1;
    }

    s->data = ngli_calloc(s->count, s->data_stride);
    if (!s->data)
        return -1;

    ssize_t n = read(s->fd, s->data, s->data_size);
    if (n < 0) {
        LOG(ERROR, "could not read '%s': %zd", s->filename, n);
        return -1;
    }

    if (n != s->data_size) {
        LOG(ERROR, "read %zd bytes does not match expected size of %d bytes", n, s->data_size);
        return -1;
    }

    return 0;
}

static int buffer_init_from_count(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->count = s->count ? s->count : 1;
    s->data_size = s->count * s->data_stride;
    s->data = ngli_calloc(s->count, s->data_stride);
    if (!s->data)
        return -1;

    return 0;
}

static int buffer_init_from_block(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    struct block_priv *block = s->block->priv_data;
    if (s->block_field < 0 || s->block_field >= block->nb_fields) {
        LOG(ERROR, "invalid field id %d; %s has %d fields",
            s->block_field, s->block->label, block->nb_fields);
        return -1;
    }

    struct ngl_node *buffer_target = block->fields[s->block_field];
    if (buffer_target->class->id != node->class->id) {
        LOG(ERROR, "%s[%d] of type %s mismatches %s local type",
            s->block->label, s->block_field, buffer_target->class->name, node->class->name);
        return -1;
    }

    struct buffer_priv *buffer_target_priv = buffer_target->priv_data;
    if (s->count > buffer_target_priv->count) {
        LOG(ERROR, "block buffer reference count can not be larger than target buffer count (%d > %d)",
            s->count, buffer_target_priv->count);
        return -1;
    }
    s->count = s->count ? s->count : buffer_target_priv->count;
    s->data = buffer_target_priv->data;
    s->data_stride = buffer_target_priv->data_stride;
    s->data_size = s->count * s->data_stride;

    return 0;
}

static int buffer_init(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->data && s->filename) {
        LOG(ERROR,
            "data and filename option cannot be set at the same time");
        return -1;
    }

    if (s->block && (s->data || s->filename)) {
        LOG(ERROR, "block option can not be set with data or filename");
        return -1;
    }

    int ret;
    int data_comp_size;
    int nb_comp;
    int format;

    switch (node->class->id) {
    case NGL_NODE_BUFFERBYTE:   data_comp_size = 1; nb_comp = 1; format = NGLI_FORMAT_R8_SNORM;               break;
    case NGL_NODE_BUFFERBVEC2:  data_comp_size = 1; nb_comp = 2; format = NGLI_FORMAT_R8G8_SNORM;             break;
    case NGL_NODE_BUFFERBVEC3:  data_comp_size = 1; nb_comp = 3; format = NGLI_FORMAT_R8G8B8_SNORM;           break;
    case NGL_NODE_BUFFERBVEC4:  data_comp_size = 1; nb_comp = 4; format = NGLI_FORMAT_R8G8B8A8_SNORM;         break;
    case NGL_NODE_BUFFERINT:    data_comp_size = 4; nb_comp = 1; format = NGLI_FORMAT_R32_SINT;               break;
    case NGL_NODE_BUFFERIVEC2:  data_comp_size = 4; nb_comp = 2; format = NGLI_FORMAT_R32G32_SINT;            break;
    case NGL_NODE_BUFFERIVEC3:  data_comp_size = 4; nb_comp = 3; format = NGLI_FORMAT_R32G32B32_SINT;         break;
    case NGL_NODE_BUFFERIVEC4:  data_comp_size = 4; nb_comp = 4; format = NGLI_FORMAT_R32G32B32A32_SINT;      break;
    case NGL_NODE_BUFFERSHORT:  data_comp_size = 2; nb_comp = 1; format = NGLI_FORMAT_R16_SNORM;              break;
    case NGL_NODE_BUFFERSVEC2:  data_comp_size = 2; nb_comp = 2; format = NGLI_FORMAT_R16G16_SNORM;           break;
    case NGL_NODE_BUFFERSVEC3:  data_comp_size = 2; nb_comp = 3; format = NGLI_FORMAT_R16G16B16_SNORM;        break;
    case NGL_NODE_BUFFERSVEC4:  data_comp_size = 2; nb_comp = 4; format = NGLI_FORMAT_R16G16B16A16_SNORM;     break;
    case NGL_NODE_BUFFERUBYTE:  data_comp_size = 1; nb_comp = 1; format = NGLI_FORMAT_R8_UNORM;               break;
    case NGL_NODE_BUFFERUBVEC2: data_comp_size = 1; nb_comp = 2; format = NGLI_FORMAT_R8G8_UNORM;             break;
    case NGL_NODE_BUFFERUBVEC3: data_comp_size = 1; nb_comp = 3; format = NGLI_FORMAT_R8G8B8_UNORM;           break;
    case NGL_NODE_BUFFERUBVEC4: data_comp_size = 1; nb_comp = 4; format = NGLI_FORMAT_R8G8B8A8_UNORM;         break;
    case NGL_NODE_BUFFERUINT:   data_comp_size = 4; nb_comp = 1; format = NGLI_FORMAT_R32_UINT;               break;
    case NGL_NODE_BUFFERUIVEC2: data_comp_size = 4; nb_comp = 2; format = NGLI_FORMAT_R32G32_UINT;            break;
    case NGL_NODE_BUFFERUIVEC3: data_comp_size = 4; nb_comp = 3; format = NGLI_FORMAT_R32G32B32_UINT;         break;
    case NGL_NODE_BUFFERUIVEC4: data_comp_size = 4; nb_comp = 4; format = NGLI_FORMAT_R32G32B32A32_UINT;      break;
    case NGL_NODE_BUFFERUSHORT: data_comp_size = 2; nb_comp = 1; format = NGLI_FORMAT_R16_UNORM;              break;
    case NGL_NODE_BUFFERUSVEC2: data_comp_size = 2; nb_comp = 2; format = NGLI_FORMAT_R16G16_UNORM;           break;
    case NGL_NODE_BUFFERUSVEC3: data_comp_size = 2; nb_comp = 3; format = NGLI_FORMAT_R16G16B16_UNORM;        break;
    case NGL_NODE_BUFFERUSVEC4: data_comp_size = 2; nb_comp = 4; format = NGLI_FORMAT_R16G16B16A16_UNORM;     break;
    case NGL_NODE_BUFFERFLOAT:  data_comp_size = 4; nb_comp = 1; format = NGLI_FORMAT_R32_SFLOAT;             break;
    case NGL_NODE_BUFFERVEC2:   data_comp_size = 4; nb_comp = 2; format = NGLI_FORMAT_R32G32_SFLOAT;          break;
    case NGL_NODE_BUFFERVEC3:   data_comp_size = 4; nb_comp = 3; format = NGLI_FORMAT_R32G32B32_SFLOAT;       break;
    case NGL_NODE_BUFFERVEC4:   data_comp_size = 4; nb_comp = 4; format = NGLI_FORMAT_R32G32B32A32_SFLOAT;    break;
    case NGL_NODE_BUFFERMAT4:   data_comp_size = 4; nb_comp = 16; format = NGLI_FORMAT_R32G32B32A32_SFLOAT;   break;
    default:
        ngli_assert(0);
    }

    s->usage = GL_STATIC_DRAW;
    s->data_comp = nb_comp;
    s->data_format = format;

    if (!s->data_stride)
        s->data_stride = s->data_comp * data_comp_size;

    if (s->data)
        ret = buffer_init_from_data(node);
    else if (s->filename)
        ret = buffer_init_from_filename(node);
    else if (s->block)
        ret = buffer_init_from_block(node);
    else
        ret = buffer_init_from_count(node);
    if (ret < 0)
        return ret;

    return 0;
}

static void buffer_uninit(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->filename) {
        ngli_free(s->data);
        s->data = NULL;
        s->data_size = 0;

        if (s->fd) {
            int ret = close(s->fd);
            if (ret < 0) {
                LOG(ERROR, "could not properly close '%s'", s->filename);
            }
        }
    } else if (s->block) {
        /* Prevent the param API to free a non-owned pointer */
        s->data = NULL;
        s->data_size = 0;
    }
}

#define DEFINE_BUFFER_CLASS(class_id, class_name, type)     \
const struct node_class ngli_buffer##type##_class = {       \
    .id        = class_id,                                  \
    .name      = class_name,                                \
    .init      = buffer_init,                               \
    .uninit    = buffer_uninit,                             \
    .priv_size = sizeof(struct buffer_priv),                \
    .params    = buffer_params,                             \
    .params_id = "Buffer",                                  \
    .file      = __FILE__,                                  \
};

DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBYTE,    "BufferByte",    byte)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC2,   "BufferBVec2",   bvec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC3,   "BufferBVec3",   bvec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC4,   "BufferBVec4",   bvec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT,     "BufferInt",     int)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC2,   "BufferIVec2",   ivec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC3,   "BufferIVec3",   ivec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC4,   "BufferIVec4",   ivec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSHORT,   "BufferShort",   short)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC2,   "BufferSVec2",   svec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC3,   "BufferSVec3",   svec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC4,   "BufferSVec4",   svec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBYTE,   "BufferUByte",   ubyte)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC2,  "BufferUBVec2",  ubvec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC3,  "BufferUBVec3",  ubvec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC4,  "BufferUBVec4",  ubvec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUINT,    "BufferUInt",    uint)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC2,  "BufferUIVec2",  uivec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC3,  "BufferUIVec3",  uivec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC4,  "BufferUIVec4",  uivec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSHORT,  "BufferUShort",  ushort)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC2,  "BufferUSVec2",  usvec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC3,  "BufferUSVec3",  usvec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC4,  "BufferUSVec4",  usvec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERFLOAT,   "BufferFloat",   float)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC2,    "BufferVec2",    vec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC3,    "BufferVec3",    vec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC4,    "BufferVec4",    vec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERMAT4,    "BufferMat4",    mat4)
