/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_BUFFER_GL_H
#define NGPU_BUFFER_GL_H

#include "cmd_buffer_gl.h"
#include "glincludes.h"
#include "ngpu/buffer.h"
#include "utils/darray.h"

struct ngpu_buffer_gl {
    struct ngpu_buffer parent;
    GLuint id;
    GLbitfield map_flags;
    GLbitfield barriers;
    struct darray cmd_buffers;
};

struct ngpu_ctx;

struct ngpu_buffer *ngpu_buffer_gl_create(struct ngpu_ctx *gpu_ctx);
int ngpu_buffer_gl_init(struct ngpu_buffer *s);
int ngpu_buffer_gl_wait(struct ngpu_buffer *s);
int ngpu_buffer_gl_add_wait_fence(struct ngpu_buffer *s, GLsync fence);
int ngpu_buffer_gl_upload(struct ngpu_buffer *s, const void *data, size_t offset, size_t size);
int ngpu_buffer_gl_map(struct ngpu_buffer *s, size_t offset, size_t size, void **datap);
void ngpu_buffer_gl_unmap(struct ngpu_buffer *s);
int ngpu_buffer_gl_ref_cmd_buffer(struct ngpu_buffer *s, struct ngpu_cmd_buffer_gl *cmd_buffer);
int ngpu_buffer_gl_unref_cmd_buffer(struct ngpu_buffer *s, struct ngpu_cmd_buffer_gl *cmd_buffer);
void ngpu_buffer_gl_freep(struct ngpu_buffer **sp);

#endif
