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

struct text_builtin {
    int32_t chr_w, chr_h;
    int32_t char_map[256];
    struct atlas *atlas;
};

static int atlas_create(struct text *text)
{
    struct text_builtin *s = text->priv_data;

    s->atlas = ngli_atlas_create(text->ctx);
    if (!s->atlas)
        return NGL_ERROR_MEMORY;

    int ret = ngli_atlas_init(s->atlas);
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
        ret = ngli_atlas_add_bitmap(s->atlas, &bitmap, &bitmap_id);
        if (ret < 0)
            return ret;

        /* Map the character codepoint to its bitmap ID in the atlas */
        s->char_map[chr] = bitmap_id;
    }

    return ngli_atlas_finalize(s->atlas);
}

static int text_builtin_init(struct text *text)
{
    struct text_builtin *s = text->priv_data;

    s->chr_w = NGLI_FONT_W;
    s->chr_h = NGLI_FONT_H;

    int ret = atlas_create(text);
    if (ret < 0)
        return ret;

    text->atlas_texture = ngli_atlas_get_texture(s->atlas);

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

static uint32_t get_char_tags(char c)
{
    if (c == ' ')
        return NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR;
    if (c == '\n')
        return NGLI_TEXT_CHAR_TAG_LINE_BREAK;
    return NGLI_TEXT_CHAR_TAG_GLYPH;
}

static int text_builtin_set_string(struct text *text, const char *str, struct darray *chars_dst)
{
    struct text_builtin *s = text->priv_data;

    size_t text_nbchr;
    int32_t text_cols, text_rows;
    get_char_box_dim(str, &text_cols, &text_rows, &text_nbchr);

    int32_t col = 0, row = 0;

    for (size_t i = 0; str[i]; i++) {
        const enum char_tag tags = get_char_tags(str[i]);
        if ((tags & NGLI_TEXT_CHAR_TAG_GLYPH) != NGLI_TEXT_CHAR_TAG_GLYPH) {
            const struct char_info_internal chr = {.tags = tags};
            if (!ngli_darray_push(chars_dst, &chr))
                return NGL_ERROR_MEMORY;
            if (tags & NGLI_TEXT_CHAR_TAG_LINE_BREAK) {
                switch (text->config.writing_mode) {
                case NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB: row++; col = 0; break;
                case NGLI_TEXT_WRITING_MODE_VERTICAL_RL:   col--; row = 0; break;
                case NGLI_TEXT_WRITING_MODE_VERTICAL_LR:   col++; row = 0; break;
                default: ngli_assert(0);
                }
            } else if (tags & NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR) {
                switch (text->config.writing_mode) {
                case NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB: col++; break;
                case NGLI_TEXT_WRITING_MODE_VERTICAL_RL:
                case NGLI_TEXT_WRITING_MODE_VERTICAL_LR:   row++; break;
                default: ngli_assert(0);
                }
            } else {
                ngli_assert(0);
            }
            continue;
        }

        const int32_t atlas_id = s->char_map[(uint8_t)str[i]];
        int32_t atlas_coords[4];
        ngli_atlas_get_bitmap_coords(s->atlas, atlas_id, atlas_coords);

        const struct char_info_internal chr = {
            .x = NGLI_I32_TO_I26D6(s->chr_w * col),
            .y = NGLI_I32_TO_I26D6(s->chr_h * (text_rows - row - 1)),
            .w = NGLI_I32_TO_I26D6(s->chr_w),
            .h = NGLI_I32_TO_I26D6(s->chr_h),
            .atlas_coords = {NGLI_ARG_VEC4(atlas_coords)},
            .tags = NGLI_TEXT_CHAR_TAG_GLYPH,
        };

        if (!ngli_darray_push(chars_dst, &chr))
            return NGL_ERROR_MEMORY;

        switch (text->config.writing_mode) {
        case NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB: col++; break;
        case NGLI_TEXT_WRITING_MODE_VERTICAL_RL:
        case NGLI_TEXT_WRITING_MODE_VERTICAL_LR:   row++; break;
        default: ngli_assert(0);
        }
    }

    return 0;
}

static void text_builtin_reset(struct text *text)
{
    struct text_builtin *s = text->priv_data;
    ngli_atlas_freep(&s->atlas);
}

const struct text_cls ngli_text_builtin = {
    .init            = text_builtin_init,
    .set_string      = text_builtin_set_string,
    .reset           = text_builtin_reset,
    .priv_size       = sizeof(struct text_builtin),
    .flags           = 0,
};
