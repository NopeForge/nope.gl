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

#include "buffer.h"
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
        if (!(*used_texture_units & (1ULL << i))) {
            *used_texture_units |= (1ULL << i);
            return i;
        }
    }
    LOG(ERROR, "no texture unit available");
    return -1;
}

static const struct {
    const char *name;
    GLenum type;
} tex_specs[] = {
    [0] = {"2D",  GL_TEXTURE_2D},
    [1] = {"OES", GL_TEXTURE_EXTERNAL_OES},
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

    TRACE("using texture unit %d for disabled %s textures",
          tex_unit, tex_specs[type_index].name);
    s->disabled_texture_unit[type_index] = tex_unit;

    ngli_glActiveTexture(gl, GL_TEXTURE0 + tex_unit);
    ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
    if (gl->features & NGLI_FEATURE_OES_EGL_EXTERNAL_IMAGE)
        ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, 0);

    return tex_unit;
}

static int bind_texture_plane(const struct glcontext *gl,
                              struct texture *texture,
                              uint64_t *used_texture_units,
                              int index,
                              int location)
{
    GLuint id = texture->planes[index].id;
    GLenum target = texture->planes[index].target;
    int texture_index = acquire_next_available_texture_unit(used_texture_units);
    if (texture_index < 0)
        return -1;
    ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
    ngli_glBindTexture(gl, target, id);
    ngli_glUniform1i(gl, location, texture_index);
    return 0;
}

static int update_sampler(const struct glcontext *gl,
                          struct pipeline *s,
                          struct texture *texture,
                          const struct textureprograminfo *info,
                          uint64_t *used_texture_units,
                          int *sampling_mode)
{
    struct {
        int id;
        int type_index;
        int bound;
    } samplers[] = {
        { info->sampler_location,          0, 0 },
        { info->y_sampler_location,        0, 0 },
        { info->uv_sampler_location,       0, 0 },
        { info->external_sampler_location, 1, 0 },
    };

    *sampling_mode = NGLI_SAMPLING_MODE_NONE;
    if (texture->layout == NGLI_TEXTURE_LAYOUT_DEFAULT) {
        if (info->sampler_location >= 0) {
            if (info->sampler_type == GL_IMAGE_2D) {
                GLuint id = texture->planes[0].id;
                GLuint unit = info->sampler_value;
                ngli_glBindImageTexture(gl, unit, id, 0, GL_FALSE, 0, texture->access, texture->internal_format);
            } else {
                int ret = bind_texture_plane(gl, texture, used_texture_units, 0, info->sampler_location);
                if (ret < 0)
                    return ret;
                *sampling_mode = NGLI_SAMPLING_MODE_DEFAULT;
            }
            samplers[0].bound = 1;
        }
    } else if (texture->layout == NGLI_TEXTURE_LAYOUT_NV12) {
        if (info->y_sampler_location >= 0) {
            int ret = bind_texture_plane(gl, texture, used_texture_units, 0, info->y_sampler_location);
            if (ret < 0)
                return ret;
            samplers[1].bound = 1;
            *sampling_mode = NGLI_SAMPLING_MODE_NV12;
        }
        if (info->uv_sampler_location >= 0) {
            int ret = bind_texture_plane(gl, texture, used_texture_units, 1, info->uv_sampler_location);
            if (ret < 0)
                return ret;
            samplers[2].bound = 1;
            *sampling_mode = NGLI_SAMPLING_MODE_NV12;
        }
    } else if (texture->layout == NGLI_TEXTURE_LAYOUT_MEDIACODEC) {
        if (info->external_sampler_location >= 0) {
            int ret = bind_texture_plane(gl, texture, used_texture_units, 0, info->external_sampler_location);
            if (ret < 0)
                return ret;
            samplers[3].bound = 1;
            *sampling_mode = NGLI_SAMPLING_MODE_EXTERNAL_OES;
        }
    }

    for (int i = 0; i < NGLI_ARRAY_NB(samplers); i++) {
        if (samplers[i].id < 0)
            continue;
        if (samplers[i].bound)
            continue;
        int disabled_texture_unit = get_disabled_texture_unit(gl, s, used_texture_units, samplers[i].type_index);
        if (disabled_texture_unit < 0)
            return -1;
        ngli_glUniform1i(gl, samplers[i].id, disabled_texture_unit);
    }

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

            int sampling_mode;
            int ret = update_sampler(gl, s, texture, info, &used_texture_units, &sampling_mode);
            if (ret < 0)
                return ret;

            if (info->sampling_mode_location >= 0)
                ngli_glUniform1i(gl, info->sampling_mode_location, sampling_mode);

            if (info->coord_matrix_location >= 0)
                ngli_glUniformMatrix4fv(gl, info->coord_matrix_location, 1, GL_FALSE, texture->coordinates_matrix);

            if (info->dimensions_location >= 0) {
                const float dimensions[3] = {texture->width, texture->height, texture->depth};
                if (info->dimensions_type == GL_FLOAT_VEC2)
                    ngli_glUniform2fv(gl, info->dimensions_location, 1, dimensions);
                else if (info->dimensions_type == GL_FLOAT_VEC3)
                    ngli_glUniform3fv(gl, info->dimensions_location, 1, dimensions);
            }

            if (info->ts_location >= 0)
                ngli_glUniform1f(gl, info->ts_location, texture->data_src_ts);
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
        const GLint uid = info->location;
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

        ngli_glBindBufferBase(gl, info->type, info->binding, buffer->graphic_buffer.id);
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

#define OFFSET(x) offsetof(struct textureprograminfo, x)
static const struct texture_uniform_map {
    const char *suffix;
    const GLenum *allowed_types;
    size_t location_offset;
    size_t type_offset;
    size_t binding_offset;
} texture_uniform_maps[] = {
    {"",                  (const GLenum[]){GL_SAMPLER_2D, GL_SAMPLER_3D, GL_IMAGE_2D, 0}, OFFSET(sampler_location),          OFFSET(sampler_type),    OFFSET(sampler_value)},
    {"_sampling_mode",    (const GLenum[]){GL_INT, 0},                                    OFFSET(sampling_mode_location),    SIZE_MAX,                SIZE_MAX},
    {"_coord_matrix",     (const GLenum[]){GL_FLOAT_MAT4, 0},                             OFFSET(coord_matrix_location),     SIZE_MAX,                SIZE_MAX},
    {"_dimensions",       (const GLenum[]){GL_FLOAT_VEC2, GL_FLOAT_VEC3, 0},              OFFSET(dimensions_location),       OFFSET(dimensions_type), SIZE_MAX},
    {"_ts",               (const GLenum[]){GL_FLOAT, 0},                                  OFFSET(ts_location),               SIZE_MAX,                SIZE_MAX},
    {"_external_sampler", (const GLenum[]){GL_SAMPLER_EXTERNAL_OES, 0},                   OFFSET(external_sampler_location), SIZE_MAX,                SIZE_MAX},
    {"_y_sampler",        (const GLenum[]){GL_SAMPLER_2D, 0},                             OFFSET(y_sampler_location),        SIZE_MAX,                SIZE_MAX},
    {"_uv_sampler",       (const GLenum[]){GL_SAMPLER_2D, 0},                             OFFSET(uv_sampler_location),       SIZE_MAX,                SIZE_MAX},
};

static int is_allowed_type(const GLenum *allowed_types, GLenum type)
{
    for (int i = 0; allowed_types[i]; i++)
        if (allowed_types[i] == type)
            return 1;
    return 0;
}

static int load_textureprograminfo(struct textureprograminfo *info,
                                   struct hmap *active_uniforms,
                                   const char *tex_key)
{
    for (int i = 0; i < NGLI_ARRAY_NB(texture_uniform_maps); i++) {
        const struct texture_uniform_map *map = &texture_uniform_maps[i];
        const char *suffix = map->suffix;
        const struct uniformprograminfo *uniform = get_uniform_info(active_uniforms, tex_key, suffix);
        if (!uniform && !strcmp(map->suffix, "")) { // Allow _sampler suffix
            suffix = "_sampler";
            uniform = get_uniform_info(active_uniforms, tex_key, suffix);
        }
        if (uniform && !is_allowed_type(map->allowed_types, uniform->type)) {
            LOG(ERROR, "invalid type 0x%x found for texture uniform %s%s",
                uniform->type, tex_key, suffix);
            return -1;
        }

#define SET_INFO_FIELD(field) do {                                                       \
    if (map->field##_offset != SIZE_MAX)                                                 \
        *(int *)((uint8_t *)info + map->field##_offset) = uniform ? uniform->field : -1; \
} while (0)

        SET_INFO_FIELD(location);
        SET_INFO_FIELD(type);
        SET_INFO_FIELD(binding);
    }
    return 0;
}

int ngli_pipeline_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct pipeline *s = get_pipeline(node);
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

            struct nodeprograminfopair pair = {
                .node = entry->data,
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
            struct texture *texture = tnode->priv_data;

            struct textureprograminfo *info = &s->textureprograminfos[s->nb_textureprograminfos];

            int ret = load_textureprograminfo(info, program->active_uniforms, key);
            if (ret < 0)
                return ret;

            if (info->sampler_type == GL_IMAGE_2D) {
                texture->direct_rendering = 0;
                if (info->sampler_value >= max_nb_textures) {
                    LOG(ERROR, "maximum number (%d) of texture unit reached", max_nb_textures);
                    return -1;
                }
                if (s->used_texture_units & (1ULL << info->sampler_value)) {
                    LOG(ERROR, "texture unit %d is already used by another image", info->sampler_value);
                    return -1;
                }
                s->used_texture_units |= 1ULL << info->sampler_value;
            }

#if defined(TARGET_ANDROID)
            const int has_aux_sampler = info->external_sampler_location >= 0;
#elif defined(TARGET_IPHONE)
            const int has_aux_sampler = (info->y_sampler_location >= 0 || info->uv_sampler_location >= 0);
#else
            const int has_aux_sampler = 0;
#endif

            if (info->sampler_location < 0 && !has_aux_sampler)
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

            if (info->type == GL_UNIFORM_BUFFER &&
                buffer->data_size > gl->max_uniform_block_size) {
                LOG(ERROR, "buffer %s size (%d) exceeds max uniform block size (%d)",
                    bnode->name, buffer->data_size, gl->max_uniform_block_size);
                return -1;
            }

            int ret = ngli_buffer_ref(bnode);
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
    for (int i = 0; i < s->nb_buffer_pairs; i++) {
        struct nodeprograminfopair *pair = &s->buffer_pairs[i];
        ngli_buffer_unref((struct ngl_node *)pair->node);
    }
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
            struct ngl_node *bnode = (struct ngl_node *)entry->data;
            int ret = ngli_node_update(bnode, t);
            if (ret < 0)
                return ret;
            ret = ngli_buffer_upload(bnode);
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
