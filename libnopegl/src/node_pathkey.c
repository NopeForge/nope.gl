/*
 * Copyright 2021-2022 GoPro Inc.
 * Copyright 2021-2022 Clément Bœsch <u pkh.me>
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

#include "internal.h"
#include "node_pathkey.h"
#include "nopegl.h"
#include "utils/bstr.h"

#define OFFSET_MOVE(x) offsetof(struct pathkey_move_opts, x)
static const struct node_param pathkey_move_params[] = {
    {"to", NGLI_PARAM_TYPE_VEC3, OFFSET_MOVE(to),
           .desc=NGLI_DOCSTRING("new cursor position")},
    {NULL}
};

#define OFFSET_LINE(x) offsetof(struct pathkey_line_opts, x)
static const struct node_param pathkey_line_params[] = {
    {"to", NGLI_PARAM_TYPE_VEC3, OFFSET_LINE(to),
           .desc=NGLI_DOCSTRING("end point of the line, new cursor position")},
    {NULL}
};

#define OFFSET_BEZIER2(x) offsetof(struct pathkey_bezier2_opts, x)
static const struct node_param pathkey_bezier2_params[] = {
    {"control", NGLI_PARAM_TYPE_VEC3, OFFSET_BEZIER2(control),
                .desc=NGLI_DOCSTRING("control point")},
    {"to", NGLI_PARAM_TYPE_VEC3, OFFSET_BEZIER2(to),
           .desc=NGLI_DOCSTRING("end point of the curve, new cursor position")},
    {NULL}
};

#define OFFSET_BEZIER3(x) offsetof(struct pathkey_bezier3_opts, x)
static const struct node_param pathkey_bezier3_params[] = {
    {"control1", NGLI_PARAM_TYPE_VEC3, OFFSET_BEZIER3(control1),
                 .desc=NGLI_DOCSTRING("first control point")},
    {"control2", NGLI_PARAM_TYPE_VEC3, OFFSET_BEZIER3(control2),
                 .desc=NGLI_DOCSTRING("second control point")},
    {"to",       NGLI_PARAM_TYPE_VEC3, OFFSET_BEZIER3(to),
                 .desc=NGLI_DOCSTRING("end point of the curve, new cursor position")},
    {NULL}
};

static char *pathkey_info_str(const struct ngl_node *node)
{
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;
    if (node->cls->id == NGL_NODE_PATHKEYMOVE) {
        const struct pathkey_move_opts *o = node->opts;
        ngli_bstr_printf(b, "move to:%g,%g,%g", NGLI_ARG_VEC3(o->to));
    } else if (node->cls->id == NGL_NODE_PATHKEYLINE) {
        const struct pathkey_line_opts *o = node->opts;
        ngli_bstr_printf(b, "line to:%g,%g,%g", NGLI_ARG_VEC3(o->to));
    } else if (node->cls->id == NGL_NODE_PATHKEYBEZIER2) {
        const struct pathkey_bezier2_opts *o = node->opts;
        ngli_bstr_printf(b, "bezier2 ctl:%g,%g,%g to:%g,%g,%g",
                         NGLI_ARG_VEC3(o->control), NGLI_ARG_VEC3(o->to));
    } else if (node->cls->id == NGL_NODE_PATHKEYBEZIER3) {
        const struct pathkey_bezier3_opts *o = node->opts;
        ngli_bstr_printf(b, "bezier3 ctl1:%g,%g,%g ctl2:%g,%g,%g to:%g,%g,%g",
                         NGLI_ARG_VEC3(o->control1), NGLI_ARG_VEC3(o->control2), NGLI_ARG_VEC3(o->to));
    } else if (node->cls->id == NGL_NODE_PATHKEYCLOSE) {
        ngli_bstr_print(b, "close sub-path");
    } else {
        ngli_assert(0);
    }
    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

const struct node_class ngli_pathkeymove_class = {
    .id        = NGL_NODE_PATHKEYMOVE,
    .name      = "PathKeyMove",
    .info_str  = pathkey_info_str,
    .opts_size = sizeof(struct pathkey_move_opts),
    .params    = pathkey_move_params,
    .file      = __FILE__,
};

const struct node_class ngli_pathkeyline_class = {
    .id        = NGL_NODE_PATHKEYLINE,
    .name      = "PathKeyLine",
    .info_str  = pathkey_info_str,
    .opts_size = sizeof(struct pathkey_line_opts),
    .params    = pathkey_line_params,
    .file      = __FILE__,
};

const struct node_class ngli_pathkeybezier2_class = {
    .id        = NGL_NODE_PATHKEYBEZIER2,
    .name      = "PathKeyBezier2",
    .info_str  = pathkey_info_str,
    .opts_size = sizeof(struct pathkey_bezier2_opts),
    .params    = pathkey_bezier2_params,
    .file      = __FILE__,
};

const struct node_class ngli_pathkeybezier3_class = {
    .id        = NGL_NODE_PATHKEYBEZIER3,
    .name      = "PathKeyBezier3",
    .info_str  = pathkey_info_str,
    .opts_size = sizeof(struct pathkey_bezier3_opts),
    .params    = pathkey_bezier3_params,
    .file      = __FILE__,
};

const struct node_class ngli_pathkeyclose_class = {
    .id        = NGL_NODE_PATHKEYCLOSE,
    .name      = "PathKeyClose",
    .info_str  = pathkey_info_str,
    .file      = __FILE__,
};
