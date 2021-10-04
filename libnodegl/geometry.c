/*
 * Copyright 2021 GoPro Inc.
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

#include "format.h"
#include "geometry.h"
#include "nodegl.h"
#include "type.h"

static int geometry_gen_buffer(struct buffer **bufferp, const struct buffer_layout *layout,
                               struct gpu_ctx *gpu_ctx, const void *data, int usage)
{
    struct buffer *buffer = ngli_buffer_create(gpu_ctx);
    if (!buffer)
        return NGL_ERROR_MEMORY;

    const int size = layout->count * layout->stride;

    int ret = ngli_buffer_init(buffer, size, NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | usage);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(buffer, data, size, layout->offset);
    if (ret < 0)
        return ret;

    *bufferp = buffer;
    return 0;
}

int ngli_geometry_gen_vec3(struct buffer **bufferp, struct buffer_layout *layout,
                           struct gpu_ctx *gpu_ctx, int count, const void *data)
{
    const int format = NGLI_FORMAT_R32G32B32_SFLOAT;
    *layout = (struct buffer_layout){
        .type   = NGLI_TYPE_VEC3,
        .format = format,
        .stride = ngli_format_get_bytes_per_pixel(format),
        .comp   = ngli_format_get_nb_comp(format),
        .count  = count,
        .offset = 0,
    };
    return geometry_gen_buffer(bufferp, layout, gpu_ctx, data, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

int ngli_geometry_gen_vec2(struct buffer **bufferp, struct buffer_layout *layout,
                           struct gpu_ctx *gpu_ctx, int count, const void *data)
{
    const int format = NGLI_FORMAT_R32G32_SFLOAT;
    *layout = (struct buffer_layout){
        .type   = NGLI_TYPE_VEC2,
        .format = format,
        .stride = ngli_format_get_bytes_per_pixel(format),
        .comp   = ngli_format_get_nb_comp(format),
        .count  = count,
        .offset = 0,
    };
    return geometry_gen_buffer(bufferp, layout, gpu_ctx, data, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

int ngli_geometry_gen_indices(struct buffer **bufferp, struct buffer_layout *layout,
                              struct gpu_ctx *gpu_ctx, int count, const void *data)
{
    const int format = NGLI_FORMAT_R16_UNORM;
    *layout = (struct buffer_layout){
        .type   = NGLI_TYPE_NONE,
        .format = format,
        .stride = ngli_format_get_bytes_per_pixel(format),
        .comp   = ngli_format_get_nb_comp(format),
        .count  = count,
        .offset = 0,
    };
    return geometry_gen_buffer(bufferp, layout, gpu_ctx, data, NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT);
}
