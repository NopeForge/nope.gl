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

#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_UNIFORMFLOAT,      \
                                          NGL_NODE_UNIFORMVEC2,       \
                                          NGL_NODE_UNIFORMVEC3,       \
                                          NGL_NODE_UNIFORMVEC4,       \
                                          NGL_NODE_UNIFORMQUAT,       \
                                          NGL_NODE_UNIFORMINT,        \
                                          NGL_NODE_UNIFORMMAT4,       \
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

#define TEXTURES_TYPES_LIST (const int[]){NGL_NODE_TEXTURE2D,       \
                                          NGL_NODE_TEXTURE3D,       \
                                          -1}

#define BUFFERS_TYPES_LIST (const int[]){NGL_NODE_BUFFERFLOAT,   \
                                         NGL_NODE_BUFFERVEC2,    \
                                         NGL_NODE_BUFFERVEC3,    \
                                         NGL_NODE_BUFFERVEC4,    \
                                         NGL_NODE_BUFFERINT,     \
                                         NGL_NODE_BUFFERIVEC2,   \
                                         NGL_NODE_BUFFERIVEC3,   \
                                         NGL_NODE_BUFFERIVEC4,   \
                                         NGL_NODE_BUFFERUINT,    \
                                         NGL_NODE_BUFFERUIVEC2,  \
                                         NGL_NODE_BUFFERUIVEC3,  \
                                         NGL_NODE_BUFFERUIVEC4,  \
                                         -1}

#define OFFSET(x) offsetof(struct render, x)
static const struct node_param render_params[] = {
    {"geometry", PARAM_TYPE_NODE, OFFSET(geometry), .flags=PARAM_FLAG_CONSTRUCTOR,
                 .node_types=GEOMETRY_TYPES_LIST},
    {"program",  PARAM_TYPE_NODE, OFFSET(program),
                 .node_types=(const int[]){NGL_NODE_PROGRAM, -1}},
    {"textures", PARAM_TYPE_NODEDICT, OFFSET(textures),
                 .node_types=TEXTURES_TYPES_LIST},
    {"uniforms", PARAM_TYPE_NODEDICT, OFFSET(uniforms),
                 .node_types=UNIFORMS_TYPES_LIST},
    {"attributes", PARAM_TYPE_NODEDICT, OFFSET(attributes),
                 .node_types=ATTRIBUTES_TYPES_LIST},
    {"buffers",  PARAM_TYPE_NODEDICT, OFFSET(buffers),
                 .node_types=BUFFERS_TYPES_LIST},
    {NULL}
};

static inline void bind_texture(const struct glfunctions *gl, GLenum target, GLint uniform_location, GLuint texture_id, int idx)
{
    ngli_glActiveTexture(gl, GL_TEXTURE0 + idx);
    ngli_glBindTexture(gl, target, texture_id);
    ngli_glUniform1i(gl, uniform_location, idx);
}

#define SAMPLING_MODE_NONE         0
#define SAMPLING_MODE_2D           1
#define SAMPLING_MODE_EXTERNAL_OES 2

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
            case NGL_NODE_UNIFORMQUAT:   ngli_glUniformMatrix4fv(gl, uid, 1, GL_FALSE, u->matrix); break;
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
        int texture_index = 0;
        const struct hmap_entry *entry = NULL;

        if (s->disable_1st_texture_unit) {
            ngli_glActiveTexture(gl, GL_TEXTURE0);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
#ifdef TARGET_ANDROID
            ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, 0);
#endif
            texture_index = 1;
        }

        while ((entry = ngli_hmap_next(s->textures, entry))) {
            struct ngl_node *tnode = entry->data;
            struct texture *texture = tnode->priv_data;
            struct textureprograminfo *info = &s->textureprograminfos[i];

            int sampling_mode = SAMPLING_MODE_NONE;
            switch (texture->target) {
            case GL_TEXTURE_2D:
                if (info->sampler_id >= 0) {
                    sampling_mode = SAMPLING_MODE_2D;
                    bind_texture(gl, texture->target, info->sampler_id, texture->id, texture_index);
                }

                if (info->external_sampler_id >= 0)
                    ngli_glUniform1i(gl, info->external_sampler_id, 0);
                break;
            case GL_TEXTURE_3D:
                bind_texture(gl, texture->target, info->sampler_id, texture->id, texture_index);
                break;
#ifdef TARGET_ANDROID
            case GL_TEXTURE_EXTERNAL_OES:
                if (info->sampler_id >= 0)
                    ngli_glUniform1i(gl, info->sampler_id, 0);

                if (info->external_sampler_id >= 0) {
                    sampling_mode = SAMPLING_MODE_EXTERNAL_OES;
                    bind_texture(gl, texture->target, info->external_sampler_id, texture->id, texture_index);
                }
                break;
#endif
            }

            if (info->sampling_mode_id >= 0)
                ngli_glUniform1i(gl, info->sampling_mode_id, sampling_mode);

            if (info->coord_matrix_id >= 0)
                ngli_glUniformMatrix4fv(gl, info->coord_matrix_id, 1, GL_FALSE, texture->coordinates_matrix);

            if (info->dimensions_id >= 0) {
                const float dimensions[3] = {texture->width, texture->height, texture->depth};
                if (texture->target == GL_TEXTURE_3D)
                    ngli_glUniform3fv(gl, info->dimensions_id, 1, dimensions);
                else
                    ngli_glUniform2fv(gl, info->dimensions_id, 1, dimensions);
            }

            i++;
            texture_index++;
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

    if (geometry->uvcoords_buffer) {
        struct buffer *buffer = geometry->uvcoords_buffer->priv_data;
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

static int update_buffers(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct render *s = node->priv_data;

    if (glcontext->has_ssbo_compatibility && s->buffers) {
        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->buffers, entry))) {
            const struct ngl_node *bnode = entry->data;
            const struct buffer *b = bnode->priv_data;
            ngli_glBindBufferBase(gl, GL_SHADER_STORAGE_BUFFER, s->buffer_ids[i], b->buffer_id);
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

    ret = ngli_node_init(s->geometry);
    if (ret < 0)
        return ret;

    if (!s->program) {
        s->program = ngl_node_create(NGL_NODE_PROGRAM);
        if (!s->program)
            return -1;
        ngli_node_attach_ctx(s->program, ctx);
    }

    ret = ngli_node_init(s->program);
    if (ret < 0)
        return ret;

    struct program *program = s->program->priv_data;

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
            struct ngl_node *tnode = entry->data;

            ret = ngli_node_init(tnode);
            if (ret < 0)
                return ret;

            struct textureprograminfo *info = &s->textureprograminfos[i];

#define GET_TEXTURE_UNIFORM_LOCATION(suffix) do {                                          \
            char name[128];                                                                \
            snprintf(name, sizeof(name), "%s_" #suffix, entry->key);                       \
            info->suffix##_id = ngli_glGetUniformLocation(gl, program->program_id, name);  \
} while (0)

            GET_TEXTURE_UNIFORM_LOCATION(sampling_mode);
            GET_TEXTURE_UNIFORM_LOCATION(sampler);
            GET_TEXTURE_UNIFORM_LOCATION(external_sampler);
            GET_TEXTURE_UNIFORM_LOCATION(coord_matrix);
            GET_TEXTURE_UNIFORM_LOCATION(dimensions);

#undef GET_TEXTURE_UNIFORM_LOCATION

            if (info->sampler_id < 0 &&
                info->external_sampler_id < 0) {
                LOG(WARNING, "no sampler found for texture %s", entry->key);
            }

            if (info->sampler_id >= 0 &&
                info->external_sampler_id >= 0)
                s->disable_1st_texture_unit = 1;

            struct texture *texture = tnode->priv_data;
            texture->direct_rendering = texture->direct_rendering &&
                                        info->external_sampler_id >= 0;
            LOG(VERBOSE,
                "direct rendering %s available for texture %s",
                texture->direct_rendering ? "is" : "is not",
                entry->key);

            i++;
        }
    }

    int nb_buffers = s->buffers ? ngli_hmap_count(s->buffers) : 0;
    if (glcontext->has_ssbo_compatibility && nb_buffers > 0) {
        s->buffer_ids = calloc(nb_buffers, sizeof(*s->buffer_ids));
        if (!s->buffer_ids)
            return -1;

        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->buffers, entry))) {
            struct ngl_node *unode = entry->data;
            ret = ngli_node_init(unode);
            if (ret < 0)
                return ret;

            static const GLenum props[] = {GL_BUFFER_BINDING};
            GLsizei nb_props = 1;
            GLint params = 0;
            GLsizei nb_params = 1;
            GLsizei nb_params_ret = 0;

            GLuint index = ngli_glGetProgramResourceIndex(gl,
                                                          program->program_id,
                                                          GL_SHADER_STORAGE_BLOCK,
                                                          entry->key);

            if (index != GL_INVALID_INDEX)
                ngli_glGetProgramResourceiv(gl,
                                            program->program_id,
                                            GL_SHADER_STORAGE_BLOCK,
                                            index,
                                            nb_props,
                                            props,
                                            nb_params,
                                            &nb_params_ret,
                                            &params);

            s->buffer_ids[i] = params;
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
    free(s->buffer_ids);
}


static int render_update(struct ngl_node *node, double t)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    struct render *s = node->priv_data;

    int ret = ngli_node_update(s->geometry, t);
    if (ret < 0)
        return ret;

    if (s->textures) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    if (s->uniforms) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->uniforms, entry))) {
            ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    if (glcontext->has_ssbo_compatibility && s->buffers) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->buffers, entry))) {
            int ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    return ngli_node_update(s->program, t);
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
    update_buffers(node);

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
    .file      = __FILE__,
};
