/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2016-2022 GoPro Inc.
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

#include <stdlib.h>
#include <string.h>

#include "gpu_format.h"
#include "gpu_ctx.h"
#include "internal.h"
#include "memory.h"
#include "gpu_rendertarget.h"
#include "rtt.h"

struct rtt_ctx {
    struct ngl_ctx *ctx;
    struct rtt_params params;

    struct gpu_texture *color;

    struct gpu_rendertarget *rt;
    struct gpu_rendertarget *rt_resume;
    struct gpu_rendertarget *available_rendertargets[2];
    struct gpu_texture *depth;

    struct gpu_texture *ms_colors[NGLI_GPU_MAX_COLOR_ATTACHMENTS];
    size_t nb_ms_colors;
    struct gpu_texture *ms_depth;

    struct image images[NGLI_GPU_MAX_COLOR_ATTACHMENTS];

    int started;
    struct gpu_viewport prev_viewport;
    struct gpu_scissor prev_scissor;
    struct gpu_rendertarget *prev_rendertargets[2];
    struct gpu_rendertarget *prev_rendertarget;
};

struct rtt_ctx *ngli_rtt_create(struct ngl_ctx *ctx)
{
    struct rtt_ctx *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_rtt_init(struct rtt_ctx *s, const struct rtt_params *params)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    s->params = *params;

    uint32_t transient_usage = 0;
    if (!params->nb_interruptions)
        transient_usage |= NGLI_GPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    struct gpu_rendertarget_params rt_params = {
        .width = s->params.width,
        .height = s->params.height,
    };

    for (size_t i = 0; i < s->params.nb_colors; i++) {
        struct gpu_attachment *attachment = &s->params.colors[i];
        if (s->params.samples > 1) {
            struct gpu_texture *texture = attachment->attachment;
            const int texture_layer = attachment->attachment_layer;

            struct gpu_texture *ms_texture = ngli_gpu_texture_create(gpu_ctx);
            if (!ms_texture)
                return NGL_ERROR_MEMORY;
            s->ms_colors[s->nb_ms_colors++] = ms_texture;

            struct gpu_texture_params attachment_params = {
                .type    = NGLI_GPU_TEXTURE_TYPE_2D,
                .format  = texture->params.format,
                .width   = s->params.width,
                .height  = s->params.height,
                .samples = s->params.samples,
                .usage   = NGLI_GPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | transient_usage,
            };
            int ret = ngli_gpu_texture_init(ms_texture, &attachment_params);
            if (ret < 0)
                return ret;

            rt_params.colors[rt_params.nb_colors].attachment = ms_texture;
            rt_params.colors[rt_params.nb_colors].attachment_layer = 0;
            rt_params.colors[rt_params.nb_colors].resolve_target = texture;
            rt_params.colors[rt_params.nb_colors].resolve_target_layer = texture_layer;
            rt_params.colors[rt_params.nb_colors].load_op = NGLI_GPU_LOAD_OP_CLEAR;
            float *clear_value = rt_params.colors[rt_params.nb_colors].clear_value;
            memcpy(clear_value, attachment->clear_value, sizeof(attachment->clear_value));
            const int store_op = s->params.nb_interruptions ? NGLI_GPU_STORE_OP_STORE : NGLI_GPU_STORE_OP_DONT_CARE;
            rt_params.colors[rt_params.nb_colors].store_op = store_op;
        } else {
            rt_params.colors[rt_params.nb_colors] = s->params.colors[i];
        }

        const struct image_params image_params = {
            .width = s->params.width,
            .height = s->params.height,
            .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
            .color_info = NGLI_COLOR_INFO_DEFAULTS,
        };
        ngli_image_init(&s->images[i], &image_params, &rt_params.colors[i].attachment);

        rt_params.nb_colors++;
    }

    if (s->params.depth_stencil.attachment) {
        struct gpu_attachment *attachment = &s->params.depth_stencil;
        if (s->params.samples > 1) {
            struct gpu_texture *texture = attachment->attachment;
            const int texture_layer = attachment->attachment_layer;

            struct gpu_texture *ms_texture = ngli_gpu_texture_create(gpu_ctx);
            if (!ms_texture)
                return NGL_ERROR_MEMORY;
            s->ms_depth = ms_texture;

            struct gpu_texture_params attachment_params = {
                .type    = NGLI_GPU_TEXTURE_TYPE_2D,
                .format  = texture->params.format,
                .width   = s->params.width,
                .height  = s->params.height,
                .samples = s->params.samples,
                .usage   = NGLI_GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient_usage,
            };
            int ret = ngli_gpu_texture_init(ms_texture, &attachment_params);
            if (ret < 0)
                return ret;

            rt_params.depth_stencil.attachment = ms_texture;
            rt_params.depth_stencil.attachment_layer = 0;
            rt_params.depth_stencil.resolve_target = texture;
            rt_params.depth_stencil.resolve_target_layer = texture_layer;
            rt_params.depth_stencil.load_op = NGLI_GPU_LOAD_OP_CLEAR;
            const int store_op = s->params.nb_interruptions ? NGLI_GPU_STORE_OP_STORE : NGLI_GPU_STORE_OP_DONT_CARE;
            rt_params.depth_stencil.store_op = store_op;
        } else {
            rt_params.depth_stencil = s->params.depth_stencil;
        }
    } else if (s->params.depth_stencil_format != NGLI_GPU_FORMAT_UNDEFINED) {
        struct gpu_texture *depth = ngli_gpu_texture_create(gpu_ctx);
        if (!depth)
            return NGL_ERROR_MEMORY;
        s->depth = depth;

        struct gpu_texture_params attachment_params = {
            .type    = NGLI_GPU_TEXTURE_TYPE_2D,
            .format  = s->params.depth_stencil_format,
            .width   = s->params.width,
            .height  = s->params.height,
            .samples = s->params.samples,
            .usage   = NGLI_GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient_usage,
        };
        int ret = ngli_gpu_texture_init(depth, &attachment_params);
        if (ret < 0)
            return ret;

        rt_params.depth_stencil.attachment = depth;
        rt_params.depth_stencil.load_op = NGLI_GPU_LOAD_OP_CLEAR;
        /*
         * For the first rendertarget with load operations set to clear, if
         * the depth attachment is not exposed in the graph (ie: it is not
         * a user supplied texture) and if the renderpass is not
         * interrupted we can discard the depth attachment at the end of
         * the renderpass.
         */
        const int store_op = s->params.nb_interruptions ? NGLI_GPU_STORE_OP_STORE : NGLI_GPU_STORE_OP_DONT_CARE;
        rt_params.depth_stencil.store_op = store_op;
    }

    s->rt = ngli_gpu_rendertarget_create(gpu_ctx);
    if (!s->rt)
        return NGL_ERROR_MEMORY;

    int ret = ngli_gpu_rendertarget_init(s->rt, &rt_params);
    if (ret < 0)
        return ret;

    s->available_rendertargets[0] = s->rt;
    s->available_rendertargets[1] = s->rt;

    if (s->params.nb_interruptions) {
        for (size_t i = 0; i < rt_params.nb_colors; i++)
            rt_params.colors[i].load_op = NGLI_GPU_LOAD_OP_LOAD;
        rt_params.depth_stencil.load_op = NGLI_GPU_LOAD_OP_LOAD;

        if (s->params.depth_stencil.attachment) {
            rt_params.depth_stencil.store_op = NGLI_GPU_STORE_OP_STORE;
        } else {
            /*
             * For the second rendertarget with load operations set to load, if
             * the depth attachment is not exposed in the graph (ie: it is not
             * a user supplied texture) and if the renderpass is interrupted
             * *once*, we can discard the depth attachment at the end of the
             * renderpass.
             */
            const int store_op = s->params.nb_interruptions > 1 ? NGLI_GPU_STORE_OP_STORE : NGLI_GPU_STORE_OP_DONT_CARE;
            rt_params.depth_stencil.store_op = store_op;
        }

        s->rt_resume = ngli_gpu_rendertarget_create(gpu_ctx);
        if (!s->rt_resume)
            return NGL_ERROR_MEMORY;

        ret = ngli_gpu_rendertarget_init(s->rt_resume, &rt_params);
        if (ret < 0)
            return ret;
        s->available_rendertargets[1] = s->rt_resume;
    }

    return 0;
}

int ngli_rtt_from_texture_params(struct rtt_ctx *s, const struct gpu_texture_params *params)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    s->color = ngli_gpu_texture_create(gpu_ctx);
    if (!s->color)
        return NGL_ERROR_MEMORY;

    int ret = ngli_gpu_texture_init(s->color, params);
    if (ret < 0)
        return ret;

    const struct rtt_params rtt_params = (struct rtt_params) {
        .width = params->width,
        .height = params->height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = s->color,
            .load_op = NGLI_GPU_LOAD_OP_CLEAR,
            .store_op = NGLI_GPU_STORE_OP_STORE,
        },
    };

    return ngli_rtt_init(s, &rtt_params);
}

void ngli_rtt_get_dimensions(struct rtt_ctx *s, int32_t *width, int32_t *height)
{
    *width = s->params.width;
    *height = s->params.height;
}

struct gpu_texture *ngli_rtt_get_texture(struct rtt_ctx *s, size_t index)
{
    ngli_assert(index < s->params.nb_colors);
    return s->params.colors[index].attachment;
}

struct image *ngli_rtt_get_image(struct rtt_ctx *s, size_t index)
{
    ngli_assert(index < s->params.nb_colors);
    return &s->images[index];
}

void ngli_rtt_begin(struct rtt_ctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    ngli_assert(!s->started);
    s->started = 1;
    s->prev_viewport = ctx->viewport;
    s->prev_scissor = ctx->scissor;
    s->prev_rendertargets[0] = ctx->available_rendertargets[0];
    s->prev_rendertargets[1] = ctx->available_rendertargets[1];
    s->prev_rendertarget = ctx->current_rendertarget;

    if (ctx->render_pass_started) {
        ngli_gpu_ctx_end_render_pass(gpu_ctx);
        ctx->render_pass_started = 0;
        s->prev_rendertarget = ctx->available_rendertargets[1];
    }

    const int32_t width = s->params.width;
    const int32_t height = s->params.height;
    ctx->viewport = (struct gpu_viewport){0, 0, width, height};
    ctx->scissor = (struct gpu_scissor){0, 0, width, height};

    ctx->available_rendertargets[0] = s->available_rendertargets[0];
    ctx->available_rendertargets[1] = s->available_rendertargets[1];
    ctx->current_rendertarget = s->available_rendertargets[0];
}

void ngli_rtt_end(struct rtt_ctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    ngli_assert(s->started);
    s->started = 0;

    if (!ctx->render_pass_started) {
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->render_pass_started = 1;
    }
    ngli_gpu_ctx_end_render_pass(gpu_ctx);

    ctx->render_pass_started = 0;
    ctx->current_rendertarget = s->prev_rendertarget;
    ctx->available_rendertargets[0] = s->prev_rendertargets[0];
    ctx->available_rendertargets[1] = s->prev_rendertargets[1];
    ctx->viewport = s->prev_viewport;
    ctx->scissor = s->prev_scissor;

    for (size_t i = 0; i < s->params.nb_colors; i++) {
        struct gpu_texture *texture = s->params.colors[i].attachment;
        const struct gpu_texture_params *texture_params = &texture->params;
        if (texture_params->mipmap_filter != NGLI_GPU_MIPMAP_FILTER_NONE)
            ngli_gpu_ctx_generate_texture_mipmap(gpu_ctx, texture);
    }
}

void ngli_rtt_freep(struct rtt_ctx **sp)
{
    struct rtt_ctx *s = *sp;
    if (!s)
        return;

    s->available_rendertargets[0] = NULL;
    s->available_rendertargets[1] = NULL;

    ngli_gpu_rendertarget_freep(&s->rt);
    ngli_gpu_rendertarget_freep(&s->rt_resume);
    ngli_gpu_texture_freep(&s->depth);

    for (size_t i = 0; i < s->nb_ms_colors; i++)
        ngli_gpu_texture_freep(&s->ms_colors[i]);
    s->nb_ms_colors = 0;
    ngli_gpu_texture_freep(&s->ms_depth);
    ngli_gpu_texture_freep(&s->color);

    ngli_freep(sp);
}
