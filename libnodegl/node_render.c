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

#include "glincludes.h"
#include "hmap.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_UNIFORMFLOAT,   \
                                          NGL_NODE_UNIFORMVEC2,    \
                                          NGL_NODE_UNIFORMVEC3,    \
                                          NGL_NODE_UNIFORMVEC4,    \
                                          NGL_NODE_UNIFORMINT,     \
                                          NGL_NODE_UNIFORMMAT4,    \
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

#define OFFSET(x) offsetof(struct render, x)
static const struct node_param render_params[] = {
    {"geometry", PARAM_TYPE_NODE, OFFSET(geometry), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=GEOMETRY_TYPES_LIST},
    {"program",  PARAM_TYPE_NODE, OFFSET(program), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=(const int[]){NGL_NODE_PROGRAM, -1}},
    {"textures", PARAM_TYPE_NODEDICT, OFFSET(textures),
                 .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1}},
    {"uniforms", PARAM_TYPE_NODEDICT, OFFSET(uniforms),
                 .node_types=UNIFORMS_TYPES_LIST},
    {"attributes", PARAM_TYPE_NODEDICT, OFFSET(attributes),
                 .node_types=ATTRIBUTES_TYPES_LIST},
    {NULL}
};

static inline void bind_texture(const struct glfunctions *gl, GLenum target, GLint uniform_location, GLuint texture_id, int idx)
{
    ngli_glActiveTexture(gl, GL_TEXTURE0 + idx);
    ngli_glBindTexture(gl, target, texture_id);
    ngli_glUniform1i(gl, uniform_location, idx);
}

static int update_uniforms(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct render *s = node->priv_data;
    struct program *program = s->program->priv_data;

    if (s->uniforms) {
        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->uniforms, entry))) {
            const struct ngl_node *unode = entry->data;
            const struct uniform *u = unode->priv_data;
            const GLint uid = s->uniform_ids[i];
            switch (unode->class->id) {
            case NGL_NODE_UNIFORMFLOAT:  ngli_glUniform1f (gl, uid,    u->scalar);                 break;
            case NGL_NODE_UNIFORMVEC2:   ngli_glUniform2fv(gl, uid, 1, u->vector);                 break;
            case NGL_NODE_UNIFORMVEC3:   ngli_glUniform3fv(gl, uid, 1, u->vector);                 break;
            case NGL_NODE_UNIFORMVEC4:   ngli_glUniform4fv(gl, uid, 1, u->vector);                 break;
            case NGL_NODE_UNIFORMINT:    ngli_glUniform1i (gl, uid,    u->ival);                   break;
            case NGL_NODE_UNIFORMMAT4:   ngli_glUniformMatrix4fv(gl, uid, 1, GL_FALSE, u->matrix); break;
            default:
                LOG(ERROR, "unsupported uniform of type %s", unode->class->name);
                break;
            }
            i++;
        }
    }

    if (s->textures) {
        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            struct ngl_node *tnode = entry->data;
            struct texture *texture = tnode->priv_data;
            struct textureprograminfo *textureprograminfo = &s->textureprograminfos[i];

            if (textureprograminfo->sampler_id >= 0) {
                const int sampler_id = textureprograminfo->sampler_id;
                bind_texture(gl, texture->target, sampler_id, texture->id, i);
            }

            if (textureprograminfo->coord_matrix_id >= 0) {
                ngli_glUniformMatrix4fv(gl, textureprograminfo->coord_matrix_id, 1, GL_FALSE, texture->coordinates_matrix);
            }

            if (textureprograminfo->dimensions_id >= 0) {
                const float dimensions[2] = { texture->width, texture->height };
                ngli_glUniform2fv(gl, textureprograminfo->dimensions_id, 1, dimensions);
            }

            i++;
        }
    }

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

static int update_vertex_attribs(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct render *s = node->priv_data;
    struct geometry *geometry = s->geometry->priv_data;
    struct program *program = s->program->priv_data;

    if (geometry->vertices_buffer) {
        struct buffer *buffer = geometry->vertices_buffer->priv_data;
        if (program->position_location_id >= 0) {
            ngli_glEnableVertexAttribArray(gl, program->position_location_id);
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->buffer_id);
            ngli_glVertexAttribPointer(gl, program->position_location_id, buffer->data_comp, GL_FLOAT, GL_FALSE, buffer->data_stride, NULL);
        }
    }

    if (geometry->texcoords_buffer) {
        struct buffer *buffer = geometry->texcoords_buffer->priv_data;
        if (program->uvcoord_location_id >= 0) {
            ngli_glEnableVertexAttribArray(gl, program->uvcoord_location_id);
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->buffer_id);
            ngli_glVertexAttribPointer(gl, program->uvcoord_location_id, buffer->data_comp, GL_FLOAT, GL_FALSE, buffer->data_stride, NULL);
        }
    }

    if (geometry->normals_buffer) {
        struct buffer *buffer = geometry->normals_buffer->priv_data;
        if (program->normal_location_id >= 0) {
            ngli_glEnableVertexAttribArray(gl, program->normal_location_id);
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->buffer_id);
            ngli_glVertexAttribPointer(gl, program->normal_location_id, buffer->data_comp, GL_FLOAT, GL_FALSE, buffer->data_stride, NULL);
        }
    }

    if (s->attributes) {
        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->attributes, entry))) {
            if (s->attribute_ids[i] < 0)
                continue;

            struct ngl_node *anode = entry->data;
            struct buffer *b = anode->priv_data;
            ngli_glEnableVertexAttribArray(gl, s->attribute_ids[i]);
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, b->buffer_id);
            ngli_glVertexAttribPointer(gl, s->attribute_ids[i], b->data_comp, GL_FLOAT, GL_FALSE, b->data_stride, NULL);
            i++;
        }
    }

    return 0;
}

static int render_init(struct ngl_node *node)
{
    int ret;

    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct render *s = node->priv_data;
    struct program *program = s->program->priv_data;

    ret = ngli_node_init(s->geometry);
    if (ret < 0)
        return ret;

    ret = ngli_node_init(s->program);
    if (ret < 0)
        return ret;

    int nb_uniforms = s->uniforms ? ngli_hmap_count(s->uniforms) : 0;
    if (nb_uniforms > 0) {
        s->uniform_ids = calloc(nb_uniforms, sizeof(*s->uniform_ids));
        if (!s->uniform_ids)
            return -1;

        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->uniforms, entry))) {
            struct ngl_node *unode = entry->data;
            ret = ngli_node_init(unode);
            if (ret < 0)
                return ret;
            s->uniform_ids[i] = ngli_glGetUniformLocation(gl, program->program_id, entry->key);
            i++;
        }
    }

    int nb_attributes = s->attributes ? ngli_hmap_count(s->attributes) : 0;
    if (nb_attributes > 0) {
        struct geometry *geometry = s->geometry->priv_data;
        struct buffer *vertices = geometry->vertices_buffer->priv_data;
        s->attribute_ids = calloc(nb_attributes, sizeof(*s->attribute_ids));
        if (!s->attribute_ids)
            return -1;

        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->attributes, entry))) {
            struct ngl_node *anode = entry->data;
            ret = ngli_node_init(anode);
            if (ret < 0)
                return ret;
            struct buffer *buffer = anode->priv_data;
            if (buffer->count != vertices->count) {
                LOG(ERROR,
                    "attribute buffer %s count (%d) does not match vertices count (%d)",
                    entry->key,
                    buffer->count,
                    vertices->count);
                return -1;
            }
            s->attribute_ids[i] = ngli_glGetAttribLocation(gl, program->program_id, entry->key);
            i++;
        }
    }

    int nb_textures = s->textures ? ngli_hmap_count(s->textures) : 0;
    if (nb_textures > glcontext->max_texture_image_units) {
        LOG(ERROR, "Attached textures count (%d) exceeds driver limit (%d)",
            nb_textures, glcontext->max_texture_image_units);
        return -1;
    }

    if (nb_textures > 0) {
        s->textureprograminfos = calloc(nb_textures, sizeof(*s->textureprograminfos));
        if (!s->textureprograminfos)
            return -1;

        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            char name[128];
            struct ngl_node *tnode = entry->data;

            ret = ngli_node_init(tnode);
            if (ret < 0)
                return ret;

            struct textureprograminfo *info = &s->textureprograminfos[i];

            snprintf(name, sizeof(name), "%s_sampler", entry->key);
            info->sampler_id = ngli_glGetUniformLocation(gl, program->program_id, name);

            snprintf(name, sizeof(name), "%s_coord_matrix", entry->key);
            info->coord_matrix_id = ngli_glGetUniformLocation(gl, program->program_id, name);

            snprintf(name, sizeof(name), "%s_dimensions", entry->key);
            info->dimensions_id = ngli_glGetUniformLocation(gl, program->program_id, name);
            i++;
        }
    }

    if (glcontext->has_vao_compatibility) {
        ngli_glGenVertexArrays(gl, 1, &s->vao_id);
        ngli_glBindVertexArray(gl, s->vao_id);
        update_vertex_attribs(node);
    }

    return 0;
}

static void render_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct render *s = node->priv_data;

    if (glcontext->has_vao_compatibility) {
        ngli_glDeleteVertexArrays(gl, 1, &s->vao_id);
    }

    free(s->textureprograminfos);
    free(s->uniform_ids);
    free(s->attribute_ids);
}

static void render_update(struct ngl_node *node, double t)
{
    struct render *s = node->priv_data;

    ngli_node_update(s->geometry, t);

    if (s->textures) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            ngli_node_update(entry->data, t);
        }
    }

    if (s->uniforms) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->uniforms, entry))) {
            ngli_node_update(entry->data, t);
        }
    }

    ngli_node_update(s->program, t);
}

static void render_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct render *s = node->priv_data;

    const struct program *program = s->program->priv_data;
    ngli_glUseProgram(gl, program->program_id);

    if (glcontext->has_vao_compatibility) {
        ngli_glBindVertexArray(gl, s->vao_id);
    }

    update_uniforms(node);

    if (!glcontext->has_vao_compatibility) {
        update_vertex_attribs(node);
    }

    const struct geometry *geometry = s->geometry->priv_data;
    const struct buffer *indices_buffer = geometry->indices_buffer->priv_data;

    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_buffer->buffer_id);
    ngli_glDrawElements(gl, geometry->draw_mode, indices_buffer->count, indices_buffer->comp_type, 0);
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
};
