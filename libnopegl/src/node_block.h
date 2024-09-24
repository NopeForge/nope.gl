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

#ifndef NODE_BLOCK_H
#define NODE_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#include "block.h"
#include "buffer.h"

struct ngl_node;

struct block_info {
    struct block block;

    uint8_t *data;
    size_t data_size;
    int usage;

    struct buffer *buffer;
    size_t buffer_rev;
};

void ngli_node_block_extend_usage(struct ngl_node *node, int usage);
size_t ngli_node_block_get_cpu_size(struct ngl_node *node);
size_t ngli_node_block_get_gpu_size(struct ngl_node *node);

#endif
