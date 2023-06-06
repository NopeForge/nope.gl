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

#include "atlas.h"
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
    if (ctx->font_atlas)
        return 0;

    ctx->font_atlas = ngli_atlas_create(ctx);
    if (!ctx->font_atlas)
        return NGL_ERROR_MEMORY;

    int ret = ngli_atlas_init(ctx->font_atlas);
    if (ret < 0)
        return 0;

    for (uint8_t chr = 32; chr < 127; chr++) {
        /* Load the glyph corresponding to the ASCII character */
        uint8_t glyph[NGLI_FONT_W * NGLI_FONT_H];
        const struct bitmap bitmap = {
            .buffer = glyph,
            .stride = NGLI_FONT_W,
            .width  = NGLI_FONT_W,
            .height = NGLI_FONT_H,
        };
        ngli_drawutils_get_glyph(bitmap.buffer, chr);

        /* Register the glyph in the atlas */
        int32_t bitmap_id;
        ret = ngli_atlas_add_bitmap(ctx->font_atlas, &bitmap, &bitmap_id);
        if (ret < 0)
            return ret;

        /* Map the character codepoint to its bitmap ID in the atlas */
        ctx->char_map[chr] = bitmap_id;
    }

    ngli_atlas_finalize(ctx->font_atlas);

    return 0;
}

int ngli_text_init(struct text *s, const struct text_config *cfg)
{
    s->config = *cfg;

    ngli_darray_init(&s->chars, sizeof(struct char_info), 0);

    int ret = atlas_create(s->ctx);
    if (ret < 0)
        return ret;

    s->texture = ngli_atlas_get_texture(s->ctx->font_atlas);
    ngli_atlas_get_dimensions(s->ctx->font_atlas, s->atlas_dim);

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

        const struct char_info chr = {
            .atlas_id = s->ctx->char_map[(uint8_t)str[i]],
            .x = chr_w * (float)px + padx,
            .y = chr_h * (float)(text_rows - py - 1) + pady,
            .w = chr_w,
            .h = chr_h,
        };

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
