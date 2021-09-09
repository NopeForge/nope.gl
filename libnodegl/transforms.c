/*
 * Copyright 2016 GoPro Inc.
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
#include "nodegl.h"
#include "math_utils.h"
#include "transforms.h"

int ngli_transform_chain_check(const struct ngl_node *node)
{
    while (node) {
        const int id = node->cls->id;
        switch (id) {
            case NGL_NODE_ROTATE:
            case NGL_NODE_ROTATEQUAT:
            case NGL_NODE_SCALE:
            case NGL_NODE_SKEW:
            case NGL_NODE_TRANSFORM:
            case NGL_NODE_TRANSLATE: {
                const struct transform_priv *trf = node->priv_data;
                node = trf->child;
                break;
            }
            case NGL_NODE_IDENTITY:
                return 0;
            default:
                LOG(ERROR, "%s (%s) is not an allowed type for a camera transformation",
                    node->label, node->cls->name);
                return NGL_ERROR_INVALID_USAGE;
        }
    }

    return 0;
}

void ngli_transform_chain_compute(const struct ngl_node *node, float *matrix)
{
    NGLI_ALIGNED_MAT(tmp) = NGLI_MAT4_IDENTITY;
    while (node && node->cls->id != NGL_NODE_IDENTITY) {
        const struct transform_priv *transform_priv = node->priv_data;
        ngli_mat4_mul(tmp, tmp, transform_priv->matrix);
        node = transform_priv->child;
    }
    memcpy(matrix, tmp, sizeof(tmp));
}

void ngli_transform_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct transform_priv *s = node->priv_data;
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
