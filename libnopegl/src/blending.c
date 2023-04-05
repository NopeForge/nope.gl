/*
 * Copyright 2021-2022 GoPro Inc.
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

#include "nopegl.h"
#include "blending.h"
#include "params.h"

/*
 * These blending modes come from "Compositing Digital Images", July 1984,
 * by Thomas Porter and Tom Duff
 */
static const struct {
    int srcf, dstf;
} blend_factors[] = {
    [NGLI_BLENDING_SRC_OVER] = {NGLI_BLEND_FACTOR_ONE,                 NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
    [NGLI_BLENDING_DST_OVER] = {NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, NGLI_BLEND_FACTOR_ONE},
    [NGLI_BLENDING_SRC_OUT]  = {NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, NGLI_BLEND_FACTOR_ZERO},
    [NGLI_BLENDING_DST_OUT]  = {NGLI_BLEND_FACTOR_ZERO,                NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
    [NGLI_BLENDING_SRC_IN]   = {NGLI_BLEND_FACTOR_DST_ALPHA,           NGLI_BLEND_FACTOR_ZERO},
    [NGLI_BLENDING_DST_IN]   = {NGLI_BLEND_FACTOR_ZERO,                NGLI_BLEND_FACTOR_SRC_ALPHA},
    [NGLI_BLENDING_SRC_ATOP] = {NGLI_BLEND_FACTOR_DST_ALPHA,           NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
    [NGLI_BLENDING_DST_ATOP] = {NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, NGLI_BLEND_FACTOR_SRC_ALPHA},
    [NGLI_BLENDING_XOR]      = {NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
};

const struct param_choices ngli_blending_choices = {
    .name = "blend_preset",
    .consts = {
        {"default",  NGLI_BLENDING_DEFAULT,  .desc=NGLI_DOCSTRING("unchanged current graphics state")},
        {"src_over", NGLI_BLENDING_SRC_OVER, .desc=NGLI_DOCSTRING("this node over destination")},
        {"dst_over", NGLI_BLENDING_DST_OVER, .desc=NGLI_DOCSTRING("destination over this node")},
        {"src_out",  NGLI_BLENDING_SRC_OUT,  .desc=NGLI_DOCSTRING("subtract destination from this node")},
        {"dst_out",  NGLI_BLENDING_DST_OUT,  .desc=NGLI_DOCSTRING("subtract this node from destination")},
        {"src_in",   NGLI_BLENDING_SRC_IN,   .desc=NGLI_DOCSTRING("keep only the part of this node overlapping with destination")},
        {"dst_in",   NGLI_BLENDING_DST_IN,   .desc=NGLI_DOCSTRING("keep only the part of destination overlapping with this node")},
        {"src_atop", NGLI_BLENDING_SRC_ATOP, .desc=NGLI_DOCSTRING("union of `src_in` and `dst_out`")},
        {"dst_atop", NGLI_BLENDING_DST_ATOP, .desc=NGLI_DOCSTRING("union of `src_out` and `dst_in`")},
        {"xor",      NGLI_BLENDING_XOR,      .desc=NGLI_DOCSTRING("exclusive or between this node and the destination")},
        {NULL}
    }
};

int ngli_blending_apply_preset(struct graphicstate *state, int preset)
{
    if (preset == NGLI_BLENDING_DEFAULT)
        return 0;
    if (preset < 0 || preset >= NGLI_ARRAY_NB(blend_factors))
        return NGL_ERROR_INVALID_ARG;
    const int srcf = blend_factors[preset].srcf;
    const int dstf = blend_factors[preset].dstf;
    state->blend = 1;
    state->blend_src_factor   = srcf;
    state->blend_dst_factor   = dstf;
    state->blend_src_factor_a = srcf;
    state->blend_dst_factor_a = dstf;
    return 0;
}
