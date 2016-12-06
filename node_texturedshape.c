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

#include "gl_utils.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_UNIFORMSCALAR,  \
                                          NGL_NODE_UNIFORMVEC2,    \
                                          NGL_NODE_UNIFORMVEC3,    \
                                          NGL_NODE_UNIFORMVEC4,    \
                                          NGL_NODE_UNIFORMINT,     \
                                          NGL_NODE_UNIFORMMAT4,    \
                                          NGL_NODE_UNIFORMSAMPLER, \
                                          -1}

#define ATTRIBUTES_TYPES_LIST (const int[]){NGL_NODE_ATTRIBUTEVEC2, \
                                            NGL_NODE_ATTRIBUTEVEC3, \
                                            NGL_NODE_ATTRIBUTEVEC4, \
                                            -1}

#define OFFSET(x) offsetof(struct texturedshape, x)
static const struct node_param texturedshape_params[] = {
    {"shape",    PARAM_TYPE_NODE, OFFSET(shape), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=(const int[]){NGL_NODE_QUAD, NGL_NODE_TRIANGLE, NGL_NODE_SHAPE, -1}},
    {"shader",   PARAM_TYPE_NODE, OFFSET(shader), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=(const int[]){NGL_NODE_SHADER, -1}},
    {"texture0", PARAM_TYPE_NODE, OFFSET(textures[0]),
                 .node_types=(const int[]){NGL_NODE_TEXTURE, -1}},
    {"texture1", PARAM_TYPE_NODE, OFFSET(textures[1]),
                 .node_types=(const int[]){NGL_NODE_TEXTURE, -1}},
    {"uniforms", PARAM_TYPE_NODELIST, OFFSET(uniforms),
                 .node_types=UNIFORMS_TYPES_LIST},
    {"attributes", PARAM_TYPE_NODELIST, OFFSET(attributes),
                 .node_types=ATTRIBUTES_TYPES_LIST},
    {NULL}
};

static inline void bind_texture(GLenum target, GLint uniform_location, GLuint texture_id, int idx)
{
    glActiveTexture(GL_TEXTURE0 + idx);
    glBindTexture(target, texture_id);
    glUniform1i(uniform_location, idx);
}

static int update_uniforms(struct ngl_node *node)
{
    struct texturedshape *s = node->priv_data;
    struct shader *shader = s->shader->priv_data;

    for (int i = 0; i < s->nb_uniforms; i++) {
        const struct ngl_node *unode = s->uniforms[i];
        const struct uniform *u = unode->priv_data;
        const GLint uid = s->uniform_ids[i];
        switch (unode->class->id) {
        case NGL_NODE_UNIFORMSCALAR:
            glUniform1f(uid, u->scalar);
            break;
        case NGL_NODE_UNIFORMVEC2: glUniform2fv(uid, 1, u->vector);                break;
        case NGL_NODE_UNIFORMVEC3: glUniform3fv(uid, 1, u->vector);                break;
        case NGL_NODE_UNIFORMVEC4: glUniform4fv(uid, 1, u->vector);                break;
        case NGL_NODE_UNIFORMINT:  glUniform1i (uid,    u->ival);                  break;
        case NGL_NODE_UNIFORMMAT4: glUniformMatrix4fv(uid, 1, GL_FALSE, u->matrix); break;
        case NGL_NODE_UNIFORMSAMPLER:                                              break;
        default:
            LOG(ERROR, "unsupported uniform of type %s", unode->class->name);
            break;
        }
    }

    for (int j = 0; j < NGLI_ARRAY_NB(s->textures); j++) {
        if (!s->textures[j])
            continue;

        struct texture *texture = s->textures[j]->priv_data;
        struct textureshaderinfo *textureshaderinfo = &s->textureshaderinfos[j];

        if (textureshaderinfo->sampler_id >= 0) {
            const int sampler_id = textureshaderinfo->sampler_id;

#ifdef __ANDROID__
            const int external_sampler_id = textureshaderinfo->sampler_external_id;

            if (texture->target == GL_TEXTURE_2D) {
                bind_texture(GL_TEXTURE_2D,           sampler_id,          texture->id, j*2);
                bind_texture(GL_TEXTURE_EXTERNAL_OES, external_sampler_id, 0,           j*2 + 1);
            } else {
                bind_texture(GL_TEXTURE_2D,           sampler_id,          0,           j*2);
                bind_texture(GL_TEXTURE_EXTERNAL_OES, external_sampler_id, texture->id, j*2 + 1);
            }
#else
            bind_texture(GL_TEXTURE_2D, sampler_id, texture->id, j);
#endif
        }

        if (textureshaderinfo->coordinates_mvp_id >= 0) {
            glUniformMatrix4fv(textureshaderinfo->coordinates_mvp_id, 1, GL_FALSE, texture->coordinates_matrix);
        }

        if (textureshaderinfo->dimensions_id >= 0) {
            float dimensions[2] = { texture->width, texture->height };
            glUniform2fv(textureshaderinfo->dimensions_id, 1, dimensions);
        }
    }

    if (shader->modelview_matrix_location_id >= 0) {
        glUniformMatrix4fv(shader->modelview_matrix_location_id, 1, GL_FALSE, node->modelview_matrix);
    }

    if (shader->projection_matrix_location_id >= 0) {
        glUniformMatrix4fv(shader->projection_matrix_location_id, 1, GL_FALSE, node->projection_matrix);
    }

    if (shader->normal_matrix_location_id >= 0) {
        float normal_matrix[3*3];
        ngli_mat3_from_mat4(normal_matrix, node->modelview_matrix);
        ngli_mat3_inverse(normal_matrix, normal_matrix);
        ngli_mat3_transpose(normal_matrix, normal_matrix);
        glUniformMatrix3fv(shader->normal_matrix_location_id, 1, GL_FALSE, normal_matrix);
    }

    return 0;
}

static int update_vertex_attribs(struct ngl_node *node)
{
    struct texturedshape *s = node->priv_data;
    struct shape *shape = s->shape->priv_data;
    struct shader *shader = s->shader->priv_data;

    for (int i = 0; i < NGLI_ARRAY_NB(s->textures); i++)  {
        struct textureshaderinfo *textureshaderinfo = &s->textureshaderinfos[i];
        if (textureshaderinfo->coordinates_id >= 0) {
            glEnableVertexAttribArray(textureshaderinfo->coordinates_id);
            glBindBuffer(GL_ARRAY_BUFFER, shape->texcoords_buffer_id);
            glVertexAttribPointer(textureshaderinfo->coordinates_id, 2, GL_FLOAT, GL_FALSE, NGLI_SHAPE_VERTICES_STRIDE(shape), NULL);
        }
    }

    if (shader->position_location_id >= 0) {
        glEnableVertexAttribArray(shader->position_location_id);
        glBindBuffer(GL_ARRAY_BUFFER, shape->vertices_buffer_id);
        glVertexAttribPointer(shader->position_location_id, 3, GL_FLOAT, GL_FALSE, NGLI_SHAPE_VERTICES_STRIDE(shape), NULL);
    }

    if (shader->normal_location_id >= 0) {
        glEnableVertexAttribArray(shader->normal_location_id);
        glBindBuffer(GL_ARRAY_BUFFER, shape->normals_buffer_id);
        glVertexAttribPointer(shader->normal_location_id, 3, GL_FLOAT, GL_FALSE, NGLI_SHAPE_VERTICES_STRIDE(shape), NULL);
    }

    return 0;
}

static int texturedshape_init(struct ngl_node *node)
{
    int ret;
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    struct texturedshape *s = node->priv_data;
    struct shader *shader = s->shader->priv_data;

    ret = ngli_node_init(s->shape);
    if (ret < 0)
        return ret;

    ret = ngli_node_init(s->shader);
    if (ret < 0)
        return ret;

    s->uniform_ids = calloc(s->nb_uniforms, sizeof(*s->uniform_ids));
    if (!s->uniform_ids)
        return -1;

    for (int i = 0; i < s->nb_uniforms; i++) {
        struct ngl_node *unode = s->uniforms[i];
        struct uniform *u = unode->priv_data;
        ret = ngli_node_init(unode);
        if (ret < 0)
            return ret;
        s->uniform_ids[i] = glGetUniformLocation(shader->program_id, u->name);
    }

    s->attribute_ids = calloc(s->nb_attributes, sizeof(*s->attribute_ids));
    if (!s->attribute_ids)
        return -1;

    for (int i = 0; i < s->nb_attributes; i++) {
        struct ngl_node *unode = s->attributes[i];
        struct attribute *u = unode->priv_data;
        ret = ngli_node_init(unode);
        if (ret < 0)
            return ret;
        s->attribute_ids[i] = glGetAttribLocation(shader->program_id, u->name);
    }

    for (int i = 0; i < NGLI_ARRAY_NB(s->textures); i++)  {
        char name[32];

        if (s->textures[i]) {
            ret = ngli_node_init(s->textures[i]);
            if (ret < 0)
                return ret;

            snprintf(name, sizeof(name), "tex%d_sampler", i);
            s->textureshaderinfos[i].sampler_id = glGetUniformLocation(shader->program_id, name);

            snprintf(name, sizeof(name), "tex%d_external_sampler", i);
            s->textureshaderinfos[i].sampler_external_id = glGetUniformLocation(shader->program_id, name);

            snprintf(name, sizeof(name), "tex%d_coords", i);
            s->textureshaderinfos[i].coordinates_id = glGetAttribLocation(shader->program_id, name);

            snprintf(name, sizeof(name), "tex%d_coords_matrix", i);
            s->textureshaderinfos[i].coordinates_mvp_id = glGetUniformLocation(shader->program_id, name);

            snprintf(name, sizeof(name), "tex%d_dimensions", i);
            s->textureshaderinfos[i].dimensions_id = glGetUniformLocation(shader->program_id, name);
        }
    }

    if (glcontext->has_vao_compatibility) {
        glcontext->glGenVertexArrays(1, &s->vao_id);
        glcontext->glBindVertexArray(s->vao_id);
        update_vertex_attribs(node);
    }

    return 0;
}

static void texturedshape_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    struct texturedshape *s = node->priv_data;

    if (glcontext->has_vao_compatibility) {
        glcontext->glDeleteVertexArrays(1, &s->vao_id);
    }

    free(s->uniform_ids);
    free(s->attribute_ids);
}

static void texturedshape_update(struct ngl_node *node, double t)
{
    struct texturedshape *s = node->priv_data;

    ngli_node_update(s->shape, t);

    for (int i = 0; i < NGLI_ARRAY_NB(s->textures); i++) {
        if (s->textures[i]) {
            ngli_node_update(s->textures[i], t);
        }
    }

    for (int i = 0; i < s->nb_uniforms; i++) {
        ngli_node_update(s->uniforms[i], t);
    }

    ngli_node_update(s->shader, t);
}

static void texturedshape_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    struct texturedshape *s = node->priv_data;
    const struct shader *shader = s->shader->priv_data;
    const struct shape *shape = s->shape->priv_data;

    glUseProgram(shader->program_id);

    if (glcontext->has_vao_compatibility) {
        glcontext->glBindVertexArray(s->vao_id);
    }

    update_uniforms(node);

    if (!glcontext->has_vao_compatibility) {
        update_vertex_attribs(node);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape->indices_buffer_id);
    glDrawElements(shape->draw_mode, shape->nb_indices, shape->draw_type, 0);
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
