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

#include "buffer.h"
#include "glincludes.h"
#include "nodes.h"

int ngli_buffer_allocate(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct buffer *s = node->priv_data;

    if (!s->buffer_id) {
        ngli_glGenBuffers(gl, 1, &s->buffer_id);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->buffer_id);
        ngli_glBufferData(gl, GL_ARRAY_BUFFER, s->data_size, s->data, s->usage);
        s->buffer_last_upload_time = -1;
    }
    s->buffer_refcount++;
    return 0;
}

void ngli_buffer_upload(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct buffer *s = node->priv_data;

    if (s->dynamic && s->buffer_last_upload_time != node->last_update_time) {
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->buffer_id);
        ngli_glBufferSubData(gl, GL_ARRAY_BUFFER, 0, s->data_size, s->data);
        s->buffer_last_upload_time = node->last_update_time;
    }
}

void ngli_buffer_free(struct ngl_node *node)
{
    if (!node)
        return;

    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct buffer *s = node->priv_data;

    if (s->buffer_refcount-- == 1) {
        ngli_glDeleteBuffers(gl, 1, &s->buffer_id);
        s->buffer_id = 0;
        s->buffer_last_upload_time = -1;
    }
    ngli_assert(s->buffer_refcount >= 0);
}
