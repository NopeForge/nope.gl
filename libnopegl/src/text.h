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

#include "darray.h"
#include "nopegl.h"

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

struct char_info {
    float x, y, w, h;
    float uvcoords[4 * 2];
};

struct text_config {
    int32_t padding;
};

struct text {
    struct ngl_ctx *ctx;
    struct text_config config;

    int32_t width;
    int32_t height;
    struct darray chars; // struct char_info
};

struct text *ngli_text_create(struct ngl_ctx *ctx);
int ngli_text_init(struct text *s, const struct text_config *cfg);
int ngli_text_set_string(struct text *s, const char *str);
void ngli_text_freep(struct text **sp);

#endif
