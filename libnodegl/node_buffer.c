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

#include <stddef.h>
#include <stdlib.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct buffer, x)
static const struct node_param buffer_params[] = {
    {"count",  PARAM_TYPE_INT,  OFFSET(count)},
    {"data",   PARAM_TYPE_DATA, OFFSET(data)},
    {"stride", PARAM_TYPE_INT,  OFFSET(data_stride)},
    {"target", PARAM_TYPE_INT,  OFFSET(target), {.i64=GL_ARRAY_BUFFER}},
    {"usage",  PARAM_TYPE_INT,  OFFSET(usage),  {.i64=GL_STATIC_DRAW}},
    {NULL}
};

static int buffer_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct buffer *s = node->priv_data;

    int data_comp_size;
    switch (node->class->id) {
    case NGL_NODE_BUFFERUBYTE:  data_comp_size = 1; s->data_comp = 1; s->type = GL_UNSIGNED_BYTE;  break;
    case NGL_NODE_BUFFERUSHORT: data_comp_size = 2; s->data_comp = 1; s->type = GL_UNSIGNED_SHORT; break;
    case NGL_NODE_BUFFERUINT:   data_comp_size = 4; s->data_comp = 1; s->type = GL_UNSIGNED_INT;   break;
    case NGL_NODE_BUFFERFLOAT:  data_comp_size = 4; s->data_comp = 1; s->type = GL_FLOAT;          break;
    case NGL_NODE_BUFFERVEC2:   data_comp_size = 4; s->data_comp = 2; s->type = GL_FLOAT;          break;
    case NGL_NODE_BUFFERVEC3:   data_comp_size = 4; s->data_comp = 3; s->type = GL_FLOAT;          break;
    case NGL_NODE_BUFFERVEC4:   data_comp_size = 4; s->data_comp = 4; s->type = GL_FLOAT;          break;
    default:
        ngli_assert(0);
    }

    if (!s->data_stride)
        s->data_stride = s->data_comp * data_comp_size;

    if (s->data) {
        s->count = s->count ? s->count : s->data_size / s->data_stride;
        if (s->data_size != s->count * s->data_stride) {
            LOG(ERROR,
                "Element count (%d) does not match data size (%d)",
                s->count,
                s->data_size);
            return -1;
        }
    } else {
        s->count = s->count ? s->count : 1;
        s->data_size = s->count * s->data_stride;
        s->data = calloc(s->count, s->data_stride);
        if (!s->data)
            return -1;
    }

    ngli_glGenBuffers(gl, 1, &s->buffer_id);
    ngli_glBindBuffer(gl, s->target, s->buffer_id);
    ngli_glBufferData(gl, s->target, s->data_size, s->data, s->usage);
    ngli_glBindBuffer(gl, s->target, 0);

    return 0;
}

static void buffer_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct buffer *s = node->priv_data;

    ngli_glDeleteBuffers(gl, 1, &s->buffer_id);

    free(s->data);
    s->data = NULL;
}

const struct node_class ngli_bufferfloat_class = {
    .id        = NGL_NODE_BUFFERFLOAT,
    .name      = "BufferFloat",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};

const struct node_class ngli_bufferubyte_class = {
    .id        = NGL_NODE_BUFFERUBYTE,
    .name      = "BufferUByte",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};

const struct node_class ngli_bufferuint_class = {
    .id        = NGL_NODE_BUFFERUINT,
    .name      = "BufferUInt",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};

const struct node_class ngli_bufferushort_class = {
    .id        = NGL_NODE_BUFFERUSHORT,
    .name      = "BufferUShort",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};

const struct node_class ngli_buffervec2_class = {
    .id        = NGL_NODE_BUFFERVEC2,
    .name      = "BufferVec2",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};

const struct node_class ngli_buffervec3_class = {
    .id        = NGL_NODE_BUFFERVEC3,
    .name      = "BufferVec3",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};

const struct node_class ngli_buffervec4_class = {
    .id        = NGL_NODE_BUFFERVEC4,
    .name      = "BufferVec4",
    .init      = buffer_init,
    .uninit    = buffer_uninit,
    .priv_size = sizeof(struct buffer),
    .params    = buffer_params,
};
