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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "geometry.h"
#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "node_buffer.h"
#include "nopegl.h"

static const struct param_choices topology_choices = {
    .name = "topology",
    .consts = {
        {"point_list",     NGPU_PRIMITIVE_TOPOLOGY_POINT_LIST,     .desc=NGLI_DOCSTRING("point list")},
        {"line_strip",     NGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP,     .desc=NGLI_DOCSTRING("line strip")},
        {"line_list",      NGPU_PRIMITIVE_TOPOLOGY_LINE_LIST,      .desc=NGLI_DOCSTRING("line list")},
        {"triangle_strip", NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, .desc=NGLI_DOCSTRING("triangle strip")},
        {"triangle_list",  NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  .desc=NGLI_DOCSTRING("triangle list")},
        {NULL}
    }
};

#define TEXCOORDS_TYPES_LIST (const uint32_t[]){NGL_NODE_BUFFERFLOAT,            \
                                                NGL_NODE_BUFFERVEC2,             \
                                                NGL_NODE_BUFFERVEC3,             \
                                                NGL_NODE_ANIMATEDBUFFERFLOAT,    \
                                                NGL_NODE_ANIMATEDBUFFERVEC2,     \
                                                NGL_NODE_ANIMATEDBUFFERVEC3,     \
                                                NGLI_NODE_NONE}

struct geometry_opts {
    struct ngl_node *vertices;
    struct ngl_node *uvcoords;
    struct ngl_node *normals;
    struct ngl_node *indices;
    enum ngpu_primitive_topology topology;
};

struct geometry_priv {
    struct geometry *geom;
};

#define OFFSET(x) offsetof(struct geometry_opts, x)
static const struct node_param geometry_params[] = {
    {"vertices",  NGLI_PARAM_TYPE_NODE, OFFSET(vertices),
                  .node_types=(const uint32_t[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, NGLI_NODE_NONE},
                  .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("vertice coordinates defining the geometry")},
    {"uvcoords",  NGLI_PARAM_TYPE_NODE, OFFSET(uvcoords),
                  .node_types=TEXCOORDS_TYPES_LIST,
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("coordinates used for UV mapping of each `vertices`")},
    {"normals",   NGLI_PARAM_TYPE_NODE, OFFSET(normals),
                  .node_types=(const uint32_t[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, NGLI_NODE_NONE},
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("normal vectors of each `vertices`")},
    {"indices",   NGLI_PARAM_TYPE_NODE, OFFSET(indices),
                  .node_types=(const uint32_t[]){NGL_NODE_BUFFERUSHORT, NGL_NODE_BUFFERUINT, NGLI_NODE_NONE},
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("indices defining the drawing order of the `vertices`, auto-generated if not set")},
    {"topology",  NGLI_PARAM_TYPE_SELECT, OFFSET(topology), {.i32=NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
                  .choices=&topology_choices,
                  .desc=NGLI_DOCSTRING("primitive topology")},
    {NULL}
};

NGLI_STATIC_ASSERT(geom_on_top_of_geometry, offsetof(struct geometry_priv, geom) == 0);

#define GET_MAX_INDICES(type) do {                         \
    type *data = (type *)indices->data;                    \
    for (size_t i = 0; i < indices->layout.count; i++) {   \
        if (data[i] > max_indices)                         \
            max_indices = data[i];                         \
    }                                                      \
} while (0)                                                \

static int geometry_init(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;
    const struct geometry_opts *o = node->opts;
    struct ngpu_ctx *gpu_ctx = node->ctx->gpu_ctx;

    s->geom = ngli_geometry_create(gpu_ctx);
    if (!s->geom)
        return NGL_ERROR_MEMORY;

    struct buffer_info *vertices = o->vertices->priv_data;
    ngli_geometry_set_vertices_buffer(s->geom, vertices->buffer, vertices->layout);
    ngli_node_buffer_extend_usage(o->vertices, NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    vertices->flags |= NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD;

    if (o->uvcoords) {
        struct buffer_info *uvcoords = o->uvcoords->priv_data;
        ngli_geometry_set_uvcoords_buffer(s->geom, uvcoords->buffer, uvcoords->layout);
        ngli_node_buffer_extend_usage(o->uvcoords, NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        uvcoords->flags |= NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD;
    }

    if (o->normals) {
        struct buffer_info *normals = o->normals->priv_data;
        ngli_geometry_set_normals_buffer(s->geom, normals->buffer, normals->layout);
        ngli_node_buffer_extend_usage(o->normals, NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        normals->flags |= NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD;
    }

    if (o->indices) {
        struct buffer_info *indices = o->indices->priv_data;
        if (indices->block) {
            LOG(ERROR, "geometry indices buffers referencing a block are not supported");
            return NGL_ERROR_UNSUPPORTED;
        }

        ngli_node_buffer_extend_usage(o->indices, NGPU_BUFFER_USAGE_INDEX_BUFFER_BIT);
        indices->flags |= NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD;

        int64_t max_indices = 0;
        switch (indices->layout.format) {
        case NGPU_FORMAT_R16_UNORM: GET_MAX_INDICES(uint16_t); break;
        case NGPU_FORMAT_R32_UINT:  GET_MAX_INDICES(uint32_t); break;
        default:
            ngli_assert(0);
        }

        ngli_geometry_set_indices_buffer(s->geom, indices->buffer, indices->layout, max_indices);
    }

    return ngli_geometry_init(s->geom, o->topology);
}

static void geometry_uninit(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;

    ngli_geometry_freep(&s->geom);
}

const struct node_class ngli_geometry_class = {
    .id        = NGL_NODE_GEOMETRY,
    .name      = "Geometry",
    .init      = geometry_init,
    .prepare   = ngli_node_prepare_children,
    .uninit    = geometry_uninit,
    .update    = ngli_node_update_children,
    .opts_size = sizeof(struct geometry_opts),
    .priv_size = sizeof(struct geometry_priv),
    .params    = geometry_params,
    .file      = __FILE__,
};
