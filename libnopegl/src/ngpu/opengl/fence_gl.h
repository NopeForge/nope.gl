/*
 * Copyright 2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NGPU_FENCE_GL_H
#define NGPU_FENCE_GL_H

#include "glincludes.h"
#include "utils/refcount.h"

struct ngpu_ctx;

struct ngpu_fence_gl {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    GLsync fence;
};

NGLI_RC_CHECK_STRUCT(ngpu_fence_gl);

struct ngpu_fence_gl *ngpu_fence_gl_create(struct ngpu_ctx *ctx);
int ngpu_fence_gl_wait(struct ngpu_fence_gl *s);
void ngpu_fence_gl_freep(struct ngpu_fence_gl **sp);

#endif
