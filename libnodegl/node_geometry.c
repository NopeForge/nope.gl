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

#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define SET_INDICES(type, count, data) do {                                    \
    type *indices = (type *)(data);                                            \
    for (int i = 0; i < (count); i++) {                                        \
        indices[i] = i;                                                        \
    }                                                                          \
} while (0)

struct ngl_node *ngli_geometry_generate_buffer(struct ngl_ctx *ctx, int type, int count, int size, void *data)
{
    struct ngl_node *node = ngl_node_create(type, count);
    if (!node)
        return NULL;

    if (data)
        ngl_node_param_set(node, "data", size, data);

    int ret = ngli_node_attach_ctx(node, ctx);
    if (ret < 0)
        goto fail;

    ret = ngli_node_init(node);
    if (ret < 0)
        goto fail;

    return node;
fail:
    ngli_node_detach_ctx(node);
    ngl_node_unrefp(&node);
    return NULL;
}

struct ngl_node *ngli_geometry_generate_indices_buffer(struct ngl_ctx *ctx, int count)
{
    int size = count * sizeof(GLushort);
    uint8_t *data = calloc(count, sizeof(GLushort));
    if (!data)
        return NULL;

    SET_INDICES(GLushort, count, data);

    struct ngl_node *node = ngli_geometry_generate_buffer(ctx,
                                                          NGL_NODE_BUFFERUSHORT,
                                                          count,
                                                          size,
                                                          data);
    free(data);
    return node;
}

#define OFFSET(x) offsetof(struct geometry, x)
static const struct node_param geometry_params[] = {
    {"vertices",  PARAM_TYPE_NODE, OFFSET(vertices_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, -1},
                  .flags=PARAM_FLAG_CONSTRUCTOR | PARAM_FLAG_DOT_DISPLAY_FIELDNAME},
    {"texcoords", PARAM_TYPE_NODE, OFFSET(texcoords_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC2, NGL_NODE_ANIMATEDBUFFERVEC2, -1},
                  .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME},
    {"normals",   PARAM_TYPE_NODE, OFFSET(normals_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, -1},
                  .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME},
    {"indices",   PARAM_TYPE_NODE, OFFSET(indices_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERUBYTE, NGL_NODE_BUFFERUINT, NGL_NODE_BUFFERUSHORT, -1},
                  .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME},
    {"draw_mode", PARAM_TYPE_INT, OFFSET(draw_mode), {.i64=GL_TRIANGLES}},
    {NULL}
};

static int geometry_init(struct ngl_node *node)
{
    struct geometry *s = node->priv_data;

    int ret = ngli_node_init(s->vertices_buffer);
    if (ret < 0)
        return ret;

    struct buffer *vertices = s->vertices_buffer->priv_data;

    if (s->texcoords_buffer) {
        int ret = ngli_node_init(s->texcoords_buffer);
        if (ret < 0)
            return ret;

        struct buffer *b = s->texcoords_buffer->priv_data;
        if (b->count != vertices->count) {
            LOG(ERROR,
                "texcoords count (%d) does not match vertices count (%d)",
                b->count,
                vertices->count);
            return -1;
        }
    }

    if (s->normals_buffer) {
        int ret = ngli_node_init(s->normals_buffer);
        if (ret < 0)
            return ret;

        struct buffer *b = s->normals_buffer->priv_data;
        if (b->count != vertices->count) {
            LOG(ERROR,
                "normals count (%d) does not match vertices count (%d)",
                b->count,
                vertices->count);
            return -1;
        }
    }

    if (s->indices_buffer) {
        int ret = ngli_node_init(s->indices_buffer);
        if (ret < 0)
            return ret;

        switch (s->indices_buffer->class->id) {
        case NGL_NODE_BUFFERUBYTE:  s->draw_type = GL_UNSIGNED_BYTE;  break;
        case NGL_NODE_BUFFERUINT:   s->draw_type = GL_UNSIGNED_INT;   break;
        case NGL_NODE_BUFFERUSHORT: s->draw_type = GL_UNSIGNED_SHORT; break;
        default:
            ngli_assert(0);
        }
    } else {
        s->indices_buffer = ngli_geometry_generate_indices_buffer(node->ctx,
                                                                  vertices->count);
        if (!s->indices_buffer)
            return -1;

        s->draw_type = GL_UNSIGNED_SHORT;
    }

    return 0;
}

static void geometry_update(struct ngl_node *node, double t)
{
    struct geometry *s = node->priv_data;

    ngli_node_update(s->vertices_buffer, t);
    if (s->texcoords_buffer)
        ngli_node_update(s->texcoords_buffer, t);
    if (s->normals_buffer)
        ngli_node_update(s->normals_buffer, t);
}

const struct node_class ngli_geometry_class = {
    .id        = NGL_NODE_GEOMETRY,
    .name      = "Geometry",
    .init      = geometry_init,
    .update    = geometry_update,
    .priv_size = sizeof(struct geometry),
    .params    = geometry_params,
};
