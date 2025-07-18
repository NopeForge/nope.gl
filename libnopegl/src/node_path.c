/*
 * Copyright 2020-2022 GoPro Inc.
 * Copyright 2020-2022 Clément Bœsch <u pkh.me>
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
#include <string.h>

#include "internal.h"
#include "node_pathkey.h"
#include "nopegl.h"
#include "path.h"

struct path_opts {
    struct ngl_node **keyframes;
    size_t nb_keyframes;
    int32_t precision;
};

struct path_priv {
    struct path *path;
};

#define OFFSET(x) offsetof(struct path_opts, x)
static const struct node_param path_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(keyframes),
                  .node_types=(const uint32_t[]){
                      NGL_NODE_PATHKEYMOVE,
                      NGL_NODE_PATHKEYLINE,
                      NGL_NODE_PATHKEYBEZIER2,
                      NGL_NODE_PATHKEYBEZIER3,
                      NGL_NODE_PATHKEYCLOSE,
                      NGLI_NODE_NONE
                  },
                  .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .desc=NGLI_DOCSTRING("anchor points the path go through")},
    {"precision", NGLI_PARAM_TYPE_I32, OFFSET(precision), {.i32=64},
                  .desc=NGLI_DOCSTRING("number of divisions per curve segment")},
    {NULL}
};

/* We must have the struct path in 1st position for AnimatedPath */
NGLI_STATIC_ASSERT(offsetof(struct path_priv, path) == 0, "path 1st field");

static int path_init(struct ngl_node *node)
{
    int ret;
    struct path_priv *s = node->priv_data;
    const struct path_opts *o = node->opts;

    s->path = ngli_path_create();
    if (!s->path)
        return NGL_ERROR_MEMORY;

    for (size_t i = 0; i < o->nb_keyframes; i++) {
        const struct ngl_node *kf = o->keyframes[i];
        if (kf->cls->id == NGL_NODE_PATHKEYMOVE) {
            const struct pathkey_move_opts *move = kf->opts;
            ret = ngli_path_move_to(s->path, move->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYLINE) {
            const struct pathkey_line_opts *line = kf->opts;
            ret = ngli_path_line_to(s->path, line->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYBEZIER2) {
            const struct pathkey_bezier2_opts *bezier2 = kf->opts;
            ret = ngli_path_bezier2_to(s->path, bezier2->control, bezier2->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYBEZIER3) {
            const struct pathkey_bezier3_opts *bezier3 = kf->opts;
            ret = ngli_path_bezier3_to(s->path, bezier3->control1, bezier3->control2, bezier3->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYCLOSE) {
            ret = ngli_path_close(s->path);
        } else {
            ngli_assert(0);
        }
        if (ret < 0)
            return ret;
    }

    ret = ngli_path_finalize(s->path);
    if (ret < 0)
        return ret;

    return ngli_path_init(s->path, o->precision);
}

static void path_uninit(struct ngl_node *node)
{
    struct path_priv *s = node->priv_data;
    ngli_path_freep(&s->path);
}

const struct node_class ngli_path_class = {
    .id        = NGL_NODE_PATH,
    .name      = "Path",
    .init      = path_init,
    .uninit    = path_uninit,
    .opts_size = sizeof(struct path_opts),
    .priv_size = sizeof(struct path_priv),
    .params    = path_params,
    .file      = __FILE__,
};
