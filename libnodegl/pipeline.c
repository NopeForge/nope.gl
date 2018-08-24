/*
 * Copyright 2016-2018 GoPro Inc.
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

static struct pipeline *get_pipeline(struct ngl_node *node)
{
    struct pipeline *ret = NULL;
    if (node->class->id == NGL_NODE_RENDER) {
        struct render *s = node->priv_data;
        ret = &s->pipeline;
    } else if (node->class->id == NGL_NODE_COMPUTE) {
        struct compute *s = node->priv_data;
        ret = &s->pipeline;
    } else {
        ngli_assert(0);
    }
    return ret;
}

static int acquire_next_available_texture_unit(uint64_t *used_texture_units)
{
    for (int i = 0; i < sizeof(*used_texture_units) * 8; i++) {
        if (!(*used_texture_units & (1 << i))) {
            *used_texture_units |= (1 << i);
            return i;
        }
    }
    LOG(ERROR, "no texture unit available");
    return -1;
}

static int update_default_sampler(const struct glcontext *gl,
                                  struct pipeline *s,
                                  struct texture *texture,
                                  const struct textureprograminfo *info,
                                  uint64_t *used_texture_units,
                                  int *sampling_mode)
{
    ngli_assert(info->sampler_id >= 0);

    int texture_index = acquire_next_available_texture_unit(used_texture_units);
    if (texture_index < 0)
        return -1;

    *sampling_mode = NGLI_SAMPLING_MODE_2D;

    ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
    ngli_glBindTexture(gl, texture->target, texture->id);
    ngli_glUniform1i(gl, info->sampler_id, texture_index);
    return 0;
}

#if defined(TARGET_ANDROID) || defined(TARGET_IPHONE)
static const struct {
    const char *name;
    GLenum type;
} tex_specs[] = {
    [0] = {"2D",  GL_TEXTURE_2D},
#if defined(TARGET_ANDROID)
    [1] = {"OES", GL_TEXTURE_EXTERNAL_OES},
#endif
};

static int get_disabled_texture_unit(const struct glcontext *gl,
                                     struct pipeline *s,
                                     uint64_t *used_texture_units,
                                     int type_index)
{
    int tex_unit = s->disabled_texture_unit[type_index];
    if (tex_unit >= 0)
        return tex_unit;

    tex_unit = acquire_next_available_texture_unit(used_texture_units);
    if (tex_unit < 0)
        return -1;

    LOG(DEBUG, "using texture unit %d for disabled %s textures",
        tex_unit, tex_specs[type_index].name);
    s->disabled_texture_unit[type_index] = tex_unit;

    ngli_glActiveTexture(gl, GL_TEXTURE0 + tex_unit);
    for (int i = 0; i < NGLI_ARRAY_NB(tex_specs); i++)
        ngli_glBindTexture(gl, tex_specs[i].type, 0);

    return tex_unit;
}
#endif

#if defined(TARGET_ANDROID)
static int update_sampler2D(const struct glcontext *gl,
                            struct pipeline *s,
                            struct texture *texture,
                            const struct textureprograminfo *info,
                            uint64_t *used_texture_units,
                            int *sampling_mode)
{
    if (texture->target == GL_TEXTURE_EXTERNAL_OES) {
        ngli_assert(info->external_sampler_id >= 0);

        *sampling_mode = NGLI_SAMPLING_MODE_EXTERNAL_OES;

        if (info->sampler_id >= 0) {
            int disabled_texture_unit = get_disabled_texture_unit(gl, s, used_texture_units, 0);
            if (disabled_texture_unit < 0)
                return -1;
            ngli_glUniform1i(gl, info->sampler_id, disabled_texture_unit);
        }

        int texture_index = acquire_next_available_texture_unit(used_texture_units);
        if (texture_index < 0)
            return -1;

        ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
        ngli_glBindTexture(gl, texture->target, texture->id);
        ngli_glUniform1i(gl, info->external_sampler_id, texture_index);

    } else if (info->sampler_id >= 0) {

        if (info->external_sampler_id >= 0) {
            int disabled_texture_unit = get_disabled_texture_unit(gl, s, used_texture_units, 1);
            if (disabled_texture_unit < 0)
                return -1;
            ngli_glUniform1i(gl, info->external_sampler_id, disabled_texture_unit);
        }

        return update_default_sampler(gl, s, texture, info, used_texture_units, sampling_mode);
    }
    return 0;
}
#elif defined(TARGET_IPHONE)
static int update_sampler2D(const struct glcontext *gl,
                             struct pipeline *s,
                             struct texture *texture,
                             const struct textureprograminfo *info,
                             uint64_t *used_texture_units,
                             int *sampling_mode)
{
    if (texture->upload_fmt == NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12_DR) {
        ngli_assert(info->y_sampler_id >= 0 || info->uv_sampler_id >= 0);

        *sampling_mode = NGLI_SAMPLING_MODE_NV12;

        if (info->sampler_id >= 0) {
            int disabled_texture_unit = get_disabled_texture_unit(gl, s, used_texture_units, 0);
            if (disabled_texture_unit < 0)
                return -1;
            ngli_glUniform1i(gl, info->sampler_id, disabled_texture_unit);
        }

        if (info->y_sampler_id >= 0) {
            int texture_index = acquire_next_available_texture_unit(used_texture_units);
            if (texture_index < 0)
                return -1;

            GLint id = CVOpenGLESTextureGetName(texture->ios_textures[0]);
            ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
            ngli_glBindTexture(gl, texture->target, id);
            ngli_glUniform1i(gl, info->y_sampler_id, texture_index);
        }

        if (info->uv_sampler_id >= 0) {
            int texture_index = acquire_next_available_texture_unit(used_texture_units);
            if (texture_index < 0)
                return -1;

            GLint id = CVOpenGLESTextureGetName(texture->ios_textures[1]);
            ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
            ngli_glBindTexture(gl, texture->target, id);
            ngli_glUniform1i(gl, info->uv_sampler_id, texture_index);
        }
    } else if (info->sampler_id >= 0) {

        if (info->y_sampler_id >= 0) {
            int disabled_texture_unit = get_disabled_texture_unit(gl, s, used_texture_units, 0);
            if (disabled_texture_unit < 0)
                return -1;
            ngli_glUniform1i(gl, info->y_sampler_id, disabled_texture_unit);
        }

        if (info->uv_sampler_id >= 0) {
            int disabled_texture_unit = get_disabled_texture_unit(gl, s, used_texture_units, 0);
            if (disabled_texture_unit < 0)
                return -1;
            ngli_glUniform1i(gl, info->uv_sampler_id, disabled_texture_unit);
        }

        return update_default_sampler(gl, s, texture, info, used_texture_units, sampling_mode);
    }
    return 0;
}
#else
static int update_sampler2D(const struct glcontext *gl,
                             struct pipeline *s,
                             struct texture *texture,
                             const struct textureprograminfo *info,
                             uint64_t *used_texture_units,
                             int *sampling_mode)
{
    if (info->sampler_id >= 0)
        return update_default_sampler(gl, s, texture, info, used_texture_units, sampling_mode);
    return 0;
}
#endif

static int update_sampler3D(const struct glcontext *gl,
                             struct pipeline *s,
                             struct texture *texture,
                             const struct textureprograminfo *info,
                             uint64_t *used_texture_units,
                             int *sampling_mode)
{
    if (info->sampler_id >= 0)
        return update_default_sampler(gl, s, texture, info, used_texture_units, sampling_mode);
    return 0;
}

static int update_images_and_samplers(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct pipeline *s = get_pipeline(node);

    if (s->textures) {
        uint64_t used_texture_units = s->used_texture_units;

        for (int i = 0; i < NGLI_ARRAY_NB(s->disabled_texture_unit); i++)
            s->disabled_texture_unit[i] = -1;

        for (int i = 0; i < s->nb_texture_pairs; i++) {
            const struct nodeprograminfopair *pair = &s->texture_pairs[i];
            const struct textureprograminfo *info = pair->program_info;
            const struct ngl_node *tnode = pair->node;
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
            } else {
                int sampling_mode = NGLI_SAMPLING_MODE_NONE;
                switch (texture->target) {
                case GL_TEXTURE_2D:
#if defined(TARGET_ANDROID)
                case GL_TEXTURE_EXTERNAL_OES:
#endif
                    update_sampler2D(gl, s, texture, info, &used_texture_units, &sampling_mode);
                    break;
                case GL_TEXTURE_3D:
                    update_sampler3D(gl, s, texture, info, &used_texture_units, &sampling_mode);
                    break;
                }

                if (info->sampling_mode_id >= 0)
                    ngli_glUniform1i(gl, info->sampling_mode_id, sampling_mode);
            }

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

    return 0;
}

static int update_uniforms(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct pipeline *s = get_pipeline(node);

    for (int i = 0; i < s->nb_uniform_pairs; i++) {
        const struct nodeprograminfopair *pair = &s->uniform_pairs[i];
        const struct uniformprograminfo *info = pair->program_info;
        const GLint uid = info->id;
        if (uid < 0)
            continue;
        const struct ngl_node *unode = pair->node;
        switch (unode->class->id) {
        case NGL_NODE_UNIFORMFLOAT: {
            const struct uniform *u = unode->priv_data;
            ngli_glUniform1f(gl, uid, u->scalar);
            break;
        }
        case NGL_NODE_UNIFORMVEC2: {
            const struct uniform *u = unode->priv_data;
            ngli_glUniform2fv(gl, uid, 1, u->vector);
            break;
        }
        case NGL_NODE_UNIFORMVEC3: {
            const struct uniform *u = unode->priv_data;
            ngli_glUniform3fv(gl, uid, 1, u->vector);
            break;
        }
        case NGL_NODE_UNIFORMVEC4: {
            const struct uniform *u = unode->priv_data;
            ngli_glUniform4fv(gl, uid, 1, u->vector);
            break;
        }
        case NGL_NODE_UNIFORMINT: {
            const struct uniform *u = unode->priv_data;
            ngli_glUniform1i(gl, uid, u->ival);
            break;
        }
        case NGL_NODE_UNIFORMQUAT: {
            const struct uniform *u = unode->priv_data;
            if (info->type == GL_FLOAT_MAT4)
                ngli_glUniformMatrix4fv(gl, uid, 1, GL_FALSE, u->matrix);
            else if (info->type == GL_FLOAT_VEC4)
                ngli_glUniform4fv(gl, uid, 1, u->vector);
            else
                LOG(ERROR,
                    "quaternion uniform '%s' must be declared as vec4 or mat4 in the shader",
                    pair->name);
            break;
        }
        case NGL_NODE_UNIFORMMAT4: {
            const struct uniform *u = unode->priv_data;
            ngli_glUniformMatrix4fv(gl, uid, 1, GL_FALSE, u->matrix);
            break;
        }
        case NGL_NODE_BUFFERFLOAT: {
            const struct buffer *buffer = unode->priv_data;
            ngli_glUniform1fv(gl, uid, buffer->count, (const GLfloat *)buffer->data);
            break;
        }
        case NGL_NODE_BUFFERVEC2: {
            const struct buffer *buffer = unode->priv_data;
            ngli_glUniform2fv(gl, uid, buffer->count, (const GLfloat *)buffer->data);
            break;
        }
        case NGL_NODE_BUFFERVEC3: {
            const struct buffer *buffer = unode->priv_data;
            ngli_glUniform3fv(gl, uid, buffer->count, (const GLfloat *)buffer->data);
            break;
        }
        case NGL_NODE_BUFFERVEC4: {
            const struct buffer *buffer = unode->priv_data;
            ngli_glUniform4fv(gl, uid, buffer->count, (const GLfloat *)buffer->data);
            break;
        }
        default:
            LOG(ERROR, "unsupported uniform of type %s", unode->class->name);
            break;
        }
    }

    return 0;
}

static int update_buffers(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct pipeline *s = get_pipeline(node);

    for (int i = 0; i < s->nb_buffer_pairs; i++) {
        const struct nodeprograminfopair *pair = &s->buffer_pairs[i];
        const struct ngl_node *bnode = pair->node;
        const struct buffer *buffer = bnode->priv_data;
        const struct bufferprograminfo *info = pair->program_info;
        ngli_glBindBufferBase(gl, GL_SHADER_STORAGE_BUFFER, info->binding, buffer->buffer_id);
    }

    return 0;
}

static struct uniformprograminfo *get_uniform_info(struct hmap *uniforms,
                                                   const char *basename,
                                                   const char *suffix)
{
    char name[MAX_ID_LEN];
    snprintf(name, sizeof(name), "%s%s", basename, suffix);
    return ngli_hmap_get(uniforms, name);
}

static int get_uniform_location(struct hmap *uniforms,
                                const char *basename,
                                const char *suffix)
{
    const struct uniformprograminfo *active_uniform = get_uniform_info(uniforms, basename, suffix);
    return active_uniform ? active_uniform->id : -1;
}

static void load_textureprograminfo(struct textureprograminfo *info,
                                    struct hmap *active_uniforms,
                                    const char *tex_key)
{
    const struct uniformprograminfo *sampler = get_uniform_info(active_uniforms, tex_key, "");
    if (!sampler) // Allow _sampler suffix
        sampler = get_uniform_info(active_uniforms, tex_key, "_sampler");
    if (sampler) {
        info->sampler_value = sampler->binding;
        info->sampler_type  = sampler->type;
        info->sampler_id    = sampler->id;
    } else {
        info->sampler_value =
        info->sampler_type  =
        info->sampler_id    = -1;
    }

    info->sampling_mode_id    = get_uniform_location(active_uniforms, tex_key, "_sampling_mode");
    info->coord_matrix_id     = get_uniform_location(active_uniforms, tex_key, "_coord_matrix");
    info->dimensions_id       = get_uniform_location(active_uniforms, tex_key, "_dimensions");
    info->ts_id               = get_uniform_location(active_uniforms, tex_key, "_ts");

#if defined(TARGET_ANDROID)
    info->external_sampler_id = get_uniform_location(active_uniforms, tex_key, "_external_sampler");
#elif defined(TARGET_IPHONE)
    info->y_sampler_id        = get_uniform_location(active_uniforms, tex_key, "_y_sampler");
    info->uv_sampler_id       = get_uniform_location(active_uniforms, tex_key, "_uv_sampler");
#endif
}

int ngli_pipeline_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct pipeline *s = get_pipeline(node);

    int ret = ngli_node_init(s->program);
    if (ret < 0)
        return ret;

    struct program *program = s->program->priv_data;

    int nb_uniforms = s->uniforms ? ngli_hmap_count(s->uniforms) : 0;
    if (nb_uniforms > 0) {
        s->uniform_pairs = calloc(nb_uniforms, sizeof(*s->uniform_pairs));
        if (!s->uniform_pairs)
            return -1;

        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->uniforms, entry))) {
            const struct uniformprograminfo *active_uniform =
                ngli_hmap_get(program->active_uniforms, entry->key);
            if (!active_uniform) {
                LOG(WARNING, "uniform %s attached to %s not found in %s",
                    entry->key, node->name, s->program->name);
                continue;
            }

            struct ngl_node *unode = entry->data;
            ret = ngli_node_init(unode);
            if (ret < 0)
                return ret;

            struct nodeprograminfopair pair = {
                .node = unode,
                .program_info = (void *)active_uniform,
            };
            snprintf(pair.name, sizeof(pair.name), "%s", entry->key);
            s->uniform_pairs[s->nb_uniform_pairs++] = pair;
        }
    }

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

        s->texture_pairs = calloc(nb_textures, sizeof(*s->texture_pairs));
        if (!s->texture_pairs)
            return -1;

        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->textures, entry))) {
            const char *key = entry->key;
            struct ngl_node *tnode = entry->data;
            int ret = ngli_node_init(tnode);
            if (ret < 0)
                return ret;

            struct texture *texture = tnode->priv_data;

            struct textureprograminfo *info = &s->textureprograminfos[s->nb_textureprograminfos];

            load_textureprograminfo(info, program->active_uniforms, key);

            if (info->sampler_type == GL_IMAGE_2D) {
                texture->direct_rendering = 0;
                if (info->sampler_value >= max_nb_textures) {
                    LOG(ERROR, "maximum number (%d) of texture unit reached", max_nb_textures);
                    return -1;
                }
                if (s->used_texture_units & (1 << info->sampler_value)) {
                    LOG(ERROR, "texture unit %d is already used by another image", info->sampler_value);
                    return -1;
                }
                s->used_texture_units |= 1 << info->sampler_value;
            }

#if defined(TARGET_ANDROID)
            const int has_aux_sampler = info->external_sampler_id >= 0;
#elif defined(TARGET_IPHONE)
            const int has_aux_sampler = (info->y_sampler_id >= 0 || info->uv_sampler_id >= 0);
#else
            const int has_aux_sampler = 0;
#endif

            if (info->sampler_id < 0 && !has_aux_sampler)
                LOG(WARNING, "no sampler found for texture %s", key);

#if defined(TARGET_ANDROID) || defined(TARGET_IPHONE)
            texture->direct_rendering = texture->direct_rendering && has_aux_sampler;
            LOG(INFO, "direct rendering for texture %s.%s: %s",
                node->name, key, texture->direct_rendering ? "yes" : "no");
#endif
            s->nb_textureprograminfos++;

            struct nodeprograminfopair pair = {
                .node = tnode,
                .program_info = (void *)info,
            };
            snprintf(pair.name, sizeof(pair.name), "%s", key);
            s->texture_pairs[s->nb_texture_pairs++] = pair;
        }
    }

    int nb_buffers = s->buffers ? ngli_hmap_count(s->buffers) : 0;
    if (nb_buffers > 0 &&
        gl->features & NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT) {
        s->buffer_pairs = calloc(nb_buffers, sizeof(*s->buffer_pairs));
        if (!s->buffer_pairs)
            return -1;

        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->buffers, entry))) {
            const struct bufferprograminfo *info =
                ngli_hmap_get(program->active_buffer_blocks, entry->key);
            if (!info) {
                LOG(WARNING, "buffer %s attached to %s not found in %s",
                    entry->key, node->name, s->program->name);
                continue;
            }

            struct ngl_node *bnode = entry->data;
            struct buffer *buffer = bnode->priv_data;
            buffer->generate_gl_buffer = 1;
            ret = ngli_node_init(bnode);
            if (ret < 0)
                return ret;

            struct nodeprograminfopair pair = {
                .node = bnode,
                .program_info = (void *)info,
            };
            snprintf(pair.name, sizeof(pair.name), "%s", entry->key);
            s->buffer_pairs[s->nb_buffer_pairs++] = pair;
        }
    }

    return 0;
}

void ngli_pipeline_uninit(struct ngl_node *node)
{
    struct pipeline *s = get_pipeline(node);

    free(s->textureprograminfos);
    free(s->texture_pairs);
    free(s->uniform_pairs);
    free(s->buffer_pairs);
}

int ngli_pipeline_update(struct ngl_node *node, double t)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct pipeline *s = get_pipeline(node);

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

    if (s->buffers &&
        gl->features & NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(s->buffers, entry))) {
            int ret = ngli_node_update(entry->data, t);
            if (ret < 0)
                return ret;
        }
    }

    return ngli_node_update(s->program, t);
}

int ngli_pipeline_upload_data(struct ngl_node *node)
{
    int ret;

    if ((ret = update_uniforms(node)) < 0 ||
        (ret = update_images_and_samplers(node)) < 0 ||
        (ret = update_buffers(node)) < 0)
        return ret;

    return 0;
}
