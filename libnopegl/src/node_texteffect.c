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

#include <stddef.h>

#include "log.h"
#include "internal.h"
#include "transforms.h"

static const struct param_choices target_choices = {
    .name = "text_target",
    .consts = {
        {"char",         NGLI_TEXT_EFFECT_CHAR,         .desc=NGLI_DOCSTRING("characters")},
        {"char_nospace", NGLI_TEXT_EFFECT_CHAR_NOSPACE, .desc=NGLI_DOCSTRING("characters (skipping whitespaces)")},
        {"word",         NGLI_TEXT_EFFECT_WORD,         .desc=NGLI_DOCSTRING("words")},
        {"line",         NGLI_TEXT_EFFECT_LINE,         .desc=NGLI_DOCSTRING("lines")},
        {"text",         NGLI_TEXT_EFFECT_TEXT,         .desc=NGLI_DOCSTRING("whole text")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct texteffect_opts, x)
static const struct node_param texteffect_params[] = {
    {"start",        NGLI_PARAM_TYPE_F64, OFFSET(start_time), {.f64=0.0},
                     .desc=NGLI_DOCSTRING("absolute start time of the effect")},
    {"end",          NGLI_PARAM_TYPE_F64, OFFSET(end_time), {.f64=5.0},
                     .desc=NGLI_DOCSTRING("absolute end time of the effect")},
    {"target",       NGLI_PARAM_TYPE_SELECT, OFFSET(target), {.i32=NGLI_TEXT_EFFECT_TEXT},
                     .choices=&target_choices,
                     .desc=NGLI_DOCSTRING("segmentation target of the effect")},
    {"random",       NGLI_PARAM_TYPE_BOOL, OFFSET(random),
                     .desc=NGLI_DOCSTRING("randomize the order the effect are applied on the target")},
    {"random_seed",  NGLI_PARAM_TYPE_U32, OFFSET(random_seed),
                     .desc=NGLI_DOCSTRING("random seed for the `random` parameter")},
    {"start_pos",    NGLI_PARAM_TYPE_F32, OFFSET(start_pos_node), {.f32=0.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("normalized text position where the effect starts")},
    {"end_pos",      NGLI_PARAM_TYPE_F32, OFFSET(end_pos_node), {.f32=1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("normalized text position where the effect ends")},
    {"overlap",      NGLI_PARAM_TYPE_F32, OFFSET(overlap_node),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("overlap factor between target elements")},
    {"transform",    NGLI_PARAM_TYPE_NODE, OFFSET(transform_chain), .node_types=TRANSFORM_TYPES_LIST,
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .desc=NGLI_DOCSTRING("transformation chain")},
    {"color",        NGLI_PARAM_TYPE_VEC3, OFFSET(color_node), {.vec={-1.f, -1.f, -1.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters fill color, use negative values for unchanged from previous text effects "
                                          "(default is `Text.fg_color`)")},
    {"opacity",      NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=-1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters opacity, use negative value for unchanged from previous text effects "
                                          "(default is `Text.opacity`)")},
    {"outline",      NGLI_PARAM_TYPE_F32, OFFSET(outline_node), {.f32=-1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters outline width, use negative value for unchanged from previous text effects "
                                          "(default is 0)")},
    {"outline_color", NGLI_PARAM_TYPE_VEC3, OFFSET(outline_color_node), {.vec={-1.f, -1.f, -1.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters outline color, use negative value for unchanged from previous text effects "
                                          "(default is yellow, (1, 1, 0))")},
    {"glow",         NGLI_PARAM_TYPE_F32, OFFSET(glow_node), {.f32=-1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters glow width, use negative value for unchanged from previous text effects "
                                          "(default is 0)")},
    {"glow_color",   NGLI_PARAM_TYPE_VEC3, OFFSET(glow_color_node), {.vec={-1.f, -1.f, -1.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters glow color, use negative value for unchanged from previous text effects "
                                          "(default is white, (1, 1, 1))")},
    {"blur",         NGLI_PARAM_TYPE_F32, OFFSET(blur_node), {.f32=-1.f},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("characters blur, use negative value for unchanged from previous text effects "
                                          "(default is 0)")},
    {NULL}
};

static int texteffect_init(struct ngl_node *node)
{
    const struct texteffect_opts *o = node->opts;

    if (o->start_time >= o->end_time) {
        LOG(ERROR, "end time must be strictly superior to start time");
        return NGL_ERROR_INVALID_ARG;
    }

    int ret = ngli_transform_chain_check(o->transform_chain);
    if (ret < 0)
        return ret;

    return 0;
}

const struct node_class ngli_texteffect_class = {
    .id        = NGL_NODE_TEXTEFFECT,
    .name      = "TextEffect",
    .init      = texteffect_init,
    .opts_size = sizeof(struct texteffect_opts),
    .params    = texteffect_params,
    .file      = __FILE__,
};
