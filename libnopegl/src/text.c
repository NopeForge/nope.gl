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

#include <string.h>

#include "memory.h"
#include "text.h"

extern const struct text_cls ngli_text_builtin;

struct box_stats {
    enum writing_mode writing_mode;
    struct darray linelens;         // int32_t, length of each line (not necessarily horizontal)
    int32_t max_linelen;            // maximum value in the linelens array
    int32_t linemin, linemax;       // current line min/max
    int32_t xmin, xmax;             // current box min/max on x-axis
    int32_t ymin, ymax;             // current box min/max on y-axis
};

static void box_stats_init(struct box_stats *s, enum writing_mode writing_mode)
{
    s->writing_mode = writing_mode;
    ngli_darray_init(&s->linelens, sizeof(int32_t), 0);
    s->max_linelen = INT32_MIN;
    s->linemin = s->xmin = s->ymin = INT32_MAX;
    s->linemax = s->xmax = s->ymax = INT32_MIN;
}

static int box_stats_register_eol(struct box_stats *s)
{
    const int32_t len = s->linemax == INT32_MIN ? 0 : s->linemax - s->linemin;
    if (!ngli_darray_push(&s->linelens, &len))
        return NGL_ERROR_MEMORY;
    s->max_linelen = NGLI_MAX(s->max_linelen, len);
    s->linemin = INT32_MAX;
    s->linemax = INT32_MIN;
    return 0;
}

static void box_stats_register_chr(struct box_stats *s, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (s->writing_mode == NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB) {
        s->linemin = NGLI_MIN(s->linemin, x);
        s->linemax = NGLI_MAX(s->linemax, x + w);
    } else {
        s->linemin = NGLI_MIN(s->linemin, y);
        s->linemax = NGLI_MAX(s->linemax, y + h);
    }
    s->xmin = NGLI_MIN(s->xmin, x);
    s->xmax = NGLI_MAX(s->xmax, x + w);
    s->ymin = NGLI_MIN(s->ymin, y);
    s->ymax = NGLI_MAX(s->ymax, y + h);
}

static void box_stats_reset(struct box_stats *s)
{
    ngli_darray_reset(&s->linelens);
    memset(s, 0, sizeof(*s));
}

static int build_stats(struct box_stats *stats, struct darray *chars_array, enum writing_mode writing_mode)
{
    box_stats_init(stats, writing_mode);

    const struct char_info_internal *chars_internal = ngli_darray_data(chars_array);
    for (size_t i = 0; i < ngli_darray_count(chars_array); i++) {
        const struct char_info_internal *chr = &chars_internal[i];
        if (chr->tags & NGLI_TEXT_CHAR_TAG_GLYPH)
            box_stats_register_chr(stats, chr->x, chr->y, chr->w, chr->h);

        if (chr->tags & NGLI_TEXT_CHAR_TAG_LINE_BREAK) {
            int ret = box_stats_register_eol(stats);
            if (ret < 0)
                return ret;
        }
    }

    /* We simulate an EOF to make sure the last line length is taken into account */
    int ret = box_stats_register_eol(stats);
    if (ret < 0)
        return ret;

    return 0;
}

struct text *ngli_text_create(struct ngl_ctx *ctx)
{
    struct text *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_text_init(struct text *s, const struct text_config *cfg)
{
    s->config = *cfg;

    ngli_darray_init(&s->chars, sizeof(struct char_info), 0);

    s->cls = &ngli_text_builtin;
    if (s->cls->priv_size) {
        s->priv_data = ngli_calloc(1, s->cls->priv_size);
        if (!s->priv_data)
            return NGL_ERROR_MEMORY;
    }
    return s->cls->init(s);
}

int ngli_text_set_string(struct text *s, const char *str)
{
    struct box_stats stats = {0};
    struct darray chars_internal_array;
    ngli_darray_init(&chars_internal_array, sizeof(struct char_info_internal), 0);

    int ret = s->cls->set_string(s, str, &chars_internal_array);
    if (ret < 0)
        goto end;

    /* Build bounding box statistics for the layout logic */
    build_stats(&stats, &chars_internal_array, s->config.writing_mode);

    /* Make sure it doesn't explode if the string is empty or only contains line breaks */
    if (stats.max_linelen <= 0) {
        s->width = 0;
        s->height = 0;
        goto end;
    }

    const int32_t padding = NGLI_I32_TO_I26D6(s->config.padding);
    s->width  = NGLI_I26D6_TO_I32_TRUNCATED(stats.xmax - stats.xmin + 2 * padding);
    s->height = NGLI_I26D6_TO_I32_TRUNCATED(stats.ymax - stats.ymin + 2 * padding);

    /* Honor layout */
    int32_t line = 0;
    const int32_t *linelens = ngli_darray_data(&stats.linelens);
    struct char_info_internal *chars_internal = ngli_darray_data(&chars_internal_array);
    for (size_t i = 0; i < ngli_darray_count(&chars_internal_array); i++) {
        struct char_info_internal *chr = &chars_internal[i];

        if (chr->tags & NGLI_TEXT_CHAR_TAG_LINE_BREAK)
            line++;

        if (!(chr->tags & NGLI_TEXT_CHAR_TAG_GLYPH))
            continue;

        /* Recenter text so that it starts at (0,0) */
        chr->x -= stats.xmin;
        chr->y -= stats.ymin;

        chr->x += padding;
        chr->y += padding;

        /* Honor the alignment setting for each line */
        const int32_t space = stats.max_linelen - linelens[line];
        if (s->config.writing_mode == NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB) {
            if      (s->config.halign == NGLI_TEXT_HALIGN_CENTER) chr->x += space / 2;
            else if (s->config.halign == NGLI_TEXT_HALIGN_RIGHT)  chr->x += space;
        } else {
            if      (s->config.valign == NGLI_TEXT_VALIGN_CENTER) chr->y -= space / 2;
            else if (s->config.valign == NGLI_TEXT_VALIGN_BOTTOM) chr->y -= space;
        }
    }

    /* Expose characters publicly */
    ngli_darray_clear(&s->chars);
    for (size_t i = 0; i < ngli_darray_count(&chars_internal_array); i++) {
        const struct char_info_internal *chr_internal = &chars_internal[i];

        if (!(chr_internal->tags & NGLI_TEXT_CHAR_TAG_GLYPH))
            continue;

        const struct char_info chr = {
            .x = NGLI_I26D6_TO_F32(chr_internal->x) / (float)s->width,
            .y = NGLI_I26D6_TO_F32(chr_internal->y) / (float)s->height,
            .w = NGLI_I26D6_TO_F32(chr_internal->w) / (float)s->width,
            .h = NGLI_I26D6_TO_F32(chr_internal->h) / (float)s->height,
            .atlas_coords = {
                (float)chr_internal->atlas_coords[0] / (float)s->texture->params.width,
                (float)chr_internal->atlas_coords[1] / (float)s->texture->params.height,
                (float)chr_internal->atlas_coords[2] / (float)s->texture->params.width,
                (float)chr_internal->atlas_coords[3] / (float)s->texture->params.height,
            },
        };

        if (!ngli_darray_push(&s->chars, &chr)) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }
    }

end:
    ngli_darray_reset(&chars_internal_array);
    box_stats_reset(&stats);
    return ret;
}

void ngli_text_freep(struct text **sp)
{
    struct text *s = *sp;
    if (s->cls->reset)
        s->cls->reset(s);
    ngli_freep(&s->priv_data);
    ngli_darray_reset(&s->chars);
    ngli_freep(sp);
}
