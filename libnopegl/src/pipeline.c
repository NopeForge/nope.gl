/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2019-2022 GoPro Inc.
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

#include <string.h>

#include "gpu_ctx.h"
#include "log.h"
#include "memory.h"
#include "pipeline.h"
#include "buffer.h"
#include "type.h"
#include "utils.h"

int ngli_pipeline_graphics_copy(struct pipeline_graphics *dst, const struct pipeline_graphics *src)
{
    dst->topology = src->topology;
    dst->state    = src->state;
    dst->rt_layout  = src->rt_layout;
    NGLI_ARRAY_MEMDUP(dst->vertex_state, src->vertex_state, buffers);

    return 0;
}

void ngli_pipeline_graphics_reset(struct pipeline_graphics *graphics)
{
    ngli_freep(&graphics->vertex_state.buffers);
    memset(graphics, 0, sizeof(*graphics));
}

struct pipeline *ngli_pipeline_create(struct gpu_ctx *gpu_ctx)
{
    return gpu_ctx->cls->pipeline_create(gpu_ctx);
}

int ngli_pipeline_init(struct pipeline *s, const struct pipeline_params *params)
{
    s->type     = params->type;

    int ret = ngli_pipeline_graphics_copy(&s->graphics, &params->graphics);
    if (ret < 0)
        return ret;

    s->program  = params->program;
    s->layout = params->layout;

    return s->gpu_ctx->cls->pipeline_init(s);
}

void ngli_pipeline_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    ngli_pipeline_graphics_reset(&s->graphics);

    (*sp)->gpu_ctx->cls->pipeline_freep(sp);
}
