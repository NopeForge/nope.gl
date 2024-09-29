/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NODE_DRAW_H
#define NODE_DRAW_H

#include "aabb.h"
#include "gpu_ctx.h"

struct ngl_node;

struct draw_info {
    int compute_bounds;
    NGLI_ATTR_ALIGNED struct aabb aabb;

    struct gpu_viewport viewport;
    NGLI_ALIGNED_MAT(transform_matrix);

    NGLI_ATTR_ALIGNED struct aabb screen_aabb;
    NGLI_ATTR_ALIGNED struct obb2d screen_obb;
    int screen_obb_computed;
};

int ngli_node_compute_oriented_bounding_box(struct ngl_node *node);

#endif
