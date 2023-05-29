/*
 * Copyright 2023 Nope Project
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

#include "drawutils.h"
#include "internal.h"
#include "memory.h"
#include "text.h"
#include "utils.h"

struct text *ngli_text_create(struct ngl_ctx *ctx)
{
    struct text *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

static int atlas_create(struct ngl_ctx *ctx)
{
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    if (ctx->font_atlas)
        return 0;

    struct canvas canvas = {0};
    int ret = ngli_drawutils_get_font_atlas(&canvas);
    if (ret < 0)
        goto end;

    struct texture_params tex_params = {
        .type          = NGLI_TEXTURE_TYPE_2D,
        .width         = canvas.w,
        .height        = canvas.h,
        .format        = NGLI_FORMAT_R8_UNORM,
        .min_filter    = NGLI_FILTER_LINEAR,
        .mag_filter    = NGLI_FILTER_NEAREST,
        .mipmap_filter = NGLI_MIPMAP_FILTER_LINEAR,
        .usage         = NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT
                       | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT
                       | NGLI_TEXTURE_USAGE_SAMPLED_BIT,
    };

    ctx->font_atlas = ngli_texture_create(gpu_ctx); // freed at context reconfiguration/destruction
    if (!ctx->font_atlas) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    ret = ngli_texture_init(ctx->font_atlas, &tex_params);
    if (ret < 0)
        goto end;

    ret = ngli_texture_upload(ctx->font_atlas, canvas.buf, 0);
    if (ret < 0)
        goto end;

end:
    ngli_free(canvas.buf);
    return ret;
}

int ngli_text_init(struct text *s, const struct text_config *cfg)
{
    s->config = *cfg;

    ngli_darray_init(&s->chars, sizeof(struct char_info), 0);

    int ret = atlas_create(s->ctx);
    if (ret < 0)
        return ret;

    s->texture = s->ctx->font_atlas;

    return 0;
}

static void get_char_box_dim(const char *s, int32_t *wp, int32_t *hp, size_t *np)
{
    int32_t w = 0, h = 1;
    int32_t cur_w = 0;
    size_t n = 0;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '\n') {
            cur_w = 0;
            h++;
        } else {
            cur_w++;
            w = NGLI_MAX(w, cur_w);
            n++;
        }
    }
    *wp = w;
    *hp = h;
    *np = n;
}

int ngli_text_set_string(struct text *s, const char *str)
{
    ngli_darray_clear(&s->chars);

    size_t text_nbchr;
    int32_t text_cols, text_rows;
    get_char_box_dim(str, &text_cols, &text_rows, &text_nbchr);

    s->width  = text_cols * NGLI_FONT_W + 2 * s->config.padding;
    s->height = text_rows * NGLI_FONT_H + 2 * s->config.padding;

    const float padx = (float)s->config.padding / (float)s->width;
    const float pady = (float)s->config.padding / (float)s->height;

    const float chr_w = (1.f - 2.f * padx) / (float)text_cols;
    const float chr_h = (1.f - 2.f * pady) / (float)text_rows;

    int32_t px = 0, py = 0;

    for (size_t i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            py++;
            px = 0;
            continue;
        }

        struct char_info chr = {
            .x = chr_w * (float)px + padx,
            .y = chr_h * (float)(text_rows - py - 1) + pady,
            .w = chr_w,
            .h = chr_h,
        };

        ngli_drawutils_get_atlas_uvcoords(str[i], chr.uvcoords);

        if (!ngli_darray_push(&s->chars, &chr))
            return NGL_ERROR_MEMORY;

        px++;
    }

    return 0;
}

void ngli_text_freep(struct text **sp)
{
    struct text *s = *sp;
    ngli_darray_reset(&s->chars);
    ngli_freep(sp);
}
