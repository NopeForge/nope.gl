/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "log.h"
#include "node_transform.h"
#include "nopegl.h"
#include "utils/darray.h"
#include "transforms.h"
#include "utils/utils.h"

struct gridlayout_opts {
    struct ngl_node **children;
    size_t nb_children;
    int32_t size[2];
};

struct gridlayout_priv {
    struct transform trf;
    struct darray matrices; // float[4*4]
};

#define OFFSET(x) offsetof(struct gridlayout_opts, x)
static const struct node_param gridlayout_params[] = {
    {"children", NGLI_PARAM_TYPE_NODELIST, OFFSET(children),
                 .desc=NGLI_DOCSTRING("a set of scenes")},
    {"size",     NGLI_PARAM_TYPE_IVEC2, OFFSET(size), {.ivec={-1, -1}},
                 .desc=NGLI_DOCSTRING("number of cols and rows in the grid")},
    {NULL}
};

static int gridlayout_init(struct ngl_node *node)
{
    struct gridlayout_priv *s = node->priv_data;
    const struct gridlayout_opts *o = node->opts;

    if (o->nb_children > 1U << 24)
        return NGL_ERROR_LIMIT_EXCEEDED;

    const float n = (float)o->nb_children;

    int32_t cols = o->size[0];
    int32_t rows = o->size[1];
    if ((int32_t)o->nb_children > rows * cols) {
        LOG(ERROR, "the number of specified children (%zu) does not fit in the requested %dx%d grid",
            o->nb_children, rows, cols);
        return NGL_ERROR_INVALID_ARG;
    }

    ngli_assert((int32_t)n <= rows * cols);

    ngli_darray_init(&s->matrices, sizeof(float[4 * 4]), NGLI_DARRAY_FLAG_ALIGNED);

    const float scale_x = 1.f / (float)cols;
    const float scale_y = 1.f / (float)rows;

    size_t i = 0;
    for (int32_t row = 0; row < rows; row++) {
        for (int32_t col = 0; col < cols; col++) {
            const float pos_x = scale_x * ((float)col *  2.f + 1.f) - 1.f;
            const float pos_y = scale_y * ((float)row * -2.f - 1.f) + 1.f;
            // This is equivalent to Translate(Scale(node))
            const NGLI_ALIGNED_MAT(matrix) = {
                scale_x, 0, 0, 0,
                0, scale_y, 0, 0,
                0, 0, 1, 0,
                pos_x, pos_y, 0, 1,
            };
            if (!ngli_darray_push(&s->matrices, matrix))
                return NGL_ERROR_MEMORY;
            if (++i == o->nb_children)
                return 0;
        }
    }

    return 0;
}

static int gridlayout_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    const struct gridlayout_opts *o = node->opts;

    int ret = 0;
    struct rnode *rnode_pos = ctx->rnode_pos;
    for (size_t i = 0; i < o->nb_children; i++) {
        struct rnode *rnode = ngli_rnode_add_child(rnode_pos);
        if (!rnode)
            return NGL_ERROR_MEMORY;
        ctx->rnode_pos = rnode;

        struct ngl_node *child = o->children[i];
        ret = ngli_node_prepare(child);
        if (ret < 0)
            goto done;
    }

done:
    ctx->rnode_pos = rnode_pos;
    return ret;
}

static void gridlayout_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gridlayout_priv *s = node->priv_data;
    const struct gridlayout_opts *o = node->opts;

    struct rnode *rnode_pos = ctx->rnode_pos;
    struct rnode *rnodes = ngli_darray_data(&rnode_pos->children);

    const float *matrices = ngli_darray_data(&s->matrices);
    for (size_t i = 0; i < o->nb_children; i++) {
        ctx->rnode_pos = &rnodes[i];
        s->trf.child = o->children[i];

        const float *matrix = &matrices[i * 4 * 4];
        memcpy(s->trf.matrix, matrix, sizeof(s->trf.matrix));

        ngli_transform_draw(node);
    }

    ctx->rnode_pos = rnode_pos;
}

static void gridlayout_uninit(struct ngl_node *node)
{
    struct gridlayout_priv *s = node->priv_data;
    ngli_darray_reset(&s->matrices);
}

const struct node_class ngli_gridlayout_class = {
    .id        = NGL_NODE_GRIDLAYOUT,
    .name      = "GridLayout",
    .init      = gridlayout_init,
    .prepare   = gridlayout_prepare,
    .update    = ngli_node_update_children,
    .draw      = gridlayout_draw,
    .uninit    = gridlayout_uninit,
    .opts_size = sizeof(struct gridlayout_opts),
    .priv_size = sizeof(struct gridlayout_priv),
    .params    = gridlayout_params,
    .file      = __FILE__,
};
