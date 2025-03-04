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

#ifndef NODE_UNIFORM_H
#define NODE_UNIFORM_H

#include <stdint.h>

#include "internal.h"

struct variable_opts {
    struct livectl live;

    struct ngl_node **animkf;
    size_t nb_animkf;

    union {
        struct ngl_node *path_node; /* AnimatedPath only */
        struct ngl_node *transform; /* UniformMat4 only */
        int as_mat4; /* UniformQuat and AnimatedQuat only */
        int space; /* UniformColor and AnimatedColor only */
    };

    double time_offset;
};

struct variable_info {
    void *data;
    size_t data_size;
    enum ngpu_type data_type;          // any of NGPU_TYPE_*
    int dynamic;
};

void *ngli_node_get_data_ptr(const struct ngl_node *var_node, void *data_fallback);

#endif
