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
#include <limits.h>

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
    {"nb_group_x", PARAM_TYPE_INT,      OFFSET(nb_group_x), .flags=PARAM_FLAG_CONSTRUCTOR,
                   .desc=NGLI_DOCSTRING("number of work groups to be executed in the x dimension")},
    {"nb_group_y", PARAM_TYPE_INT,      OFFSET(nb_group_y), .flags=PARAM_FLAG_CONSTRUCTOR,
                   .desc=NGLI_DOCSTRING("number of work groups to be executed in the y dimension")},
    {"nb_group_z", PARAM_TYPE_INT,      OFFSET(nb_group_z), .flags=PARAM_FLAG_CONSTRUCTOR,
                   .desc=NGLI_DOCSTRING("number of work groups to be executed in the z dimension")},
    {"program",    PARAM_TYPE_NODE,     OFFSET(program),    .flags=PARAM_FLAG_CONSTRUCTOR, .node_types=PROGRAMS_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("compute program to be executed")},
    {"textures",   PARAM_TYPE_NODEDICT, OFFSET(textures),   .node_types=TEXTURES_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("input and output textures made accessible to the compute `program`")},
    {"uniforms",   PARAM_TYPE_NODEDICT, OFFSET(uniforms),   .node_types=UNIFORMS_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("uniforms made accessible to the compute `program`")},
    {"buffers",    PARAM_TYPE_NODEDICT, OFFSET(buffers),    .node_types=BUFFERS_TYPES_LIST,
                   .desc=NGLI_DOCSTRING("input and output buffers made accessible to the compute `program`")},
    {NULL}
};

static int acquire_next_available_texture_unit(uint64_t *used_texture_units)
{
    for (int i = 0; i < sizeof(*used_texture_units) * 8; i++) {
        if (!(*used_texture_units & (1 << i))) {
            *used_texture_units |= (1 << i);
            return i;
        }
    }
    return -1;
}

#ifdef TARGET_ANDROID
static void update_sampler2D(const struct glcontext *gl,
                             struct compute *s,
                             struct texture *texture,
                             struct textureprograminfo *info,
                             int *unit_index,
                             int *sampling_mode)
{
    if (info->sampler_id >= 0 || info->external_sampler_id >= 0)
        ngli_glActiveTexture(gl, GL_TEXTURE0 + *unit_index);

    if (info->external_sampler_id >= 0) {
        ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, 0);
        ngli_glUniform1i(gl, info->external_sampler_id, s->disabled_texture_unit);
    }

    if (info->sampler_id >= 0) {
        *sampling_mode = NGLI_SAMPLING_MODE_2D;
        ngli_glBindTexture(gl, texture->target, texture->id);
        ngli_glUniform1i(gl, info->sampler_id, *unit_index);
    }
}

static void update_external_sampler(const struct glcontext *gl,
                                    struct compute *s,
                                    struct texture *texture,
                                    struct textureprograminfo *info,
                                    int *unit_index,
                                    int *sampling_mode)
{
    if (info->sampler_id >= 0 || info->external_sampler_id >= 0)
        ngli_glActiveTexture(gl, GL_TEXTURE0 + *unit_index);

    if (info->sampler_id >= 0) {
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        ngli_glUniform1i(gl, info->sampler_id, s->disabled_texture_unit);
    }

    if (info->external_sampler_id >= 0) {
        *sampling_mode = NGLI_SAMPLING_MODE_EXTERNAL_OES;
        ngli_glBindTexture(gl, texture->target, texture->id);
        ngli_glUniform1i(gl, info->external_sampler_id, *unit_index);
    }
}
#else
static void update_sampler2D(const struct glcontext *gl,
                             struct compute *s,
                             struct texture *texture,
                             struct textureprograminfo *info,
                             int *unit_index,
                             int *sampling_mode)
{
    if (info->sampler_id) {
        *sampling_mode = NGLI_SAMPLING_MODE_2D;

        ngli_glActiveTexture(gl, GL_TEXTURE0 + *unit_index);
        ngli_glBindTexture(gl, texture->target, texture->id);
        ngli_glUniform1i(gl, info->sampler_id, *unit_index);
    }
}
#endif

static void update_sampler3D(const struct glcontext *gl,
                             struct compute *s,
                             struct texture *texture,
                             struct textureprograminfo *info,
                             int *unit_index,
                             int *sampling_mode)
{
    if (info->sampler_id) {
        *sampling_mode = NGLI_SAMPLING_MODE_2D;

        ngli_glActiveTexture(gl, GL_TEXTURE0 + *unit_index);
        ngli_glBindTexture(gl, texture->target, texture->id);
        ngli_glUniform1i(gl, info->sampler_id, *unit_index);
    }
}

static int update_uniforms(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct compute *s = node->priv_data;

    if (s->textures) {
        uint64_t used_texture_units = s->used_texture_units;

        if (s->disabled_texture_unit >= 0) {
            ngli_glActiveTexture(gl, GL_TEXTURE0 + s->disabled_texture_unit);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, s->disabled_texture_unit);
#ifdef TARGET_ANDROID
            ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, s->disabled_texture_unit);
#endif
        }

        for (int i = 0; i < s->nb_textureprograminfos; i++) {
            struct textureprograminfo *info = &s->textureprograminfos[i];
            const struct ngl_node *tnode = ngli_hmap_get(s->textures, info->name);
            if (!tnode)
                continue;
            struct texture *texture = tnode->priv_data;

            if (info->sampler_type == GL_IMAGE_2D) {
                LOG(VERBOSE,
                    "image at location=%d will use texture_unit=%d",
                    info->sampler_id,
                    info->sampler_value);
                if (info->sampler_id >= 0) {
                    ngli_glBindImageTexture(gl,
                                            info->sampler_value,
                                            texture->id,
                                            0,
                                            GL_FALSE,
                                            0,
                                            texture->access,
                                            texture->internal_format);
                }
                if (info->dimensions_id >= 0) {
                    const float dimensions[2] = {texture->width, texture->height};
                    ngli_glUniform2fv(gl, info->dimensions_id, 1, dimensions);
                }
            } else {
                int texture_index = acquire_next_available_texture_unit(&used_texture_units);
                if (texture_index < 0) {
                    LOG(ERROR, "no texture unit available");
                    return -1;
                }
                LOG(VERBOSE,
                    "sampler at location=%d will use texture_unit=%d",
                    info->sampler_id,
                    texture_index);
                int sampling_mode = NGLI_SAMPLING_MODE_NONE;
                switch (texture->target) {
                case GL_TEXTURE_2D:
                    if (info->sampler_type != GL_SAMPLER_2D) {
                        LOG(ERROR, "sampler type (0x%x) does not match texture target (0x%x)",
                            info->sampler_type, texture->target);
                        return -1;
                    }
                    update_sampler2D(gl, s, texture, info, &texture_index, &sampling_mode);
                    break;
                case GL_TEXTURE_3D:
                    if (info->sampler_type != GL_SAMPLER_3D) {
                        LOG(ERROR, "sampler type (0x%x) does not match texture target (0x%x)",
                            info->sampler_type, texture->target);
                        return -1;
                    }
                    update_sampler3D(gl, s, texture, info, &texture_index, &sampling_mode);
                    break;
#ifdef TARGET_ANDROID
                case GL_TEXTURE_EXTERNAL_OES:
                    if (info->sampler_type != GL_SAMPLER_EXTERNAL_OES) {
                        LOG(ERROR, "sampler type (0x%x) does not match texture target (0x%x)",
                            info->sampler_type, texture->target);
                        return -1;
                    }
                    update_external_sampler(gl, s, texture, info, &texture_index, &sampling_mode);
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

                if (info->ts_id >= 0)
                    ngli_glUniform1f(gl, info->ts_id, texture->data_src_ts);
            }
        }
    }

    for (int i = 0; i < s->nb_uniform_ids; i++) {
        struct uniformprograminfo *info = &s->uniform_ids[i];
        const GLint uid = info->id;
        if (uid < 0)
            continue;
        const struct ngl_node *unode = ngli_hmap_get(s->uniforms, info->name);
        const struct uniform *u = unode->priv_data;
        switch (unode->class->id) {
        case NGL_NODE_UNIFORMFLOAT:
            ngli_glUniform1f(gl, uid, u->scalar);
            break;
        case NGL_NODE_UNIFORMVEC2:
            ngli_glUniform2fv(gl, uid, 1, u->vector);
            break;
        case NGL_NODE_UNIFORMVEC3:
            ngli_glUniform3fv(gl, uid, 1, u->vector);
            break;
        case NGL_NODE_UNIFORMVEC4:
            ngli_glUniform4fv(gl, uid, 1, u->vector);
            break;
        case NGL_NODE_UNIFORMINT:
            ngli_glUniform1i(gl, uid, u->ival);
            break;
        case NGL_NODE_UNIFORMQUAT: {
            GLenum type = s->uniform_ids[i].type;
            if (type == GL_FLOAT_MAT4)
                ngli_glUniformMatrix4fv(gl, uid, 1, GL_FALSE, u->matrix);
            else if (type == GL_FLOAT_VEC4)
                ngli_glUniform4fv(gl, uid, 1, u->vector);
            else
                LOG(ERROR,
                    "quaternion uniform '%s' must be declared as vec4 or mat4 in the shader",
                    info->name);
            break;
        }
        case NGL_NODE_UNIFORMMAT4:
            ngli_glUniformMatrix4fv(gl, uid, 1, GL_FALSE, u->matrix);
            break;
        default:
            LOG(ERROR, "unsupported uniform of type %s", unode->class->name);
            break;
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

static int remove_suffix(char *dst, size_t dst_size, const char *src, const char *suffix)
{
    size_t suffix_len = strlen(suffix);
    size_t suffix_pos = strlen(src) - suffix_len;
    if (suffix_pos <= 0 || suffix_pos >= INT_MAX)
        return -1;
    if (strcmp(src + suffix_pos, suffix))
        return -1;
    snprintf(dst, dst_size, "%.*s", (int)suffix_pos, src);
    return 0;
}

static int compute_init(struct ngl_node *node)
{
    int ret;

    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct compute *s = node->priv_data;
    struct computeprogram *program = s->program->priv_data;

    if (!(gl->features & NGLI_FEATURE_COMPUTE_SHADER_ALL)) {
        LOG(ERROR, "context does not support compute shaders");
        return -1;
    }

    if (s->nb_group_x > gl->max_compute_work_group_counts[0] ||
        s->nb_group_y > gl->max_compute_work_group_counts[1] ||
        s->nb_group_z > gl->max_compute_work_group_counts[2]) {
        LOG(ERROR,
            "compute work group size (%d, %d, %d) exceeds driver limit (%d, %d, %d)",
            s->nb_group_x,
            s->nb_group_y,
            s->nb_group_z,
            gl->max_compute_work_group_counts[0],
            gl->max_compute_work_group_counts[1],
            gl->max_compute_work_group_counts[2]);
        return -1;
    }

    ret = ngli_node_init(s->program);
    if (ret < 0)
        return ret;

    s->disabled_texture_unit = -1;

    int nb_textures = s->textures ? ngli_hmap_count(s->textures) : 0;
    int max_nb_textures = NGLI_MIN(gl->max_texture_image_units, sizeof(s->used_texture_units) * 8);
    if (nb_textures > max_nb_textures) {
        LOG(ERROR, "attached textures count (%d) exceeds driver limit (%d)",
            nb_textures, gl->max_texture_image_units);
        return -1;
    }

    if (nb_textures > 0) {
        s->textureprograminfos = calloc(nb_textures, sizeof(*s->textureprograminfos));
        if (!s->textureprograminfos)
            return -1;

        for (int i = 0; i < program->nb_active_uniforms; i++) {
            struct uniformprograminfo *active_uniform = &program->active_uniforms[i];

            if (active_uniform->type == GL_IMAGE_2D) {
                struct ngl_node *tnode = ngli_hmap_get(s->textures, active_uniform->name);
                if (!tnode)
                    return -1;
                ret = ngli_node_init(tnode);
                if (ret < 0)
                    return ret;
                struct texture *texture = tnode->priv_data;
                texture->direct_rendering = 0;

                struct textureprograminfo *info = &s->textureprograminfos[s->nb_textureprograminfos++];
                snprintf(info->name, sizeof(info->name), "%s", active_uniform->name);
                info->sampler_id = active_uniform->id;
                info->sampler_type = active_uniform->type;
                char name[128];
                snprintf(name, sizeof(name), "%s_dimensions", active_uniform->name);
                info->dimensions_id = ngli_glGetUniformLocation(gl, program->program_id, name);
                ngli_glGetUniformiv(gl, program->program_id, info->sampler_id, &info->sampler_value);
                if (info->sampler_value >= max_nb_textures) {
                    LOG(ERROR, "maximum number (%d) of texture unit reached", max_nb_textures);
                    return -1;
                }
                if (s->used_texture_units & (1 << info->sampler_value)) {
                    LOG(ERROR, "texture unit %d is already used by another image", info->sampler_value);
                    return -1;
                }
                s->used_texture_units |= 1 << info->sampler_value;
            } else if (active_uniform->type == GL_SAMPLER_2D ||
                       active_uniform->type == GL_SAMPLER_3D ||
                       active_uniform->type == GL_SAMPLER_EXTERNAL_OES) {
                char key[64];
                const char *suffix = active_uniform->type == GL_SAMPLER_EXTERNAL_OES ? "_external_sampler"
                                                                                     : "_sampler";
                ret = remove_suffix(key, sizeof(key), active_uniform->name, suffix);
                if (ret < 0)
                    continue;

                struct ngl_node *tnode = ngli_hmap_get(s->textures, key);
                if (!tnode)
                    return -1;
                ret = ngli_node_init(tnode);
                if (ret < 0)
                    return ret;

                struct textureprograminfo *info = &s->textureprograminfos[s->nb_textureprograminfos++];
                snprintf(info->name, sizeof(info->name), "%s", key);
                info->sampler_type = active_uniform->type;

#define GET_TEXTURE_UNIFORM_LOCATION(suffix) do {                                              \
                char name[128];                                                                \
                snprintf(name, sizeof(name), "%s_" #suffix, key);                              \
                info->suffix##_id = ngli_glGetUniformLocation(gl, program->program_id, name);  \
} while (0)

                GET_TEXTURE_UNIFORM_LOCATION(sampling_mode);
                GET_TEXTURE_UNIFORM_LOCATION(sampler);
#if defined(TARGET_ANDROID)
                GET_TEXTURE_UNIFORM_LOCATION(external_sampler);
#elif defined(TARGET_IPHONE)
                GET_TEXTURE_UNIFORM_LOCATION(y_sampler);
                GET_TEXTURE_UNIFORM_LOCATION(uv_sampler);
#endif
                GET_TEXTURE_UNIFORM_LOCATION(coord_matrix);
                GET_TEXTURE_UNIFORM_LOCATION(dimensions);
                GET_TEXTURE_UNIFORM_LOCATION(ts);

#undef GET_TEXTURE_UNIFORM_LOCATION

#if TARGET_ANDROID
                if (info->sampler_id < 0 &&
                    info->external_sampler_id < 0) {
                    LOG(WARNING, "no sampler found for texture %s", key);
                }

                if (info->sampler_id >= 0 &&
                    info->external_sampler_id >= 0) {
                    s->disabled_texture_unit = acquire_next_available_texture_unit(&s->used_texture_units);
                    if (s->disabled_texture_unit < 0) {
                        LOG(ERROR, "no texture unit available");
                        return -1;
                    }
                }

                struct texture *texture = tnode->priv_data;
                texture->direct_rendering = texture->direct_rendering &&
                                            info->external_sampler_id >= 0;
                LOG(INFO,
                    "direct rendering %s available for texture %s",
                    texture->direct_rendering ? "is" : "is not",
                    key);
#else
                if (info->sampler_id < 0) {
                    LOG(WARNING, "no sampler found for texture %s", key);
                }
#endif
            }
        }
    }

    int nb_uniforms = s->uniforms ? ngli_hmap_count(s->uniforms) : 0;
    if (nb_uniforms > 0) {
        s->uniform_ids = calloc(nb_uniforms, sizeof(*s->uniform_ids));
        if (!s->uniform_ids)
            return -1;

        for (int i = 0; i < program->nb_active_uniforms; i++) {
            struct uniformprograminfo *active_uniform = &program->active_uniforms[i];
            struct ngl_node *unode = ngli_hmap_get(s->uniforms, active_uniform->name);
            if (!unode)
                continue;

            ret = ngli_node_init(unode);
            if (ret < 0)
                return ret;

            struct uniformprograminfo *infop = &s->uniform_ids[s->nb_uniform_ids++];
            *infop = *active_uniform;
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
            struct buffer *buffer = unode->priv_data;
            buffer->generate_gl_buffer = 1;

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
    struct glcontext *gl = ctx->glcontext;

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
