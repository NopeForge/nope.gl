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

#ifndef NODE_BUFFER_H
#define NODE_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "buffer_layout.h"

struct ngl_node;

#define NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD (1 << 0) /* The gpu_buffer is responsible for uploading its data to the GPU */
#define NGLI_BUFFER_INFO_FLAG_DYNAMIC    (1 << 1) /* The gpu_buffer CPU data may change at every update */

struct buffer_info {
    struct buffer_layout layout;

    uint8_t *data;          // buffer of <count> elements
    size_t data_size;       // total buffer data size in bytes

    struct ngl_node *block;
    int usage;              // flags defining buffer use

    uint32_t flags;

    struct gpu_buffer *buffer;
};

void ngli_node_buffer_extend_usage(struct ngl_node *node, int usage);
size_t ngli_node_buffer_get_cpu_size(struct ngl_node *node);
size_t ngli_node_buffer_get_gpu_size(struct ngl_node *node);

#endif
