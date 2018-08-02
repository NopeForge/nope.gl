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
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "glincludes.h"
#include "hmap.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define TEXTURES_TYPES_LIST (const int[]){NGL_NODE_TEXTURE2D,       \
                                          NGL_NODE_TEXTURE3D,       \
                                          -1}

#define PROGRAMS_TYPES_LIST (const int[]){NGL_NODE_PROGRAM,         \
                                          -1}

#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_BUFFERFLOAT,     \
                                          NGL_NODE_BUFFERVEC2,      \
                                          NGL_NODE_BUFFERVEC3,      \
                                          NGL_NODE_BUFFERVEC4,      \
                                          NGL_NODE_UNIFORMFLOAT,    \
                                          NGL_NODE_UNIFORMVEC2,     \
                                          NGL_NODE_UNIFORMVEC3,     \
                                          NGL_NODE_UNIFORMVEC4,     \
                                          NGL_NODE_UNIFORMQUAT,     \
                                          NGL_NODE_UNIFORMINT,      \
                                          NGL_NODE_UNIFORMMAT4,     \
                                          -1}

#define ATTRIBUTES_TYPES_LIST (const int[]){NGL_NODE_BUFFERFLOAT,   \
                                            NGL_NODE_BUFFERVEC2,    \
                                            NGL_NODE_BUFFERVEC3,    \
                                            NGL_NODE_BUFFERVEC4,    \
                                            -1}

#define GEOMETRY_TYPES_LIST (const int[]){NGL_NODE_CIRCLE,          \
                                          NGL_NODE_GEOMETRY,        \
                                          NGL_NODE_QUAD,            \
                                          NGL_NODE_TRIANGLE,        \
                                          -1}

#define BUFFERS_TYPES_LIST (const int[]){NGL_NODE_BUFFERFLOAT,      \
                                         NGL_NODE_BUFFERVEC2,       \
                                         NGL_NODE_BUFFERVEC3,       \
                                         NGL_NODE_BUFFERVEC4,       \
                                         NGL_NODE_BUFFERINT,        \
                                         NGL_NODE_BUFFERIVEC2,      \
                                         NGL_NODE_BUFFERIVEC3,      \
                                         NGL_NODE_BUFFERIVEC4,      \
                                         NGL_NODE_BUFFERUINT,       \
                                         NGL_NODE_BUFFERUIVEC2,     \
                                         NGL_NODE_BUFFERUIVEC3,     \
                                         NGL_NODE_BUFFERUIVEC4,     \
                                         -1}

#define OFFSET(x) offsetof(struct render, x)
static const struct node_param render_params[] = {
    {"geometry", PARAM_TYPE_NODE, OFFSET(geometry), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=GEOMETRY_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("geometry to be rasterized")},
    {"program",  PARAM_TYPE_NODE, OFFSET(pipeline.program),
                 .node_types=PROGRAMS_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("program to be executed")},
    {"textures", PARAM_TYPE_NODEDICT, OFFSET(pipeline.textures),
                 .node_types=TEXTURES_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("textures made accessible to the `program`")},
    {"uniforms", PARAM_TYPE_NODEDICT, OFFSET(pipeline.uniforms),
                 .node_types=UNIFORMS_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("uniforms made accessible to the `program`")},
    {"buffers",  PARAM_TYPE_NODEDICT, OFFSET(pipeline.buffers),
                 .node_types=BUFFERS_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("buffers made accessible to the `program`")},
    {"attributes", PARAM_TYPE_NODEDICT, OFFSET(attributes),
                 .node_types=ATTRIBUTES_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("extra vertex attributes made accessible to the `program`")},
    {NULL}
};

static int update_geometry_uniforms(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct render *s = node->priv_data;
    struct program *program = s->pipeline.program->priv_data;

    if (program->modelview_matrix_location_id >= 0) {
        ngli_glUniformMatrix4fv(gl, program->modelview_matrix_location_id, 1, GL_FALSE, node->modelview_matrix);
    }

    if (program->projection_matrix_location_id >= 0) {
        ngli_glUniformMatrix4fv(gl, program->projection_matrix_location_id, 1, GL_FALSE, node->projection_matrix);
    }

    if (program->normal_matrix_location_id >= 0) {
        float normal_matrix[3*3];
        ngli_mat3_from_mat4(normal_matrix, node->modelview_matrix);
        ngli_mat3_inverse(normal_matrix, normal_matrix);
        ngli_mat3_transpose(normal_matrix, normal_matrix);
        ngli_glUniformMatrix3fv(gl, program->normal_matrix_location_id, 1, GL_FALSE, normal_matrix);
    }

    return 0;
}

static void update_vertex_attrib(struct ngl_node *node, struct buffer *buffer, int location)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    ngli_glEnableVertexAttribArray(gl, location);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->buffer_id);
    ngli_glVertexAttribPointer(gl, location, buffer->data_comp, GL_FLOAT, GL_FALSE, buffer->data_stride, NULL);
}

static int update_vertex_attribs(struct ngl_node *node)
{
    struct render *s = node->priv_data;
    struct geometry *geometry = s->geometry->priv_data;
    struct program *program = s->pipeline.program->priv_data;

    if (geometry->vertices_buffer) {
        struct buffer *buffer = geometry->vertices_buffer->priv_data;
        if (program->position_location_id >= 0) {
            update_vertex_attrib(node, buffer, program->position_location_id);
        }
    }

    if (geometry->uvcoords_buffer) {
        struct buffer *buffer = geometry->uvcoords_buffer->priv_data;
        if (program->uvcoord_location_id >= 0) {
            update_vertex_attrib(node, buffer, program->uvcoord_location_id);
        }
    }

    if (geometry->normals_buffer) {
        struct buffer *buffer = geometry->normals_buffer->priv_data;
        if (program->normal_location_id >= 0) {
            update_vertex_attrib(node, buffer, program->normal_location_id);
        }
    }

    for (int i = 0; i < s->nb_attribute_ids; i++) {
        struct attributeprograminfo *info = &s->attribute_ids[i];
        const GLint aid = info->id;
        if (aid < 0)
            continue;
        const struct ngl_node *anode = ngli_hmap_get(s->attributes, info->name);
        struct buffer *buffer = anode->priv_data;
        update_vertex_attrib(node, buffer, aid);
    }

    return 0;
}

static int disable_vertex_attribs(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct render *s = node->priv_data;
    struct geometry *geometry = s->geometry->priv_data;
    struct program *program = s->pipeline.program->priv_data;

    if (geometry->vertices_buffer) {
        if (program->position_location_id >= 0) {
            ngli_glDisableVertexAttribArray(gl, program->position_location_id);
        }
    }

    if (geometry->uvcoords_buffer) {
        if (program->uvcoord_location_id >= 0) {
            ngli_glDisableVertexAttribArray(gl, program->uvcoord_location_id);
        }
    }

    if (geometry->normals_buffer) {
        if (program->normal_location_id >= 0) {
            ngli_glDisableVertexAttribArray(gl, program->normal_location_id);
        }
    }

    for (int i = 0; i < s->nb_attribute_ids; i++) {
        struct attributeprograminfo *info = &s->attribute_ids[i];
        const GLint aid = info->id;
        if (aid < 0)
            continue;
        ngli_glDisableVertexAttribArray(gl, aid);
    }

    return 0;
}

static int render_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct render *s = node->priv_data;

    int ret = ngli_node_init(s->geometry);
    if (ret < 0)
        return ret;

    if (!s->pipeline.program) {
        s->pipeline.program = ngl_node_create(NGL_NODE_PROGRAM);
        if (!s->pipeline.program)
            return -1;
        ret = ngli_node_attach_ctx(s->pipeline.program, ctx);
        if (ret < 0)
            return ret;
    }

    ngli_pipeline_init(node);

    int nb_attributes = s->attributes ? ngli_hmap_count(s->attributes) : 0;
    if (nb_attributes > 0) {
        struct program *program = s->pipeline.program->priv_data;
        struct geometry *geometry = s->geometry->priv_data;
        struct buffer *vertices = geometry->vertices_buffer->priv_data;
        s->attribute_ids = calloc(nb_attributes, sizeof(*s->attribute_ids));
        if (!s->attribute_ids)
            return -1;

        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->attributes, entry))) {
            struct attributeprograminfo *active_attribute =
                ngli_hmap_get(program->active_attributes, entry->key);
            if (!active_attribute) {
                LOG(WARNING, "attribute %s attached to %s not found in %s",
                    entry->key, node->name, s->pipeline.program->name);
                continue;
            }

            struct ngl_node *anode = entry->data;
            struct buffer *buffer = anode->priv_data;
            buffer->generate_gl_buffer = 1;

            ret = ngli_node_init(anode);
            if (ret < 0)
                return ret;
            if (buffer->count != vertices->count) {
                LOG(ERROR,
                    "attribute buffer %s count (%d) does not match vertices count (%d)",
                    active_attribute->name,
                    buffer->count,
                    vertices->count);
                return -1;
            }

            struct attributeprograminfo *infop = &s->attribute_ids[s->nb_attribute_ids++];
            *infop = *active_attribute;
        }
    }

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &s->vao_id);
        ngli_glBindVertexArray(gl, s->vao_id);
        update_vertex_attribs(node);
    }

    return 0;
}

static void render_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct render *s = node->priv_data;

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glDeleteVertexArrays(gl, 1, &s->vao_id);
    }

    ngli_pipeline_uninit(node);

    free(s->attribute_ids);
}

static int render_update(struct ngl_node *node, double t)
{
    struct render *s = node->priv_data;

    int ret = ngli_node_update(s->geometry, t);
    if (ret < 0)
        return ret;

    return ngli_pipeline_update(node, t);
}

static void render_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct render *s = node->priv_data;

    const struct program *program = s->pipeline.program->priv_data;
    ngli_glUseProgram(gl, program->info.program_id);

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glBindVertexArray(gl, s->vao_id);
    } else {
        update_vertex_attribs(node);
    }

    update_geometry_uniforms(node);

    ngli_pipeline_upload_data(node);

    const struct geometry *geometry = s->geometry->priv_data;
    const struct buffer *indices_buffer = geometry->indices_buffer->priv_data;

    GLenum indices_type;
    ngli_format_get_gl_format_type(gl, indices_buffer->data_format, NULL, NULL, &indices_type);

    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_buffer->buffer_id);
    ngli_glDrawElements(gl, geometry->draw_mode, indices_buffer->count, indices_type, 0);

    if (!(gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)) {
        disable_vertex_attribs(node);
    }
}

const struct node_class ngli_render_class = {
    .id        = NGL_NODE_RENDER,
    .name      = "Render",
    .init      = render_init,
    .uninit    = render_uninit,
    .update    = render_update,
    .draw      = render_draw,
    .priv_size = sizeof(struct render),
    .params    = render_params,
    .file      = __FILE__,
};
