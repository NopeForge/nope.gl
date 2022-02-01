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
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "type.h"
#include "utils.h"

#define OWN_VERTICES (1 << 0)
#define OWN_UVCOORDS (1 << 1)
#define OWN_NORMALS  (1 << 2)
#define OWN_INDICES  (1 << 3)

struct geometry *ngli_geometry_create(struct gpu_ctx *gpu_ctx)
{
    struct geometry *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->gpu_ctx = gpu_ctx;
    return s;
}

static int gen_buffer(struct geometry *s,
                      struct buffer **bufferp, const struct buffer_layout *layout,
                      const void *data, int usage)
{
    struct buffer *buffer = ngli_buffer_create(s->gpu_ctx);
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

static int gen_vec3(struct geometry *s,
                    struct buffer **bufferp, struct buffer_layout *layout,
                    int count, const float *data)
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
    return gen_buffer(s, bufferp, layout, data, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

int ngli_geometry_set_vertices(struct geometry *s, int n, const float *vertices)
{
    ngli_assert(!(s->buffer_ownership & OWN_VERTICES));
    s->buffer_ownership |= OWN_VERTICES;
    return gen_vec3(s, &s->vertices_buffer, &s->vertices_layout, n, vertices);
}

int ngli_geometry_set_normals(struct geometry *s, int n, const float *normals)
{
    ngli_assert(!(s->buffer_ownership & OWN_NORMALS));
    s->buffer_ownership |= OWN_NORMALS;
    return gen_vec3(s, &s->normals_buffer, &s->normals_layout, n, normals);
}

static int gen_vec2(struct geometry *s,
                    struct buffer **bufferp, struct buffer_layout *layout,
                    int count, const float *data)
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
    return gen_buffer(s, bufferp, layout, data, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

int ngli_geometry_set_uvcoords(struct geometry *s, int n, const float *uvcoords)
{
    ngli_assert(!(s->buffer_ownership & OWN_UVCOORDS));
    s->buffer_ownership |= OWN_UVCOORDS;
    return gen_vec2(s, &s->uvcoords_buffer, &s->uvcoords_layout, n, uvcoords);
}

int ngli_geometry_set_indices(struct geometry *s, int count, const uint16_t *indices)
{
    ngli_assert(!(s->buffer_ownership & OWN_INDICES));
    s->buffer_ownership |= OWN_INDICES;
    const int format = NGLI_FORMAT_R16_UNORM;
    s->indices_layout = (struct buffer_layout){
        .type   = NGLI_TYPE_NONE,
        .format = format,
        .stride = ngli_format_get_bytes_per_pixel(format),
        .comp   = ngli_format_get_nb_comp(format),
        .count  = count,
        .offset = 0,
    };
    for (int i = 0; i < count; i++)
        s->max_indices = NGLI_MAX(s->max_indices, indices[i]);
    return gen_buffer(s, &s->indices_buffer, &s->indices_layout, indices, NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void ngli_geometry_set_vertices_buffer(struct geometry *s, struct buffer *buffer, struct buffer_layout layout)
{
    ngli_assert(!(s->buffer_ownership & OWN_VERTICES));
    s->vertices_buffer = buffer;
    s->vertices_layout = layout;
}

void ngli_geometry_set_uvcoords_buffer(struct geometry *s, struct buffer *buffer, struct buffer_layout layout)
{
    ngli_assert(!(s->buffer_ownership & OWN_UVCOORDS));
    s->uvcoords_buffer = buffer;
    s->uvcoords_layout = layout;
}

void ngli_geometry_set_normals_buffer(struct geometry *s, struct buffer *buffer, struct buffer_layout layout)
{
    ngli_assert(!(s->buffer_ownership & OWN_NORMALS));
    s->normals_buffer = buffer;
    s->normals_layout = layout;
}

void ngli_geometry_set_indices_buffer(struct geometry *s, struct buffer *buffer,
                                      struct buffer_layout layout, int64_t max_indices)
{
    ngli_assert(!(s->buffer_ownership & OWN_INDICES));
    s->indices_buffer = buffer;
    s->indices_layout = layout;
    s->max_indices = max_indices;
}

int ngli_geometry_init(struct geometry *s, int topology)
{
    s->topology = topology;

    if (s->uvcoords_layout.count && s->uvcoords_layout.count != s->vertices_layout.count) {
        LOG(ERROR, "uvcoords count (%d) does not match vertices count (%d)", s->uvcoords_layout.count, s->vertices_layout.count);
        return NGL_ERROR_INVALID_ARG;
    }

    if (s->normals_layout.count && s->normals_layout.count != s->vertices_layout.count) {
        LOG(ERROR, "normals count (%d) does not match vertices count (%d)", s->normals_layout.count, s->vertices_layout.count);
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

void ngli_geometry_freep(struct geometry **sp)
{
    struct geometry *s = *sp;
    if (!s)
        return;
    if (s->buffer_ownership & OWN_VERTICES)
        ngli_buffer_freep(&s->vertices_buffer);
    if (s->buffer_ownership & OWN_UVCOORDS)
        ngli_buffer_freep(&s->uvcoords_buffer);
    if (s->buffer_ownership & OWN_NORMALS)
        ngli_buffer_freep(&s->normals_buffer);
    if (s->buffer_ownership & OWN_INDICES)
        ngli_buffer_freep(&s->indices_buffer);
    ngli_freep(&s);
}
