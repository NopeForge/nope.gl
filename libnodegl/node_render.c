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
#include "log.h"
#include "math_utils.h"
#include "ndict.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_UNIFORMSCALAR,  \
                                          NGL_NODE_UNIFORMVEC2,    \
                                          NGL_NODE_UNIFORMVEC3,    \
                                          NGL_NODE_UNIFORMVEC4,    \
                                          NGL_NODE_UNIFORMINT,     \
                                          NGL_NODE_UNIFORMMAT4,    \
                                          -1}

#define ATTRIBUTES_TYPES_LIST (const int[]){-1}

#define OFFSET(x) offsetof(struct texturedshape, x)
static const struct node_param texturedshape_params[] = {
    {"shape",    PARAM_TYPE_NODE, OFFSET(shape), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=(const int[]){NGL_NODE_QUAD, NGL_NODE_TRIANGLE, NGL_NODE_SHAPE, -1}},
    {"shader",   PARAM_TYPE_NODE, OFFSET(shader), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=(const int[]){NGL_NODE_SHADER, -1}},
    {"textures", PARAM_TYPE_NODEDICT, OFFSET(textures),
                 .node_types=(const int[]){NGL_NODE_TEXTURE, -1}},
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

    struct texturedshape *s = node->priv_data;
    struct shader *shader = s->shader->priv_data;

    int i = 0;
    struct ndict_entry *entry = NULL;
    while ((entry = ngli_ndict_get(s->uniforms, NULL, entry))) {
        const struct ngl_node *unode = entry->node;
        const struct uniform *u = entry->node->priv_data;
        const GLint uid = s->uniform_ids[i];
        switch (unode->class->id) {
        case NGL_NODE_UNIFORMSCALAR: ngli_glUniform1f (gl, uid,    u->scalar);                 break;
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

    i = 0;
    entry = NULL;
    while ((entry = ngli_ndict_get(s->textures, NULL, entry))) {
        struct ngl_node *tnode = entry->node;
        struct texture *texture = tnode->priv_data;
        struct textureshaderinfo *textureshaderinfo = &s->textureshaderinfos[i];

        if (textureshaderinfo->sampler_id >= 0) {
            const int sampler_id = textureshaderinfo->sampler_id;
            bind_texture(gl, texture->target, sampler_id, texture->id, i);
        }

        if (textureshaderinfo->coordinates_mvp_id >= 0) {
            ngli_glUniformMatrix4fv(gl, textureshaderinfo->coordinates_mvp_id, 1, GL_FALSE, texture->coordinates_matrix);
        }

        if (textureshaderinfo->dimensions_id >= 0) {
            float dimensions[2] = { texture->width, texture->height };
            ngli_glUniform2fv(gl, textureshaderinfo->dimensions_id, 1, dimensions);
        }

        i++;
    }

    if (shader->modelview_matrix_location_id >= 0) {
        ngli_glUniformMatrix4fv(gl, shader->modelview_matrix_location_id, 1, GL_FALSE, node->modelview_matrix);
    }

    if (shader->projection_matrix_location_id >= 0) {
        ngli_glUniformMatrix4fv(gl, shader->projection_matrix_location_id, 1, GL_FALSE, node->projection_matrix);
    }

    if (shader->normal_matrix_location_id >= 0) {
        float normal_matrix[3*3];
        ngli_mat3_from_mat4(normal_matrix, node->modelview_matrix);
        ngli_mat3_inverse(normal_matrix, normal_matrix);
        ngli_mat3_transpose(normal_matrix, normal_matrix);
        ngli_glUniformMatrix3fv(gl, shader->normal_matrix_location_id, 1, GL_FALSE, normal_matrix);
    }

    return 0;
}

static int update_vertex_attribs(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texturedshape *s = node->priv_data;
    struct shape *shape = s->shape->priv_data;
    struct shader *shader = s->shader->priv_data;

    int nb_textures = ngli_ndict_count(s->textures);
    for (int i = 0; i < nb_textures; i++)  {
        struct textureshaderinfo *textureshaderinfo = &s->textureshaderinfos[i];
        if (textureshaderinfo->coordinates_id >= 0) {
            ngli_glEnableVertexAttribArray(gl, textureshaderinfo->coordinates_id);
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, shape->texcoords_buffer_id);
            ngli_glVertexAttribPointer(gl, textureshaderinfo->coordinates_id, 2, GL_FLOAT, GL_FALSE, NGLI_SHAPE_VERTICES_STRIDE(shape), NULL);
        }
    }

    if (shader->position_location_id >= 0) {
        ngli_glEnableVertexAttribArray(gl, shader->position_location_id);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, shape->vertices_buffer_id);
        ngli_glVertexAttribPointer(gl, shader->position_location_id, 3, GL_FLOAT, GL_FALSE, NGLI_SHAPE_VERTICES_STRIDE(shape), NULL);
    }

    if (shader->normal_location_id >= 0) {
        ngli_glEnableVertexAttribArray(gl, shader->normal_location_id);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, shape->normals_buffer_id);
        ngli_glVertexAttribPointer(gl, shader->normal_location_id, 3, GL_FLOAT, GL_FALSE, NGLI_SHAPE_VERTICES_STRIDE(shape), NULL);
    }

    return 0;
}

static int texturedshape_init(struct ngl_node *node)
{
    int ret;

    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texturedshape *s = node->priv_data;
    struct shader *shader = s->shader->priv_data;

    ret = ngli_node_init(s->shape);
    if (ret < 0)
        return ret;

    ret = ngli_node_init(s->shader);
    if (ret < 0)
        return ret;

    int nb_uniforms = ngli_ndict_count(s->uniforms);
    if (nb_uniforms > 0) {
        s->uniform_ids = calloc(nb_uniforms, sizeof(*s->uniform_ids));
        if (!s->uniform_ids)
            return -1;
    }

    int i = 0;
    struct ndict_entry *entry = NULL;
    while ((entry = ngli_ndict_get(s->uniforms, NULL, entry))) {
        struct ngl_node *unode = entry->node;
        ret = ngli_node_init(unode);
        if (ret < 0)
            return ret;
        s->uniform_ids[i] = ngli_glGetUniformLocation(gl, shader->program_id, entry->name);
        i++;
    }

    int nb_attributes = ngli_ndict_count(s->attributes);
    if (nb_attributes > 0) {
        s->attribute_ids = calloc(nb_attributes, sizeof(*s->attribute_ids));
        if (!s->attribute_ids)
            return -1;
    }

    i = 0;
    entry = NULL;
    while ((entry = ngli_ndict_get(s->attributes, NULL, entry))) {
        struct ngl_node *anode = entry->node;
        ret = ngli_node_init(anode);
        if (ret < 0)
            return ret;
        s->attribute_ids[i] = ngli_glGetAttribLocation(gl, shader->program_id, entry->name);
        i++;
    }

    int nb_textures = ngli_ndict_count(s->textures);
    if (nb_textures > glcontext->max_texture_image_units) {
        LOG(ERROR, "Attached textures count (%d) exceeds driver limit (%d)",
            nb_textures, glcontext->max_texture_image_units);
        return -1;
    }

    if (nb_textures > 0) {
        s->textureshaderinfos = calloc(nb_textures, sizeof(*s->textureshaderinfos));
        if (!s->textureshaderinfos)
            return -1;
    }

    i = 0;
    entry = NULL;
    while ((entry = ngli_ndict_get(s->textures, NULL, entry))) {
        char name[128];
        struct ngl_node *tnode = entry->node;

        ret = ngli_node_init(tnode);
        if (ret < 0)
            return ret;

        snprintf(name, sizeof(name), "%s_sampler", entry->name);
        s->textureshaderinfos[i].sampler_id = ngli_glGetUniformLocation(gl, shader->program_id, name);

        snprintf(name, sizeof(name), "%s_coords", entry->name);
        s->textureshaderinfos[i].coordinates_id = ngli_glGetAttribLocation(gl, shader->program_id, name);

        snprintf(name, sizeof(name), "%s_coords_matrix", entry->name);
        s->textureshaderinfos[i].coordinates_mvp_id = ngli_glGetUniformLocation(gl, shader->program_id, name);

        snprintf(name, sizeof(name), "%s_dimensions", entry->name);
        s->textureshaderinfos[i].dimensions_id = ngli_glGetUniformLocation(gl, shader->program_id, name);
        i++;
    }

    if (glcontext->has_vao_compatibility) {
        ngli_glGenVertexArrays(gl, 1, &s->vao_id);
        ngli_glBindVertexArray(gl, s->vao_id);
        update_vertex_attribs(node);
    }

    return 0;
}

static void texturedshape_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texturedshape *s = node->priv_data;

    if (glcontext->has_vao_compatibility) {
        ngli_glDeleteVertexArrays(gl, 1, &s->vao_id);
    }

    free(s->textureshaderinfos);
    free(s->uniform_ids);
    free(s->attribute_ids);
}

static void texturedshape_update(struct ngl_node *node, double t)
{
    struct ndict_entry *entry;
    struct texturedshape *s = node->priv_data;

    ngli_node_update(s->shape, t);

    entry = NULL;
    while ((entry = ngli_ndict_get(s->textures, NULL, entry))) {
        ngli_node_update(entry->node, t);
    }

    entry = NULL;
    while ((entry = ngli_ndict_get(s->uniforms, NULL, entry))) {
        ngli_node_update(entry->node, t);
    }

    ngli_node_update(s->shader, t);
}

static void texturedshape_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texturedshape *s = node->priv_data;
    const struct shader *shader = s->shader->priv_data;
    const struct shape *shape = s->shape->priv_data;

    ngli_glUseProgram(gl, shader->program_id);

    if (glcontext->has_vao_compatibility) {
        ngli_glBindVertexArray(gl, s->vao_id);
    }

    update_uniforms(node);

    if (!glcontext->has_vao_compatibility) {
        update_vertex_attribs(node);
    }

    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, shape->indices_buffer_id);
    ngli_glDrawElements(gl, shape->draw_mode, shape->nb_indices, shape->draw_type, 0);
}

const struct node_class ngli_texturedshape_class = {
    .id        = NGL_NODE_TEXTUREDSHAPE,
    .name      = "TexturedShape",
    .init      = texturedshape_init,
    .uninit    = texturedshape_uninit,
    .update    = texturedshape_update,
    .draw      = texturedshape_draw,
    .priv_size = sizeof(struct texturedshape),
    .params    = texturedshape_params,
};
