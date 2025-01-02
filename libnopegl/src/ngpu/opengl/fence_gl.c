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
 **
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "ctx_gl.h"
#include "fence_gl.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "utils/memory.h"

static void fence_freep(void **fencep)
{
    struct ngpu_fence_gl **sp = (struct ngpu_fence_gl **)fencep;

    struct ngpu_fence_gl *s = *sp;
    if (!s)
        return;

    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    if (s->fence) {
        gl->funcs.DeleteSync(s->fence);
    }

    ngli_freep(sp);
}

struct ngpu_fence_gl *ngpu_fence_gl_create(struct ngpu_ctx *ctx)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    struct ngpu_fence_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->gpu_ctx = ctx;
    s->rc = NGLI_RC_CREATE(fence_freep);
    s->fence = gl->funcs.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (s->fence == 0) {
        ngli_free(s);
        return NULL;
    }

    return s;
}

int ngpu_fence_gl_wait(struct ngpu_fence_gl *s)
{
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    if (s->fence == 0)
        return 0;

    GLenum ret = gl->funcs.ClientWaitSync(s->fence, GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
    if (ret == GL_TIMEOUT_EXPIRED) {
        LOG(ERROR, "fence timeout expired");
        return NGL_ERROR_GRAPHICS_GENERIC;
    } else if (ret == GL_WAIT_FAILED) {
        LOG(ERROR, "fence wait failed");
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    return 0;
}

void ngpu_fence_gl_freep(struct ngpu_fence_gl **sp)
{
    NGLI_RC_UNREFP(sp);
}
