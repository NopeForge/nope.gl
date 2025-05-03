/*
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

#include "rendertarget.h"
#include "ctx.h"

static void rendertarget_freep(void **rendertargetp)
{
    struct ngpu_rendertarget **sp = (struct ngpu_rendertarget **)rendertargetp;
    if (!*sp)
        return;

    (*sp)->gpu_ctx->cls->rendertarget_freep(sp);
}

struct ngpu_rendertarget *ngpu_rendertarget_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_rendertarget *s = gpu_ctx->cls->rendertarget_create(gpu_ctx);
    if (!s)
        return NULL;
    s->rc = NGLI_RC_CREATE(rendertarget_freep);
    return s;
}

int ngpu_rendertarget_init(struct ngpu_rendertarget *s, const struct ngpu_rendertarget_params *params)
{
    const struct ngpu_limits *limits = &s->gpu_ctx->limits;
    const uint64_t features = s->gpu_ctx->features;

    s->params = *params;
    s->width = params->width;
    s->height = params->height;

    ngli_assert(params->nb_colors <= NGPU_MAX_COLOR_ATTACHMENTS);
    ngli_assert(params->nb_colors <= limits->max_color_attachments);

    if (params->depth_stencil.resolve_target) {
        ngli_assert(features & NGPU_FEATURE_DEPTH_STENCIL_RESOLVE);
    }

    /* Set the rendertarget samples value from the attachments samples value
     * and ensure all the attachments have the same width/height/samples value */
    uint32_t samples = UINT32_MAX;
    for (size_t i = 0; i < params->nb_colors; i++) {
        const struct ngpu_attachment *attachment = &params->colors[i];
        const struct ngpu_texture *texture = attachment->attachment;
        const struct ngpu_texture_params *texture_params = &texture->params;
        s->layout.colors[s->layout.nb_colors].format = texture_params->format;
        s->layout.colors[s->layout.nb_colors].resolve = attachment->resolve_target != NULL;
        s->layout.nb_colors++;
        ngli_assert(texture_params->width == s->width);
        ngli_assert(texture_params->height == s->height);
        ngli_assert(texture_params->usage & NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
        if (attachment->resolve_target) {
            const struct ngpu_texture_params *target_params = &attachment->resolve_target->params;
            ngli_assert(target_params->width == s->width);
            ngli_assert(target_params->height == s->height);
            ngli_assert(target_params->usage & NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
        }
        ngli_assert(samples == UINT32_MAX || samples == texture_params->samples);
        samples = texture_params->samples;
    }
    const struct ngpu_attachment *attachment = &params->depth_stencil;
    const struct ngpu_texture *texture = attachment->attachment;
    if (texture) {
        const struct ngpu_texture_params *texture_params = &texture->params;
        s->layout.depth_stencil.format = texture_params->format;
        s->layout.depth_stencil.resolve = attachment->resolve_target != NULL;
        ngli_assert(texture_params->width == s->width);
        ngli_assert(texture_params->height == s->height);
        ngli_assert(texture_params->usage & NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        if (attachment->resolve_target) {
            const struct ngpu_texture_params *target_params = &attachment->resolve_target->params;
            ngli_assert(target_params->width == s->width);
            ngli_assert(target_params->height == s->height);
            ngli_assert(target_params->usage & NGPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }
        ngli_assert(samples == UINT32_MAX || samples == texture_params->samples);
        samples = texture_params->samples;
    }
    s->layout.samples = samples;

    return s->gpu_ctx->cls->rendertarget_init(s);
}

void ngpu_rendertarget_freep(struct ngpu_rendertarget **sp)
{
    NGLI_RC_UNREFP(sp);
}
