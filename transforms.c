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

#include "log.h"
#include "nodegl.h"
#include "transforms.h"

float *ngli_get_last_transformation_matrix(struct ngl_node *node)
{
    struct ngl_node *child = NULL;
    struct ngl_node *current_node = node;

    while (current_node) {
        int id = current_node->class->id;
        if (id == NGL_NODE_ROTATE) {
            struct rotate *rotate = current_node->priv_data;
            child  = rotate->child;
        } else if (id == NGL_NODE_TRANSLATE) {
            struct translate *translate= current_node->priv_data;
            child  = translate->child;
        } else if (id == NGL_NODE_SCALE) {
            struct scale *scale= current_node->priv_data;
            child  = scale->child;
        } else if (id == NGL_NODE_IDENTITY) {
            return current_node->modelview_matrix;
        } else {
            LOG(ERROR, "%s (%s) is not an allowed type for a camera transformation",
                current_node->name, current_node->class->name);
            break;
        }

        current_node = child;
    }

    return NULL;
}
