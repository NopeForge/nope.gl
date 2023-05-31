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

static int text_builtin_init(struct text *text)
{
    int ret = atlas_create(text->ctx);
    if (ret < 0)
        return ret;

    text->texture = ngli_atlas_get_texture(text->ctx->font_atlas);

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

static int text_builtin_set_string(struct text *text, const char *str, struct darray *chars_dst)
{
    size_t text_nbchr;
    int32_t text_cols, text_rows;
    get_char_box_dim(str, &text_cols, &text_rows, &text_nbchr);

    text->width  = text_cols * NGLI_FONT_W + 2 * text->config.padding;
    text->height = text_rows * NGLI_FONT_H + 2 * text->config.padding;

    const float padx = (float)text->config.padding / (float)text->width;
    const float pady = (float)text->config.padding / (float)text->height;

    const float chr_w = (1.f - 2.f * padx) / (float)text_cols;
    const float chr_h = (1.f - 2.f * pady) / (float)text_rows;

    int32_t px = 0, py = 0;

    for (size_t i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            py++;
            px = 0;
            continue;
        }

        const int32_t atlas_id = text->ctx->char_map[(uint8_t)str[i]];
        int32_t atlas_coords[4];
        ngli_atlas_get_bitmap_coords(text->ctx->font_atlas, atlas_id, atlas_coords);

        const struct char_info chr = {
            .x = chr_w * (float)px + padx,
            .y = chr_h * (float)(text_rows - py - 1) + pady,
            .w = chr_w,
            .h = chr_h,
            .atlas_coords = {
                (float)atlas_coords[0] / (float)text->texture->params.width,
                (float)atlas_coords[1] / (float)text->texture->params.height,
                (float)atlas_coords[2] / (float)text->texture->params.width,
                (float)atlas_coords[3] / (float)text->texture->params.height,
            },
        };

        if (!ngli_darray_push(chars_dst, &chr))
            return NGL_ERROR_MEMORY;

        px++;
    }

    return 0;
}

const struct text_cls ngli_text_builtin = {
    .init            = text_builtin_init,
    .set_string      = text_builtin_set_string,
};
