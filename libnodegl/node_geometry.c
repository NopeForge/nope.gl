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

struct geometry_priv {
    struct geometry geom;
    struct ngl_node *vertices;
    struct ngl_node *uvcoords;
    struct ngl_node *normals;
    struct ngl_node *indices;
    struct ngl_node *update_nodes[3]; /* {vertices, uvcoords, normals} at most */
    int nb_update_nodes;
    int topology;
};

#define OFFSET(x) offsetof(struct geometry_priv, x)
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
        if (data[i] > s->geom.max_indices)                 \
            s->geom.max_indices = data[i];                 \
    }                                                      \
} while (0)                                                \

static int configure_buffer(struct ngl_node *buffer_node, int usage, struct buffer **bufferp, struct buffer_layout *layout)
{
    struct buffer_priv *buffer_priv = buffer_node->priv_data;

    int ret = ngli_node_buffer_ref(buffer_node);
    if (ret < 0)
        return ret;

    if (buffer_priv->block) {
        struct ngl_node *block_node = buffer_priv->block;
        struct block_priv *block_priv = block_node->priv_data;
        const struct block *block = &block_priv->block;
        const struct block_field *fields = ngli_darray_data(&block->fields);
        const struct block_field *fi = &fields[buffer_priv->block_field];

        *bufferp = block_priv->buffer;
        *layout = buffer_priv->layout;
        layout->stride = fi->stride;
        layout->offset = fi->offset;

        block_priv->usage |= usage;
    } else {
        *bufferp = buffer_priv->buffer;
        *layout = buffer_priv->layout;

        buffer_priv->usage |= usage;
    }

    return 0;
}

static int geometry_init(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;

    int ret = configure_buffer(s->vertices, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT, &s->geom.vertices_buffer, &s->geom.vertices_layout);
    if (ret < 0)
        return ret;
    s->update_nodes[s->nb_update_nodes++] = s->vertices;

    struct buffer_priv *vertices = s->vertices->priv_data;

    if (s->uvcoords) {
        struct buffer_priv *uvcoords = s->uvcoords->priv_data;
        if (uvcoords->layout.count != vertices->layout.count) {
            LOG(ERROR,
                "uvcoords count (%d) does not match vertices count (%d)",
                uvcoords->layout.count,
                vertices->layout.count);
            return NGL_ERROR_INVALID_ARG;
        }

        ret = configure_buffer(s->uvcoords, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT, &s->geom.uvcoords_buffer, &s->geom.uvcoords_layout);
        if (ret < 0)
            return ret;
        s->update_nodes[s->nb_update_nodes++] = s->uvcoords;
    }

    if (s->normals) {
        struct buffer_priv *normals = s->normals->priv_data;
        if (normals->layout.count != vertices->layout.count) {
            LOG(ERROR,
                "normals count (%d) does not match vertices count (%d)",
                normals->layout.count,
                vertices->layout.count);
            return NGL_ERROR_INVALID_ARG;
        }

        ret = configure_buffer(s->normals, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT, &s->geom.normals_buffer, &s->geom.normals_layout);
        if (ret < 0)
            return ret;
        s->update_nodes[s->nb_update_nodes++] = s->normals;
    }

    if (s->indices) {
        struct buffer_priv *indices = s->indices->priv_data;
        if (indices->block) {
            LOG(ERROR, "geometry indices buffers referencing a block are not supported");
            return NGL_ERROR_UNSUPPORTED;
        }

        switch (indices->layout.format) {
        case NGLI_FORMAT_R16_UNORM: GET_MAX_INDICES(uint16_t); break;
        case NGLI_FORMAT_R32_UINT:  GET_MAX_INDICES(uint32_t); break;
        default:
            ngli_assert(0);
        }

        ret = configure_buffer(s->indices, NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT, &s->geom.indices_buffer, &s->geom.indices_layout);
        if (ret < 0)
            return ret;
    }

    s->geom.topology = s->topology;

    return 0;
}

static int geometry_prepare(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;

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

    if (s->indices) {
        int ret = ngli_node_buffer_init(s->indices);
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

    ngli_node_buffer_unref(s->vertices);
    if (s->uvcoords)
        ngli_node_buffer_unref(s->uvcoords);
    if (s->normals)
        ngli_node_buffer_unref(s->normals);
    if (s->indices)
        ngli_node_buffer_unref(s->indices);
}

const struct node_class ngli_geometry_class = {
    .id        = NGL_NODE_GEOMETRY,
    .name      = "Geometry",
    .init      = geometry_init,
    .prepare   = geometry_prepare,
    .uninit    = geometry_uninit,
    .update    = geometry_update,
    .priv_size = sizeof(struct geometry_priv),
    .params    = geometry_params,
    .file      = __FILE__,
};
