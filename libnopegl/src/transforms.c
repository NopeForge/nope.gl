/*
 * Copyright 2016-2022 GoPro Inc.
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
#include "log.h"
#include "nopegl.h"
#include "math_utils.h"
#include "node_transform.h"
#include "transforms.h"

const struct ngl_node *ngli_transform_get_leaf_node(const struct ngl_node *node)
{
    while (node && node->cls->category == NGLI_NODE_CATEGORY_TRANSFORM) {
        const struct transform *transform = node->priv_data;
        node = transform->child;
    }
    return node;
}

int ngli_transform_chain_check(const struct ngl_node *node)
{
    if (!node) // it is ok for the transform chain not to be set
        return 0;

    const struct ngl_node *leaf = ngli_transform_get_leaf_node(node);

    // All transform nodes are expected to have a non-null child parameter
    ngli_assert(leaf);

    if (leaf->cls->id != NGL_NODE_IDENTITY) {
        LOG(ERROR, "%s (%s) is not an allowed type for a transformation chain",
            node->label, node->cls->name);
        return NGL_ERROR_INVALID_USAGE;
    }

    return 0;
}

void ngli_transform_chain_compute(const struct ngl_node *node, float *matrix)
{
    NGLI_ALIGNED_MAT(tmp) = NGLI_MAT4_IDENTITY;
    while (node && node->cls->category == NGLI_NODE_CATEGORY_TRANSFORM) {
        const struct transform *transform = node->priv_data;
        ngli_mat4_mul(tmp, tmp, transform->matrix);
        node = transform->child;
    }
    memcpy(matrix, tmp, sizeof(tmp));
}

void ngli_transform_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct transform *s = node->priv_data;
    struct ngl_node *child = s->child;

    float *next_matrix = ngli_darray_push(&ctx->modelview_matrix_stack, NULL);
    if (!next_matrix)
        return;

    /* We cannot use ngli_darray_tail() before calling ngli_darray_push() as
     * ngli_darray_push() can potentially perform a re-allocation on the
     * underlying matrix stack buffer */
    const float *prev_matrix = next_matrix - 4 * 4;

    ngli_mat4_mul(next_matrix, prev_matrix, s->matrix);
    ngli_node_draw(child);
    ngli_darray_pop(&ctx->modelview_matrix_stack);
}
