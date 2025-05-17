/*
 * Copyright 2023 Nope Forge
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

#include "box.h"
#include "distmap.h"
#include "internal.h"
#include "path.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "text.h"
#include "utils/utils.h"

struct text_builtin {
    uint32_t chr_w, chr_h;
    const struct text_builtin_atlas *atlas;
};

#define FIRST_CHAR '!'

/*
 * Origin is top-left. We define a "grid" of 7x8, but we allow ourselves points
 * half ways, so we're effectively working with a 14x16 grid. In practice, we
 * need to have some padding around each glyph, so we can consider an
 * exploitable 12x14 grid.
 */
static const uint32_t view_w = 7;
static const uint32_t view_h = 8;
static const char *outlines[] = {
    /* ! */ "M3 1 v4 h1 v-4 z m0 5.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* " */ "M3 1 v2 h1 v-2 z m2 0 v2 h1 v-2 z",
    /* # */ "M2 1.5 v1 h-1 v1 h1 v1 h-1 v1 h1 v1 h1 v-1 h1 v1 h1 v-1 h1 v-1 h-1 v-1 h1 v-1 h-1 v-1 h-1 v1 h-1 v-1 z m1 2 h1 v1 h-1 z",
    /* $ */ "M6 1 h-3 q-2 0 -2 2 v.5 q0 1 1 1 h2.5 q.5 0 .5 .5 0 1 -1 1 h-3 v.5 q0 .5 .5 .5 h2.5 q2 0 2 -2 v-.5 q0 -1 -1 -1 h-2.5 q-.5 0 -.5 -.5 0 -1 1 -1 h3 z M3 0 v8 h1 v-8 z",
    /* % */ "M1 2.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m4 4 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m.5 -4.5 l-4.5 4.5 .5 .5 4.5 -4.5 z",
    /* & */ "M2.5 1 q-1.5 0 -1.5 1.5 0 1 1 1.5 -1 .5 -1 1.5 0 1.5 1.5 1.5 .5 0 1.5 -1 l1 1 .5 -.5 -1 -1 1 -1.5 -.5 -.5 -1 1.5 -1 -1 q1 -.5 1 -1.5 0 -1.5 -1.5 -1.5 m0 1 q.5 0 .5 .5 0 .5 -.5 .5 -.5 0 -.5 -.5 0 -.5 .5 -.5 m0 2.5 l1 1 q-1 1 -1.5 .5 -.5 -1 .5 -1.5",
    /* ' */ "M2 1 q0 1 -1 1 v1 q2 0 2 -2 z",
    /* ( */ "M5 1 h-1 q-2 0 -2 2 v2 q0 2 2 2 h1 v-1 h-1 q-1 0 -1 -1 v-2 q0 -1 1 -1 h1 z",
    /* ) */ "M2 1 v1 h1 q1 0 1 1 v2 q0 1 -1 1 h-1 v1 h1 q2 0 2 -2 v-2 q0 -2 -2 -2 z",
    /* * */ "M1 3 v1 h2 v2 h1 v-2 h2 v-1 h-2 v-2 h-1 v2 z m1 -1.5 l-.5 .5 1.5 1.5 -1.5 1.5 .5 .5 1.5 -1.5 1.5 1.5 .5 -.5 -1.5 -1.5 1.5 -1.5 -.5 -.5 -1.5 1.5 z",
    /* + */ "M1 4 v1 h2 v2 h1 v-2 h2 v-1 h-2 v-2 h-1 v2 z",
    /* , */ "M2 6 q0 1 -1 1 v1 q2 0 2 -2 z",
    /* - */ "M2 4 v1 h3 v-1 z",
    /* . */ "M2 6.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* / */ "M6 1.5 l-.5 -.5 -4.5 5.5 .5 .5 z",
    /* 0 */ "M3 1 q-2 0 -2 2 v2 q0 2 2 2 h1 q2 0 2 -2 v-2 q0 -2 -2 -2 z m-1 4.5 v-2.5 q0 -1 1 -1 h1.5 z m3 -3 v2.5 q0 1 -1 1 h-1.5 z",
    /* 1 */ "M3 1 q0 1 -1 1 v1 h1 v4 h1 v-1 v-5 z",
    /* 2 */ "M1 3 h1 q0 -1 1 -1 h1 q1 0 1 1 0 1 -1 1 h-1 q-2 0 -2 2 v1 h4.5 q.5 0 .5 -.5 v-.5 h-4 q0 -1 1 -1 h1 q2 0 2 -2 0 -2 -2 -2 h-1 q-2 0 -2 2",
    /* 3 */ "M1 3 h1 q0 -1 1 -1 h1 q1 0 1 1 0 .5 -.5 .5 h-1.5 v1 h1.5 q.5 0 .5 .5 0 1 -1 1 h-1 q-1 0 -1 -1 h-1 q0 2 2 2 h1 q2 0 2 -2 0 -1 -1 -1 1 0 1 -1 0 -2 -2 -2 h-1 q-2 0 -2 2",
    /* 4 */ "M4 1 l-3 3 v1 h3 v2 h1 v-2 h1 v-1 h-1 v-3 h-1 m0 1.5 v1.5 h-1.5 z",
    /* 5 */ "M6 1 h-5 v3 h3 q1 0 1 1 0 1 -1 1 h-1 q-1 0 -1 -1 h-1 q0 2 2 2 h1 q2 0 2 -2 0 -2 -2 -2 h-2 v-1 h4 z",
    /* 6 */ "M3 1 q-2 0 -2 2 v2 q0 2 2 2 h1 q2 0 2 -2 0 -2 -2 -2 h-2 q0 -1 1 -1 h2.5 q0 -1 -.5 -1 z m1 3 q1 0 1 1 0 1 -1 1 h-1 q-1 0 -1 -1 0 -1 1 -1 z",
    /* 7 */ "M1 1 v1 h4 l-3 5 h1 l3 -5 v-1 z",
    /* 8 */ "M3 1 q-2 0 -2 1.5 0 1.5 1 1.5 -1 0 -1 1.5 0 1.5 2 1.5 h1 q2 0 2 -1.5 0 -1.5 -1 -1.5 1 0 1 -1.5 0 -1.5 -2 -1.5 z m1 1 q1 0 1 .5 0 1 -1 1 h-1 q-1 0 -1 -1 0 -.5 1 -.5 z m0 2.5 q1 0 1 1 0 .5 -1 .5 h-1 q-1 0 -1 -.5 0 -1 1 -1 z",
    /* 9 */ "M3 1 q-2 0 -2 2 0 2 2 2 h2 q0 1 -1 1 h-2.5 q0 1 .5 1 h2 q2 0 2 -2 v-2 q0 -2 -2 -2 z m1 1 q1 0 1 1 0 1 -1 1 h-1 q-1 0 -1 -1 0 -1 1 -1 z",
    /* : */ "M2 3.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 3 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* ; */ "M2 3.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 2.5 q0 1 -1 1 v1 q2 0 2 -2 z",
    /* < */ "M4.5 1 l-3 3 3 3 .5 -.5 -2.5 -2.5 2.5 -2.5 -.5 -.5",
    /* = */ "M1 2 h5 v1 h-5 z m0 3 h5 v1 h-5 z",
    /* > */ "M2.5 1 l-.5 .5 2.5 2.5 -2.5 2.5 .5 .5 3 -3 -3 -3",
    /* ? */ "M2 1 v1 h2.5 q.5 0 .5 .5 0 .5 -.5 .5 h-1 q-.5 0 -.5 .5 v1.5 h1 v-1 h1 q1 0 1 -1 v-1 q0 -1 -1 -1 z m1 5.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* @ */ "M3 4 q0 1 1 1 h.5 q.5 0 .5 -.5 v-.5 q1 0 1 -1 v-1 q0 -1 -1 -1 h-1 q-3 0 -3 3 0 3 3 3 h1 q2 0 2 -2 h-1 q0 1 -1 1 h-1 q-2 0 -2 -2 0 -2 2 -2 h.5 q.5 0 .5 .5 v.5 h-1 q-1 0 -1 1",
    /* A */ "M1 3 v4 h1 v-2 h3 v2 h1 v-4 q0 -2 -2 -2 h-1 q-2 0 -2 2 m1 0 q0 -1 1 -1 h1 q1 0 1 1 v1 h-3 z",
    /* B */ "M1 1 v6 h3 q2 0 2 -1.5 0 -1.5 -1 -1.5 1 0 1 -1.5 0 -1.5 -2 -1.5 z m3 1 q1 0 1 .5 0 1 -1 1 h-2 v-1.5 z m0 2.5 q1 0 1 1 0 .5 -1 .5 h-2 v-1.5 z",
    /* C */ "M6 1 h-2 q-3 0 -3 3 0 3 3 3 h2 v-1 h-2 q-2 0 -2 -2 0 -2 2 -2 h2 z",
    /* D */ "M1 1 v6 h2 q3 0 3 -3 0 -3 -3 -3 z m1 1 h1 q2 0 2 2 0 2 -2 2 h-1 z",
    /* E */ "M1 1 v6 h5 v-1 h-4 v-1.5 h3 v-1 h-3 v-1.5 h4 v-1 z",
    /* F */ "M1 1 v6 h1 v-2.5 h3 v-1 h-3 v-1.5 h4 v-1 z",
    /* G */ "M6 3 q0 -2 -2 -2 -3 0 -3 3 0 3 3 3 h1.5 q.5 0 .5 -.5 v-2.5 h-2 v1 h1 v1 h-1 q-2 0 -2 -2 0 -2 2 -2 1 0 1 1 z",
    /* H */ "M1 1 v6 h1 v-2.5 h3 v2.5 h1 v-6 h-1 v2.5 h-3 v-2.5 z",
    /* I */ "M2 1 v1 h1 v4 h-1 v1 h3 v-1 h-1 v-4 h1 v-1 z",
    /* J */ "M5 1 v4 q0 1 -1 1 h-1 q-1 0 -1 -1 h-1 q0 2 2 2 h1 q2 0 2 -2 v-4 z",
    /* K */ "M1 1 v6 h1 v-2.5 l3.5 -2.5 -.5 -1 -3 2 v-2 z m2 2.5 l2 3.5 h1 l-2 -4 z",
    /* L */ "M1 1 v6 h5 v-1 h-4 v-5 z",
    /* M */ "M1 1 v6 h1 v-5 l1.5 1 1.5 -1 v5 h1 v-6 h-1 l-1.5 1 -1.5 -1 z",
    /* N */ "M1 1 v6 h1 v-4.5 l2.5 4.5 h1.5 v-6 h-1 v4.5 l-2.5 -4.5 z",
    /* O */ "M3 1 q-2 0 -2 2 v2 q0 2 2 2 h1 q2 0 2 -2 v-2 q0 -2 -2 -2 z m0 1 h1 q1 0 1 1 v2 q0 1 -1 1 h-1 q-1 0 -1 -1 v-2 q0 -1 1 -1",
    /* P */ "M1 1 v6 h1 v-2 h2 q2 0 2 -2 0 -2 -2 -2 z m1 1 h2 q1 0 1 1 0 1 -1 1 h-2 z",
    /* Q */ "M4 1 h-1 q-2 0 -2 2 v1 q0 2 2 2 h1 q0 1 1 1 h1 v-.5 q-1 0 -1 -1 1 0 1 -1 v-1.5 q0 -2 -2 -2 m0 1 q1 0 1 1 v1 q0 1 -1 1 h-1 q-1 0 -1 -1 v-1 q0 -1 1 -1 z",
    /* R */ "M1 1 v6 h1 v-2 h.5 l2 2 h1.5 l-2 -2 q2 0 2 -2 0 -2 -2 -2 z m1 1 h2 q1 0 1 1 0 1 -1 1 h-2 z",
    /* S */ "M6 1 h-3 q-2 0 -2 2 v.5 q0 1 1 1 h2.5 q.5 0 .5 .5 0 1 -1 1 h-3 v.5 q0 .5 .5 .5 h2.5 q2 0 2 -2 v-.5 q0 -1 -1 -1 h-2.5 q-.5 0 -.5 -.5 0 -1 1 -1 h3 z",
    /* T */ "M1 1 v1 h2 v5 h1 v-5 h2 v-1 z",
    /* U */ "M1 1 v4 q0 2 2 2 h1 q2 0 2 -2 v-4 h-1 v4 q0 1 -1 1 h-1 q-1 0 -1 -1 v-4 z",
    /* V */ "M1 1 l2 6 h1 l2 -6 h-1 l-1.5 4.5 -1.5 -4.5 z",
    /* W */ "M1 1 l1 6 1.5 -1.5 1.5 1.5 1 -6 h-1 l-.5 4 -1 -1 -1 1 -.5 -4 z",
    /* X */ "M2 1 h-1 l4 6 h1 z m4 0 h-1 l-4 6 h1 z",
    /* Y */ "M3 3.5 l-1.5 -2 h-1 l2 3 v3 h1 v-3 l2 -3 h-1 l-1.5 2",
    /* Z */ "M1 1 v1 h4 l-4 4 v1 h5 v-1 h-4 l4 -4 v-1 z",
    /* [ */ "M5 1 h-3 v6 h3 v-1 h-2 v-4 h2 v-1 z",
    /* \ */ "M1.5 1 l-.5 .5 4.5 5.5 .5 -.5 z",
    /* ] */ "M2 1 v1 h2 v4 h-2 v1 h3 v-6 z",
    /* ^ */ "M3.5 1 l-2.5 2.5 .5 .5 2 -2 2 2 .5 -.5 z",
    /* _ */ "M1 7 v1 h5 v-1 z",
    /* ` */ "M1.5 1 l-.5 .5 1.5 1.5 .5 -.5 z",
    /* a */ "M6 2 h-3 q-2 0 -2 2 v1 q0 2 2 2 h1.5 q1.5 0 1.5 -2 z m-1 1 v2 q0 1 -1 1 h-1 q-1 0 -1 -1 v-1 q0 -1 1 -1 z m0 2 q0 2 2 2 v-1 q-1 0 -1 -1 z",
    /* b */ "M1 0 v6 q0 1 1 1 h2 q2 0 2 -2 v-1 q0 -2 -2 -2 h-2 v-2 z m1.5 3 h1.5 q1 0 1 1 v1 q0 1 -1 1 h-1.5 q-.5 0 -.5 -.5 v-2 q0 -.5 .5 -.5",
    /* c */ "M5 4 h1 v-1 q0 -1 -1 -1 h-2 q-2 0 -2 2 v1 q0 2 2 2 h2.5 q.5 0 .5 -.5 v-.5 h-3 q-1 0 -1 -1 v-1 q0 -1 1 -1 h1.5 q.5 0 .5 .5 z",
    /* d */ "M6 0 h-1 v2 h-2 q-2 0 -2 2 v1 q0 2 2 2 h2 q1 0 1 -1 z m-1 3.5 v2 q0 .5 -.5 .5 h-1.5 q-1 0 -1 -1 v-1 q0 -1 1 -1 h1.5 q.5 0 .5 .5",
    /* e */ "M6 5 v-1 q0 -2 -2 -2 h-1 q-2 0 -2 2 v1 q0 2 2 2 h2.5 q.5 0 .5 -.5 v-.5 h-3 q-1 0 -1 -1 z m-1 -1 h-3 q0 -1 1 -1 h1 q1 0 1 1",
    /* f */ "M6 1 h-3 q-1 0 -1 1 v1 h-1 v1 h1 v3 h1 v-3 h2 v-1 h-2 v-.5 q0 -.5 .5 -.5 h2.5 z",
    /* g */ "M6 2 h-3 q-2 0 -2 2 0 2 2 2 h2 v.5 q0 .5 -.5 .5 h-3.5 v.5 q0 .5 .5 .5 h3.5 q1 0 1 -1 z m-1 1 v2 h-2 q-1 0 -1 -1 0 -1 1 -1 z",
    /* h */ "M1 0 v7 h1 v-2.5 q0 -.5 .5 -.5 h1.5 q1 0 1 1 v2 h1 v-2 q0 -2 -2 -2 h-2 v-3 z",
    /* i */ "M3 1.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 1.5 h-1 v1 h.5 q.5 0 .5 .5 v1.5 q0 1 1 1 h1 v-1 h-.5 q-.5 0 -.5 -.5 v-1.5 q0 -1 -1 -1",
    /* j */ "M4 1.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 1.5 v3 q0 1 -1 1 -1 0 -1 -1 h-1 q0 2 2 2 2 0 2 -2 v-3 z",
    /* k */ "M1 0 v7 h1 v-2 l2.5 -1 -.5 -1 -2 1 v-4 z m1 4 l1.5 3 h1 l-1.5 -3 z",
    /* l */ "M2 0 v1 h.5 q.5 0 .5 .5 v4.5 q0 1 1 1 h1 v-1 h-.5 q-.5 0 -.5 -.5 v-4.5 q0 -1 -1 -1 z",
    /* m */ "M1 2 v5 h1 v-4 q1 0 1 1 v3 h1 v-4 q1 0 1 1 v3 h1 v-3 q0 -2 -2 -2 z",
    /* n */ "M1 2 v5 h1 v-2 q0 -2 1 -2 h1 q1 0 1 1 v3 h1 v-3 q0 -2 -2 -2 h-1 q-1 0 -1 1 v-1 z",
    /* o */ "M4 2 h-1 q-2 0 -2 2 v1 q0 2 2 2 h1 q2 0 2 -2 v-1 q0 -2 -2 -2 m0 1 q1 0 1 1 v1 q0 1 -1 1 h-1 q-1 0 -1 -1 v-1 q0 -1 1 -1 z",
    /* p */ "M1 2 v6 h1 v-2 h2 q2 0 2 -2 0 -2 -2 -2 z m1 1 h2 q1 0 1 1 0 1 -1 1 h-2 z",
    /* q */ "M6 2 h-3 q-2 0 -2 2 0 2 2 2 h2 v2 h1 z m-1 1 v2 h-2 q-1 0 -1 -1 0 -1 1 -1 z",
    /* r */ "M1 2 v5 h1 v-3 q0 -1 1 -1 h1 q1 0 1 1 h1 q0 -2 -2 -2 h-1 q-1 0 -1 1 v-1 z",
    /* s */ "M6 2 h-4 q-1 0 -1 1 v1 q0 1 1 1 h2.5 q.5 0 .5 .5 0 .5 -.5 .5 h-3.5 v.5 q0 .5 .5 .5 h3.5 q1 0 1 -1 v-1 q0 -1 -1 -1 h-2.5 q-.5 0 -.5 -.5 0 -.5 .5 -.5 h3.5 z",
    /* t */ "M2 1 v1 h-1 v1 h1 v2 q0 2 2 2 h.5 q.5 0 .5 -.5 v-.5 h-1 q-1 0 -1 -1 v-2 h2 v-1 h-2 v-1 z",
    /* u */ "M1 2 v3 q0 2 2 2 h1 q1 0 1 -1 v1 h1 v-5 h-1 v2 q0 2 -1 2 h-1 q-1 0 -1 -1 v-3 z",
    /* v */ "M1 2 l2 5 h1 l2 -5 h-1 l-1.5 4 -1.5 -4 z",
    /* w */ "M1 2 l1 5 h1 l.5 -3 .5 3 h1 l1 -5 h-1 l-.5 3 -.5 -2 h-1 l-.5 2 -.5 -3 z",
    /* x */ "M1.5 2 l-.5 .5 2 2 -2 2 .5 .5 2 -2 2 2 .5 -.5 -2 -2 2 -2 -.5 -.5 -2 2 z",
    /* y */ "M1 2 v2 q0 2 2 2 h2 v.5 q0 .5 -.5 .5 h-3.5 v.5 q0 .5 .5 .5 h3.5 q1 0 1 -1 v-5 h-1 v3 h-2 q-1 0 -1 -1 v-2 z",
    /* z */ "M1 2 v1 h3 l-3 4 h5 v-1 h-3 l3 -4 z",
    /* { */ "M5 1 h-1 q-2 0 -2 2 0 .5 -.5 .5 -.5 0 -.5 .5 0 .5 .5 .5 .5 0 .5 .5 0 2 2 2 h1 v-1 h-1 q-1 0 -1 -1 v-2 q0 -1 1 -1 h1 z",
    /* | */ "M3 1 v6 h1 v-6 z",
    /* } */ "M2 1 v1 q1 0 1 1 v2 q0 1 -1 1 v1 q2 0 2 -2 0 -.5 .5 -.5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 -.5 0 -2 -2 -2",
    /* ~ */ "M1 4 q0 .5 .5 .5 .5 0 .5 -.5 h1 q0 1 1 1 h1 q1 0 1 -1 0 -.5 -.5 -.5 -.5 0 -.5 .5 h-1 q0 -1 -1 -1 h-1 q-1 0 -1 1",
};

static int atlas_create(struct text *text, struct text_builtin_atlas *atlas)
{
    int ret = 0;
    struct text_builtin *s = text->priv_data;
    struct path *path = NULL;

    atlas->distmap = ngli_distmap_create(text->ctx);
    if (!atlas->distmap) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    ret = ngli_distmap_init(atlas->distmap);
    if (ret < 0)
        goto end;

    path = ngli_path_create();
    if (!path)
        goto end;

    /*
     * The scale corresponds to how much we need to scale the characters so
     * that paths expressed in [view_w, view_h] scale ends up in the requested
     * [chr_w, chr_h] scale instead.
     */
    const float scale = (float)s->chr_h / (float)view_h;
    const NGLI_ALIGNED_MAT(transform) = {
        scale, 0.f, 0.f, 0.f,
        0.f, -scale, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, (float)s->chr_h, 0.f, 1.f,
    };

    for (size_t i = 0; i < NGLI_ARRAY_NB(outlines); i++) {
        ngli_path_clear(path);

        ret = ngli_path_add_svg_path(path, outlines[i]);
        if (ret < 0)
            goto end;

        ngli_path_transform(path, transform);

        ret = ngli_path_finalize(path);
        if (ret < 0)
            goto end;

        /* Register the glyph in the distmap atlas */
        uint32_t shape_id;
        ret = ngli_distmap_add_shape(atlas->distmap, s->chr_w, s->chr_h, path, NGLI_DISTMAP_FLAG_PATH_AUTO_CLOSE, &shape_id);
        if (ret < 0)
            goto end;

        /* Map the character codepoint to its shape ID in the atlas */
        atlas->char_map[FIRST_CHAR + i] = shape_id;
    }

    ret = ngli_distmap_finalize(atlas->distmap);
    if (ret < 0)
        goto end;

end:
    ngli_path_freep(&path);
    return ret;
}

static int text_builtin_init(struct text *text)
{
    struct text_builtin *s = text->priv_data;

    const uint32_t pt_size = (uint32_t)text->config.pt_size;
    const uint32_t res = (uint32_t)text->config.dpi;
    const uint32_t size = pt_size * res / 72;
    s->chr_w = size * view_w / view_h;
    s->chr_h = size;

    char atlas_uid[32];
    snprintf(atlas_uid, sizeof(atlas_uid), "%d", size);
    struct text_builtin_atlas *atlas = ngli_hmap_get_str(text->ctx->text_builtin_atlasses, atlas_uid);
    if (!atlas) {
        atlas = ngli_calloc(1, sizeof(*atlas));
        if (!atlas)
            return NGL_ERROR_MEMORY;

        int ret = atlas_create(text, atlas);
        if (ret < 0) {
            ngli_free_text_builtin_atlas(NULL, atlas);
            return ret;
        }

        ret = ngli_hmap_set_str(text->ctx->text_builtin_atlasses, atlas_uid, atlas);
        if (ret < 0) {
            ngli_free_text_builtin_atlas(NULL, atlas);
            return ret;
        }
    }

    s->atlas = atlas;

    text->atlas_texture = ngli_distmap_get_texture(atlas->distmap);

    return 0;
}

static void get_char_box_dim(const char *s, uint32_t *wp, uint32_t *hp, size_t *np)
{
    uint32_t w = 0, h = 1;
    uint32_t cur_w = 0;
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
    uint32_t text_cols, text_rows;
    get_char_box_dim(str, &text_cols, &text_rows, &text_nbchr);

    uint32_t col = 0, row = 0;

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

        const uint32_t atlas_id = s->atlas->char_map[(uint8_t)str[i]];
        int32_t atlas_coords[4];
        ngli_distmap_get_shape_coords(s->atlas->distmap, atlas_id, atlas_coords);

        float scale[2];
        ngli_distmap_get_shape_scale(s->atlas->distmap, atlas_id, scale);

        const struct char_info_internal chr = {
            .x = (int32_t)NGLI_I32_TO_I26D6(s->chr_w * col),
            .y = (int32_t)NGLI_I32_TO_I26D6(s->chr_h * (text_rows - row - 1)),
            .w = (int32_t)NGLI_I32_TO_I26D6(s->chr_w),
            .h = (int32_t)NGLI_I32_TO_I26D6(s->chr_h),
            .atlas_coords = {NGLI_ARG_VEC4(atlas_coords)},
            .scale = {NGLI_ARG_VEC2(scale)},
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

const struct text_cls ngli_text_builtin = {
    .init            = text_builtin_init,
    .set_string      = text_builtin_set_string,
    .priv_size       = sizeof(struct text_builtin),
    .flags           = 0,
};
