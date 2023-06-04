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

#ifndef TEXT_H
#define TEXT_H

#include "atlas.h"
#include "darray.h"
#include "nopegl.h"
#include "texture.h"

#define NGLI_I32_TO_I26D6(x) ((x) * (1 << 6))     // convert i32 to 26.6 fixed point
#define NGLI_I26D6_TO_F32(x) ((float)(x) / 64.f)  // convert 26.6 fixed point to f32
#define NGLI_I26D6_TO_I32_TRUNCATED(x) ((x) >> 6) // convert 26.6 fixed point to i32 (truncated/rounded down)

enum writing_mode {
    NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB,
    NGLI_TEXT_WRITING_MODE_VERTICAL_RL,
    NGLI_TEXT_WRITING_MODE_VERTICAL_LR,
};

enum text_valign {
    NGLI_TEXT_VALIGN_CENTER,
    NGLI_TEXT_VALIGN_TOP,
    NGLI_TEXT_VALIGN_BOTTOM,
};

enum text_halign {
    NGLI_TEXT_HALIGN_CENTER,
    NGLI_TEXT_HALIGN_RIGHT,
    NGLI_TEXT_HALIGN_LEFT,
};

enum char_tag {
    NGLI_TEXT_CHAR_TAG_GLYPH          = 1 << 0,
    NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR = 1 << 1,
    NGLI_TEXT_CHAR_TAG_LINE_BREAK     = 1 << 2,
};

/* Exposed by text drivers  */
struct char_info_internal {
    int32_t x, y, w, h; // pixels canvas coordinates encoded in 26.6 fixed point
    int32_t atlas_coords[4]; // pixel atlas coordinates
    uint32_t tags; // combination of NGLI_TEXT_CHAR_TAG_*
};

/* Exposed by the text API */
struct char_info {
    float x, y, w, h;
    float atlas_coords[4];
};

struct text_config {
    int32_t padding;
    enum text_valign valign;
    enum text_halign halign;
    enum writing_mode writing_mode;
};

struct text;

/* structure reserved for internal implementations */
struct text_cls {
    int (*init)(struct text *text);
    int (*set_string)(struct text *text, const char *str, struct darray *chars_dst);
    void (*reset)(struct text *text);
    size_t priv_size;
};

struct text {
    struct ngl_ctx *ctx;
    struct text_config config;

    int32_t width;
    int32_t height;
    struct darray chars; // struct char_info
    struct texture *texture;

    const struct text_cls *cls;
    void *priv_data;
};

struct text *ngli_text_create(struct ngl_ctx *ctx);
int ngli_text_init(struct text *s, const struct text_config *cfg);
int ngli_text_set_string(struct text *s, const char *str);
void ngli_text_freep(struct text **sp);

#endif
