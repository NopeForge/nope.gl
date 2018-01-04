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

static const struct param_choices target_choices = {
    .name = "buffer_target",
    .consts = {
        {"array_buffer",          GL_ARRAY_BUFFER,          .desc=NGLI_DOCSTRING("vertex attributes")},
        {"element_array_buffer",  GL_ELEMENT_ARRAY_BUFFER,  .desc=NGLI_DOCSTRING("vertex array indices")},
        {"shader_storage_buffer", GL_SHADER_STORAGE_BUFFER, .desc=NGLI_DOCSTRING("read-write storage for shaders")},
        {NULL}
    }
};

static const struct param_choices usage_choices = {
    .name = "buffer_usage",
    .consts = {
        {"stream_draw",  GL_STREAM_DRAW,  .desc=NGLI_DOCSTRING("modified once by the application "
                                                               "and used at most a few times as a source for drawing")},
        {"stream_read",  GL_STREAM_READ,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used at most a few times to return the data to the application")},
        {"stream_copy",  GL_STREAM_COPY,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used at most a few times as a source for drawing")},
        {"static_draw",  GL_STATIC_DRAW,  .desc=NGLI_DOCSTRING("modified once by the application "
                                                               "and used many times as a source for drawing")},
        {"static_read",  GL_STATIC_READ,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used many times to return the data to the application")},
        {"static_copy",  GL_STATIC_COPY,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used at most a few times a source for drawing")},
        {"dynamic_draw", GL_DYNAMIC_DRAW, .desc=NGLI_DOCSTRING("modified repeatedly by the application "
                                                               "and used many times as a source for drawing")},
        {"dynamic_read", GL_DYNAMIC_READ, .desc=NGLI_DOCSTRING("modified repeatedly by reading data from the graphic pipeline "
                                                               "and used many times to return data to the application")},
        {"dynamic_copy", GL_DYNAMIC_COPY, .desc=NGLI_DOCSTRING("modified repeatedly by reading data from the graphic pipeline "
                                                               "and used many times as a source for drawing")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct buffer, x)
static const struct node_param buffer_params[] = {
    {"count",  PARAM_TYPE_INT,    OFFSET(count),
               .desc=NGLI_DOCSTRING("number of elements")},
    {"data",   PARAM_TYPE_DATA,   OFFSET(data),
               .desc=NGLI_DOCSTRING("buffer of `count` elements")},
    {"stride", PARAM_TYPE_INT,    OFFSET(data_stride),
               .desc=NGLI_DOCSTRING("stride of 1 element, in bytes")},
    {"target", PARAM_TYPE_SELECT, OFFSET(target), {.i64=GL_ARRAY_BUFFER},
               .desc=NGLI_DOCSTRING("target to which the buffer will be bound"),
               .choices=&target_choices},
    {"usage",  PARAM_TYPE_SELECT, OFFSET(usage),  {.i64=GL_STATIC_DRAW},
               .desc=NGLI_DOCSTRING("buffer usage hint"),
               .choices=&usage_choices},
    {NULL}
};

static int buffer_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct buffer *s = node->priv_data;

    int data_comp_size;
    int nb_comp;
    GLenum comp_type;

    switch (node->class->id) {
    case NGL_NODE_BUFFERBYTE:   data_comp_size = 1; nb_comp = 1; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERBVEC2:  data_comp_size = 1; nb_comp = 2; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERBVEC3:  data_comp_size = 1; nb_comp = 3; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERBVEC4:  data_comp_size = 1; nb_comp = 4; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERINT:    data_comp_size = 4; nb_comp = 1; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERIVEC2:  data_comp_size = 4; nb_comp = 2; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERIVEC3:  data_comp_size = 4; nb_comp = 3; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERIVEC4:  data_comp_size = 4; nb_comp = 4; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERSHORT:  data_comp_size = 2; nb_comp = 1; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERSVEC2:  data_comp_size = 2; nb_comp = 2; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERSVEC3:  data_comp_size = 2; nb_comp = 3; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERSVEC4:  data_comp_size = 2; nb_comp = 4; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERUBYTE:  data_comp_size = 1; nb_comp = 1; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUBVEC2: data_comp_size = 1; nb_comp = 2; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUBVEC3: data_comp_size = 1; nb_comp = 3; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUBVEC4: data_comp_size = 1; nb_comp = 4; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUINT:   data_comp_size = 4; nb_comp = 1; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUIVEC2: data_comp_size = 4; nb_comp = 2; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUIVEC3: data_comp_size = 4; nb_comp = 3; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUIVEC4: data_comp_size = 4; nb_comp = 4; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUSHORT: data_comp_size = 2; nb_comp = 1; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERUSVEC2: data_comp_size = 2; nb_comp = 2; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERUSVEC3: data_comp_size = 2; nb_comp = 3; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERUSVEC4: data_comp_size = 2; nb_comp = 4; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERFLOAT:  data_comp_size = 4; nb_comp = 1; comp_type = GL_FLOAT;             break;
    case NGL_NODE_BUFFERVEC2:   data_comp_size = 4; nb_comp = 2; comp_type = GL_FLOAT;             break;
    case NGL_NODE_BUFFERVEC3:   data_comp_size = 4; nb_comp = 3; comp_type = GL_FLOAT;             break;
    case NGL_NODE_BUFFERVEC4:   data_comp_size = 4; nb_comp = 4; comp_type = GL_FLOAT;             break;
    default:
        ngli_assert(0);
    }

    s->data_comp = nb_comp;
    s->comp_type = comp_type;

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
}

#define DEFINE_BUFFER_CLASS(class_id, class_name, type)     \
const struct node_class ngli_buffer##type##_class = {       \
    .id        = class_id,                                  \
    .name      = class_name,                                \
    .init      = buffer_init,                               \
    .uninit    = buffer_uninit,                             \
    .priv_size = sizeof(struct buffer),                     \
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
