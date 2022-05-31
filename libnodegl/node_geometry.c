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
#include "log.h"
#include "nodegl.h"
#include "internal.h"
#include "topology.h"

static const struct param_choices topology_choices = {
    .name = "topology",
    .consts = {
        {"point_list",     NGLI_PRIMITIVE_TOPOLOGY_POINT_LIST,     .desc=NGLI_DOCSTRING("point list")},
        {"line_strip",     NGLI_PRIMITIVE_TOPOLOGY_LINE_STRIP,     .desc=NGLI_DOCSTRING("line strip")},
        {"line_list",      NGLI_PRIMITIVE_TOPOLOGY_LINE_LIST,      .desc=NGLI_DOCSTRING("line list")},
        {"triangle_strip", NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, .desc=NGLI_DOCSTRING("triangle strip")},
        {"triangle_list",  NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  .desc=NGLI_DOCSTRING("triangle list")},
        {NULL}
    }
};

#define TEXCOORDS_TYPES_LIST (const int[]){NGL_NODE_BUFFERFLOAT,            \
                                           NGL_NODE_BUFFERVEC2,             \
                                           NGL_NODE_BUFFERVEC3,             \
                                           NGL_NODE_ANIMATEDBUFFERFLOAT,    \
                                           NGL_NODE_ANIMATEDBUFFERVEC2,     \
                                           NGL_NODE_ANIMATEDBUFFERVEC3,     \
                                           -1}

struct geometry_opts {
    struct ngl_node *vertices;
    struct ngl_node *uvcoords;
    struct ngl_node *normals;
    struct ngl_node *indices;
    int topology;
};

struct geometry_priv {
    struct geometry *geom;
    struct ngl_node *update_nodes[3]; /* {vertices, uvcoords, normals} at most */
    int nb_update_nodes;
};

#define OFFSET(x) offsetof(struct geometry_opts, x)
static const struct node_param geometry_params[] = {
    {"vertices",  NGLI_PARAM_TYPE_NODE, OFFSET(vertices),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, -1},
                  .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("vertice coordinates defining the geometry")},
    {"uvcoords",  NGLI_PARAM_TYPE_NODE, OFFSET(uvcoords),
                  .node_types=TEXCOORDS_TYPES_LIST,
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("coordinates used for UV mapping of each `vertices`")},
    {"normals",   NGLI_PARAM_TYPE_NODE, OFFSET(normals),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, -1},
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("normal vectors of each `vertices`")},
    {"indices",   NGLI_PARAM_TYPE_NODE, OFFSET(indices),
                  .node_types=(const int[]){NGL_NODE_BUFFERUSHORT, NGL_NODE_BUFFERUINT, -1},
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("indices defining the drawing order of the `vertices`, auto-generated if not set")},
    {"topology",  NGLI_PARAM_TYPE_SELECT, OFFSET(topology), {.i32=NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
                  .choices=&topology_choices,
                  .desc=NGLI_DOCSTRING("primitive topology")},
    {NULL}
};

NGLI_STATIC_ASSERT(geom_on_top_of_geometry, offsetof(struct geometry_priv, geom) == 0);

#define GET_MAX_INDICES(type) do {                         \
    type *data = (type *)indices->data;                    \
    for (int i = 0; i < indices->layout.count; i++) {      \
        if (data[i] > max_indices)                         \
            max_indices = data[i];                         \
    }                                                      \
} while (0)                                                \

static int configure_buffer(struct ngl_node *buffer_node, int usage, struct buffer **bufferp, struct buffer_layout *layout)
{
    struct buffer_info *buffer_info = buffer_node->priv_data;

    int ret = ngli_node_buffer_ref(buffer_node);
    if (ret < 0)
        return ret;

    *bufferp = buffer_info->buffer;
    *layout = buffer_info->layout;
    ngli_node_buffer_extend_usage(buffer_node, usage);

    return 0;
}

static int geometry_init(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;
    const struct geometry_opts *o = node->opts;
    struct gpu_ctx *gpu_ctx = node->ctx->gpu_ctx;

    s->geom = ngli_geometry_create(gpu_ctx);
    if (!s->geom)
        return NGL_ERROR_MEMORY;

    struct buffer *buffer;
    struct buffer_layout layout;

    int ret = configure_buffer(o->vertices, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT, &buffer, &layout);
    if (ret < 0)
        return ret;
    ngli_geometry_set_vertices_buffer(s->geom, buffer, layout);
    s->update_nodes[s->nb_update_nodes++] = o->vertices;

    if (o->uvcoords) {
        ret = configure_buffer(o->uvcoords, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT, &buffer, &layout);
        if (ret < 0)
            return ret;
        ngli_geometry_set_uvcoords_buffer(s->geom, buffer, layout);
        s->update_nodes[s->nb_update_nodes++] = o->uvcoords;
    }

    if (o->normals) {
        ret = configure_buffer(o->normals, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT, &buffer, &layout);
        if (ret < 0)
            return ret;
        ngli_geometry_set_normals_buffer(s->geom, buffer, layout);
        s->update_nodes[s->nb_update_nodes++] = o->normals;
    }

    if (o->indices) {
        struct buffer_info *indices = o->indices->priv_data;
        if (indices->block) {
            LOG(ERROR, "geometry indices buffers referencing a block are not supported");
            return NGL_ERROR_UNSUPPORTED;
        }

        ret = configure_buffer(o->indices, NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT, &buffer, &layout);
        if (ret < 0)
            return ret;

        int64_t max_indices = 0;
        switch (layout.format) {
        case NGLI_FORMAT_R16_UNORM: GET_MAX_INDICES(uint16_t); break;
        case NGLI_FORMAT_R32_UINT:  GET_MAX_INDICES(uint32_t); break;
        default:
            ngli_assert(0);
        }

        ngli_geometry_set_indices_buffer(s->geom, buffer, layout, max_indices);
    }

    return ngli_geometry_init(s->geom, o->topology);
}

static int geometry_prepare(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;
    const struct geometry_opts *o = node->opts;

    /*
     * Init of buffers must happen after all usage flags are set (the usage of
     * a given buffer may differ according to how it is shared)
     */
    for (int i = 0; i < s->nb_update_nodes; i++) {
        struct ngl_node *update_node = s->update_nodes[i];
        int ret = ngli_node_buffer_init(update_node);
        if (ret < 0)
            return ret;
    }

    if (o->indices) {
        int ret = ngli_node_buffer_init(o->indices);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int geometry_update(struct ngl_node *node, double t)
{
    int ret;
    struct geometry_priv *s = node->priv_data;

    for (int i = 0; i < s->nb_update_nodes; i++) {
        struct ngl_node *update_node = s->update_nodes[i];
        if ((ret = ngli_node_update(update_node, t)) < 0 ||
            (ret = ngli_node_buffer_upload(update_node)) < 0)
            return ret;
    }

    return 0;
}

static void geometry_uninit(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;
    const struct geometry_opts *o = node->opts;

    ngli_geometry_freep(&s->geom);
    ngli_node_buffer_unref(o->vertices);
    if (o->uvcoords)
        ngli_node_buffer_unref(o->uvcoords);
    if (o->normals)
        ngli_node_buffer_unref(o->normals);
    if (o->indices)
        ngli_node_buffer_unref(o->indices);
}

const struct node_class ngli_geometry_class = {
    .id        = NGL_NODE_GEOMETRY,
    .name      = "Geometry",
    .init      = geometry_init,
    .prepare   = geometry_prepare,
    .uninit    = geometry_uninit,
    .update    = geometry_update,
    .opts_size = sizeof(struct geometry_opts),
    .priv_size = sizeof(struct geometry_priv),
    .params    = geometry_params,
    .file      = __FILE__,
};
