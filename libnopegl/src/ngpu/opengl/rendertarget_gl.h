/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_RENDERTARGET_GL_H
#define NGPU_RENDERTARGET_GL_H

#include "glincludes.h"
#include "ngpu/rendertarget.h"

struct ngpu_rendertarget_gl {
    struct ngpu_rendertarget parent;
    int wrapped;
    GLuint id;
    GLuint resolve_id;
    GLenum draw_buffers[NGPU_MAX_COLOR_ATTACHMENTS];
    GLenum clear_flags;
    GLenum invalidate_attachments[NGPU_MAX_COLOR_ATTACHMENTS + 2]; // max color attachments + depth and stencil attachments
    int nb_invalidate_attachments;
    void (*clear)(struct ngpu_rendertarget *s);
    void (*invalidate)(struct ngpu_rendertarget *s);
    void (*resolve)(struct ngpu_rendertarget *s);
};

struct ngpu_rendertarget *ngpu_rendertarget_gl_create(struct ngpu_ctx *gpu_ctx);
int ngpu_rendertarget_gl_init(struct ngpu_rendertarget *s);
void ngpu_rendertarget_gl_begin_pass(struct ngpu_rendertarget *s);
void ngpu_rendertarget_gl_end_pass(struct ngpu_rendertarget *s);
void ngpu_rendertarget_gl_freep(struct ngpu_rendertarget **sp);

int ngpu_rendertarget_gl_wrap(struct ngpu_rendertarget *s, const struct ngpu_rendertarget_params *params, GLuint id);

#endif
