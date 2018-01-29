/*
 * Copyright 2017 GoPro Inc.
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
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define TEXTURES_TYPES_LIST (const int[]){NGL_NODE_TEXTURE2D,      \
                                          -1}

#define PROGRAMS_TYPES_LIST (const int[]){NGL_NODE_COMPUTEPROGRAM, \
                                          -1}

#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_UNIFORMFLOAT,   \
                                          NGL_NODE_UNIFORMVEC2,    \
                                          NGL_NODE_UNIFORMVEC3,    \
                                          NGL_NODE_UNIFORMVEC4,    \
                                          NGL_NODE_UNIFORMQUAT,    \
                                          NGL_NODE_UNIFORMINT,     \
                                          NGL_NODE_UNIFORMMAT4,    \
                                          -1}

#define BUFFERS_TYPES_LIST (const int[]) {NGL_NODE_BUFFERFLOAT,      \
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

#define OFFSET(x) offsetof(struct compute, x)
static const struct node_param compute_params[] = {
    {"nb_group_x", PARAM_TYPE_INT,      OFFSET(nb_group_x), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"nb_group_y", PARAM_TYPE_INT,      OFFSET(nb_group_y), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"nb_group_z", PARAM_TYPE_INT,      OFFSET(nb_group_z), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"program",    PARAM_TYPE_NODE,     OFFSET(program),    .flags=PARAM_FLAG_CONSTRUCTOR, .node_types=PROGRAMS_TYPES_LIST},
    {"textures",   PARAM_TYPE_NODEDICT, OFFSET(textures),   .node_types=TEXTURES_TYPES_LIST},
    {"uniforms",   PARAM_TYPE_NODEDICT, OFFSET(uniforms),   .node_types=UNIFORMS_TYPES_LIST},
    {"buffers",    PARAM_TYPE_NODEDICT, OFFSET(buffers),    .node_types=BUFFERS_TYPES_LIST},
    {NULL}
};

static int update_uniforms(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct compute *s = node->priv_data;

    if (s->textures) {
        int i = 0;
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            struct ngl_node *tnode = entry->data;
            struct texture *texture = tnode->priv_data;
            struct textureprograminfo *textureprograminfo = &s->textureprograminfos[i];

            if (textureprograminfo->sampler_id >= 0) {
                GLenum format = ngli_texture_get_sized_internal_format(glcontext,
                                                                       texture->internal_format,
                                                                       texture->type);

                ngli_glBindImageTexture(gl,
                                        textureprograminfo->sampler_id,
                                        texture->id,
                                        0,
                                        GL_FALSE,
                                        0,
                                        texture->access,
                                        format);
            }

            if (textureprograminfo->dimensions_id >= 0) {
                const float dimensions[2] = {texture->width, texture->height};
                ngli_glUniform2fv(gl, textureprograminfo->dimensions_id, 1, dimensions);
            }

            i++;
        }
    }

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

    if (s->buffers) {
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

static int compute_init(struct ngl_node *node)
{
    int ret;

    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct compute *s = node->priv_data;
    struct computeprogram *program = s->program->priv_data;

    if (!(glcontext->features & NGLI_FEATURE_COMPUTE_SHADER_ALL)) {
        LOG(ERROR, "Context does not support compute shaders");
        return -1;
    }

    if (s->nb_group_x > glcontext->max_compute_work_group_counts[0] ||
        s->nb_group_y > glcontext->max_compute_work_group_counts[1] ||
        s->nb_group_z > glcontext->max_compute_work_group_counts[2]) {
        LOG(ERROR,
            "Compute work group size (%d, %d, %d) exceeds driver limit (%d, %d, %d)",
            s->nb_group_x,
            s->nb_group_y,
            s->nb_group_z,
            glcontext->max_compute_work_group_counts[0],
            glcontext->max_compute_work_group_counts[1],
            glcontext->max_compute_work_group_counts[2]);
        return -1;
    }

    ret = ngli_node_init(s->program);
    if (ret < 0)
        return ret;

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

            s->textureprograminfos[i].sampler_id = ngli_glGetUniformLocation(gl,
                                                                             program->program_id,
                                                                             entry->key);

            char name[128];
            snprintf(name, sizeof(name), "%s_dimensions", entry->key);
            s->textureprograminfos[i].dimensions_id = ngli_glGetUniformLocation(gl,
                                                                                program->program_id,
                                                                                name);
            i++;
        }
    }

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
            s->uniform_ids[i] = ngli_glGetUniformLocation(gl,
                                                          program->program_id,
                                                          entry->key);
            i++;
        }
    }


    int nb_buffers = s->buffers ? ngli_hmap_count(s->buffers) : 0;
    if (nb_buffers > 0) {
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

    return 0;
}

static void compute_uninit(struct ngl_node *node)
{
    struct compute *s = node->priv_data;

    free(s->textureprograminfos);
    free(s->uniform_ids);
    free(s->buffer_ids);
}

static int compute_update(struct ngl_node *node, double t)
{
    struct compute *s = node->priv_data;

    if (s->textures) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            int ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    if (s->uniforms) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->uniforms, entry))) {
            int ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    if (s->buffers) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->buffers, entry))) {
            int ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    return ngli_node_update(s->program, t);
}

static void compute_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct compute *s = node->priv_data;
    const struct computeprogram *program = s->program->priv_data;

    ngli_glUseProgram(gl, program->program_id);

    update_uniforms(node);

    ngli_glMemoryBarrier(gl, GL_ALL_BARRIER_BITS);
    ngli_glDispatchCompute(gl, s->nb_group_x, s->nb_group_y, s->nb_group_z);
    ngli_glMemoryBarrier(gl, GL_ALL_BARRIER_BITS);
}

const struct node_class ngli_compute_class = {
    .id        = NGL_NODE_COMPUTE,
    .name      = "Compute",
    .init      = compute_init,
    .uninit    = compute_uninit,
    .update    = compute_update,
    .draw      = compute_draw,
    .priv_size = sizeof(struct compute),
    .params    = compute_params,
    .file      = __FILE__,
};
