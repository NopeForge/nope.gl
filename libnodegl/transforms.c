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

const float *ngli_get_last_transformation_matrix(const struct ngl_node *node)
{
    while (node) {
        const int id = node->class->id;
        switch (id) {
            case NGL_NODE_ROTATE:
            case NGL_NODE_SCALE:
            case NGL_NODE_TRANSFORM:
            case NGL_NODE_TRANSLATE: {
                const struct transform *trf = node->priv_data;
                node = trf->child;
                break;
            }
            case NGL_NODE_IDENTITY:
                return node->modelview_matrix;
            default:
                LOG(ERROR, "%s (%s) is not an allowed type for a camera transformation",
                    node->name, node->class->name);
                break;
        }
    }

    return NULL;
}

void ngli_transform_draw(struct ngl_node *node)
{
    struct transform *s = node->priv_data;
    struct ngl_node *child = s->child;
    ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, s->matrix);
    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    ngli_node_draw(child);
}
