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
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "texture.h"
#include "utils.h"

static void set_uniform_1f(struct glcontext *gl, GLint loc, void *priv)
{
    const struct uniform_priv *u = priv;
    ngli_glUniform1f(gl, loc, u->scalar);
}

static void set_uniform_2fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct uniform_priv *u = priv;
    ngli_glUniform2fv(gl, loc, 1, u->vector);
}

static void set_uniform_3fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct uniform_priv *u = priv;
    ngli_glUniform3fv(gl, loc, 1, u->vector);
}

static void set_uniform_4fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct uniform_priv *u = priv;
    ngli_glUniform4fv(gl, loc, 1, u->vector);
}

static void set_uniform_1i(struct glcontext *gl, GLint loc, void *priv)
{
    const struct uniform_priv *u = priv;
    ngli_glUniform1i(gl, loc, u->ival);
}

static void set_uniform_mat4fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct uniform_priv *u = priv;
    ngli_glUniformMatrix4fv(gl, loc, 1, GL_FALSE, u->matrix);
}

static void set_uniform_buf1fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct buffer_priv *buffer = priv;
    ngli_glUniform1fv(gl, loc, buffer->count, (const GLfloat *)buffer->data);
}

static void set_uniform_buf2fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct buffer_priv *buffer = priv;
    ngli_glUniform2fv(gl, loc, buffer->count, (const GLfloat *)buffer->data);
}

static void set_uniform_buf3fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct buffer_priv *buffer = priv;
    ngli_glUniform3fv(gl, loc, buffer->count, (const GLfloat *)buffer->data);
}

static void set_uniform_buf4fv(struct glcontext *gl, GLint loc, void *priv)
{
    const struct buffer_priv *buffer = priv;
    ngli_glUniform4fv(gl, loc, buffer->count, (const GLfloat *)buffer->data);
}

static int set_uniforms(struct pipeline *s)
{
    struct glcontext *gl = s->gl;
    const struct darray *uniform_pairs = &s->uniform_pairs;
    const struct nodeprograminfopair *pairs = ngli_darray_data(uniform_pairs);
    for (int i = 0; i < ngli_darray_count(uniform_pairs); i++) {
        const struct nodeprograminfopair *pair = &pairs[i];
        const struct ngl_node *unode = pair->node;
        const struct uniformprograminfo *info = pair->program_info;

        pair->handle(gl, info->location, unode->priv_data);
    }

    return 0;
}

struct handle_map {
    GLenum uniform_type;
    nodeprograminfopair_handle_func handle;
};

static const struct {
    int class_id;
    const struct handle_map *handles_map;
} uniforms_specs[] = {
    {
        .class_id = NGL_NODE_UNIFORMFLOAT,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT, set_uniform_1f},
            {0},
        },
    }, {
        .class_id = NGL_NODE_UNIFORMVEC2,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_VEC2, set_uniform_2fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_UNIFORMVEC3,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_VEC3, set_uniform_3fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_UNIFORMVEC4,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_VEC4, set_uniform_4fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_UNIFORMMAT4,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_MAT4, set_uniform_mat4fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_UNIFORMQUAT,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_MAT4, set_uniform_mat4fv},
            {GL_FLOAT_VEC4, set_uniform_4fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_UNIFORMINT,
        .handles_map = (const struct handle_map[]){
            {GL_INT, set_uniform_1i},
            {GL_BOOL, set_uniform_1i},
            {0},
        },
    }, {
        .class_id = NGL_NODE_BUFFERFLOAT,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT, set_uniform_buf1fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_BUFFERVEC2,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_VEC2, set_uniform_buf2fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_BUFFERVEC3,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_VEC3, set_uniform_buf3fv},
            {0},
        },
    }, {
        .class_id = NGL_NODE_BUFFERVEC4,
        .handles_map = (const struct handle_map[]){
            {GL_FLOAT_VEC4, set_uniform_buf4fv},
            {0},
        },
    },
};

static const struct handle_map *get_uniform_handle_map(int class_id)
{
    for (int i = 0; i < NGLI_ARRAY_NB(uniforms_specs); i++)
        if (uniforms_specs[i].class_id == class_id)
            return uniforms_specs[i].handles_map;
    return NULL;
}

static nodeprograminfopair_handle_func get_uniform_pair_handle(int class_id, GLenum uniform_type)
{
    const struct handle_map *handle_map = get_uniform_handle_map(class_id);
    for (int i = 0; handle_map[i].uniform_type; i++)
        if (handle_map[i].uniform_type == uniform_type)
            return handle_map[i].handle;
    return NULL;
}

static int build_uniform_pairs(struct pipeline *s)
{
    struct pipeline_params *params = &s->params;
    struct program_priv *program = params->program->priv_data;

    if (!params->uniforms)
        return 0;

    ngli_darray_init(&s->uniform_pairs, sizeof(struct nodeprograminfopair), 0);

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(params->uniforms, entry))) {
        const struct uniformprograminfo *active_uniform =
            ngli_hmap_get(program->active_uniforms, entry->key);
        if (!active_uniform) {
            LOG(WARNING, "uniform %s attached to %s not found in %s",
                entry->key, params->label, params->program->label);
            continue;
        }

        if (active_uniform->location < 0)
            continue;

        struct ngl_node *unode = entry->data;
        struct nodeprograminfopair pair = {
            .node = unode,
            .program_info = (void *)active_uniform,
        };

        pair.handle = get_uniform_pair_handle(unode->class->id, active_uniform->type);
        if (!pair.handle) {
            LOG(ERROR, "%s set on %s.%s has not the expected type in the shader",
                unode->label, params->label, entry->key);
            return -1;
        }

        snprintf(pair.name, sizeof(pair.name), "%s", entry->key);
        if (!ngli_darray_push(&s->uniform_pairs, &pair))
            return -1;
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
    {"",                  (const GLenum[]){GL_SAMPLER_2D, GL_SAMPLER_3D,
                                           GL_SAMPLER_CUBE, GL_IMAGE_2D, 0},              OFFSET(sampler_location),          OFFSET(sampler_type),    OFFSET(sampler_value)},
    {"_sampling_mode",    (const GLenum[]){GL_INT, 0},                                    OFFSET(sampling_mode_location),    SIZE_MAX,                SIZE_MAX},
    {"_coord_matrix",     (const GLenum[]){GL_FLOAT_MAT4, 0},                             OFFSET(coord_matrix_location),     SIZE_MAX,                SIZE_MAX},
    {"_dimensions",       (const GLenum[]){GL_FLOAT_VEC2, GL_FLOAT_VEC3, 0},              OFFSET(dimensions_location),       OFFSET(dimensions_type), SIZE_MAX},
    {"_ts",               (const GLenum[]){GL_FLOAT, 0},                                  OFFSET(ts_location),               SIZE_MAX,                SIZE_MAX},
    {"_external_sampler", (const GLenum[]){GL_SAMPLER_EXTERNAL_OES,
                                           GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT, 0},            OFFSET(external_sampler_location), SIZE_MAX,                SIZE_MAX},
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

static int build_texture_pairs(struct pipeline *s)
{
    struct glcontext *gl = s->gl;
    struct pipeline_params *params = &s->params;
    struct program_priv *program = params->program->priv_data;

    int nb_textures = params->textures ? ngli_hmap_count(params->textures) : 0;
    int max_nb_textures = NGLI_MIN(gl->max_texture_image_units, sizeof(s->used_texture_units) * 8);
    if (nb_textures > max_nb_textures) {
        LOG(ERROR, "attached textures count (%d) exceeds driver limit (%d)",
            nb_textures, gl->max_texture_image_units);
        return -1;
    }

    if (!nb_textures)
        return 0;

    ngli_darray_init(&s->texture_pairs, sizeof(struct nodeprograminfopair), 0);

    s->textureprograminfos = ngli_calloc(nb_textures, sizeof(*s->textureprograminfos));
    if (!s->textureprograminfos)
        return -1;

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(params->textures, entry))) {
        const char *key = entry->key;
        struct ngl_node *tnode = entry->data;
        struct texture_priv *texture = tnode->priv_data;

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
#elif defined(TARGET_IPHONE) || defined(HAVE_VAAPI_X11)
        const int has_aux_sampler = (info->y_sampler_location >= 0 || info->uv_sampler_location >= 0);
#else
        const int has_aux_sampler = 0;
#endif

        if (info->sampler_location < 0 && !has_aux_sampler)
            LOG(WARNING, "no sampler found for texture %s", key);

#if defined(TARGET_ANDROID) || defined(TARGET_IPHONE) || defined(HAVE_VAAPI_X11)
        struct pipeline_params *params = &s->params;
        texture->direct_rendering = texture->direct_rendering && has_aux_sampler;
        LOG(DEBUG, "direct rendering for texture %s.%s: %s",
            params->label, key, texture->direct_rendering ? "yes" : "no");
#endif
        s->nb_textureprograminfos++;

        struct nodeprograminfopair pair = {
            .node = tnode,
            .program_info = (void *)info,
        };
        snprintf(pair.name, sizeof(pair.name), "%s", key);
        if (!ngli_darray_push(&s->texture_pairs, &pair))
            return -1;
    }
    return 0;
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
                              const struct texture *plane,
                              uint64_t *used_texture_units,
                              int location)
{
    int texture_index = acquire_next_available_texture_unit(used_texture_units);
    if (texture_index < 0)
        return -1;
    ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
    if (gl->features & NGLI_FEATURE_OES_EGL_EXTERNAL_IMAGE) {
        GLenum target = plane->target == GL_TEXTURE_2D ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
        ngli_glBindTexture(gl, target, 0);
    }
    ngli_glBindTexture(gl, plane->target, plane->id);
    ngli_glUniform1i(gl, location, texture_index);
    return 0;
}

static int update_sampler(const struct glcontext *gl,
                          struct pipeline *s,
                          const struct image *image,
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
    if (image->layout == NGLI_IMAGE_LAYOUT_DEFAULT) {
        if (info->sampler_location >= 0) {
            const struct texture *plane = image->planes[0];
            if (info->sampler_type == GL_IMAGE_2D) {
                const struct texture_params *params = &plane->params;
                GLuint unit = info->sampler_value;
                ngli_glBindImageTexture(gl, unit, plane->id, 0, GL_FALSE, 0, params->access, plane->internal_format);
            } else {
                int ret = bind_texture_plane(gl, plane, used_texture_units, info->sampler_location);
                if (ret < 0)
                    return ret;
                *sampling_mode = NGLI_SAMPLING_MODE_DEFAULT;
            }
            samplers[0].bound = 1;
        }
    } else if (image->layout == NGLI_IMAGE_LAYOUT_NV12) {
        if (info->y_sampler_location >= 0) {
            const struct texture *plane = image->planes[0];
            int ret = bind_texture_plane(gl, plane, used_texture_units, info->y_sampler_location);
            if (ret < 0)
                return ret;
            samplers[1].bound = 1;
            *sampling_mode = NGLI_SAMPLING_MODE_NV12;
        }
        if (info->uv_sampler_location >= 0) {
            const struct texture *plane = image->planes[1];
            int ret = bind_texture_plane(gl, plane, used_texture_units, info->uv_sampler_location);
            if (ret < 0)
                return ret;
            samplers[2].bound = 1;
            *sampling_mode = NGLI_SAMPLING_MODE_NV12;
        }
    } else if (image->layout == NGLI_IMAGE_LAYOUT_MEDIACODEC) {
        if (info->external_sampler_location >= 0) {
            const struct texture *plane = image->planes[0];
            int ret = bind_texture_plane(gl, plane, used_texture_units, info->external_sampler_location);
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

static int set_textures(struct pipeline *s)
{
    struct glcontext *gl = s->gl;
    const struct darray *texture_pairs = &s->texture_pairs;

    if (!ngli_darray_count(texture_pairs))
        return 0;

    uint64_t used_texture_units = s->used_texture_units;

    for (int i = 0; i < NGLI_ARRAY_NB(s->disabled_texture_unit); i++)
        s->disabled_texture_unit[i] = -1;

    const struct nodeprograminfopair *pairs = ngli_darray_data(texture_pairs);
    for (int i = 0; i < ngli_darray_count(texture_pairs); i++) {
        const struct nodeprograminfopair *pair = &pairs[i];
        const struct textureprograminfo *info = pair->program_info;
        const struct texture_priv *texture = pair->node->priv_data;
        const struct image *image = &texture->image;

        int sampling_mode;
        int ret = update_sampler(gl, s, image, info, &used_texture_units, &sampling_mode);
        if (ret < 0)
            return ret;

        if (info->sampling_mode_location >= 0)
            ngli_glUniform1i(gl, info->sampling_mode_location, sampling_mode);

        if (info->coord_matrix_location >= 0)
            ngli_glUniformMatrix4fv(gl, info->coord_matrix_location, 1, GL_FALSE, image->coordinates_matrix);

        if (info->dimensions_location >= 0) {
            float dimensions[3] = {0};
            if (image->layout != NGLI_IMAGE_LAYOUT_NONE) {
                const struct texture_params *params = &image->planes[0]->params;
                dimensions[0] = params->width;
                dimensions[1] = params->height;
                dimensions[2] = params->depth;
            }
            if (info->dimensions_type == GL_FLOAT_VEC2)
                ngli_glUniform2fv(gl, info->dimensions_location, 1, dimensions);
            else if (info->dimensions_type == GL_FLOAT_VEC3)
                ngli_glUniform3fv(gl, info->dimensions_location, 1, dimensions);
        }

        if (info->ts_location >= 0)
            ngli_glUniform1f(gl, info->ts_location, image->ts);
    }

    return 0;
}

static int build_block_pairs(struct pipeline *s)
{
    struct glcontext *gl = s->gl;
    struct pipeline_params *params = &s->params;
    struct program_priv *program = params->program->priv_data;

    if (!params->blocks)
        return 0;

    ngli_darray_init(&s->block_pairs, sizeof(struct nodeprograminfopair), 0);

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(params->blocks, entry))) {
        const struct blockprograminfo *info =
            ngli_hmap_get(program->active_buffer_blocks, entry->key);
        if (!info) {
            LOG(WARNING, "block %s attached to %s not found in %s",
                entry->key, params->label, params->program->label);
            continue;
        }

        struct ngl_node *bnode = entry->data;
        struct block_priv *block = bnode->priv_data;

        if (info->type == GL_UNIFORM_BUFFER &&
            block->data_size > gl->max_uniform_block_size) {
            LOG(ERROR, "block %s size (%d) exceeds max uniform block size (%d)",
                bnode->label, block->data_size, gl->max_uniform_block_size);
            return -1;
        }

        int ret = ngli_node_block_ref(bnode);
        if (ret < 0)
            return ret;

        struct nodeprograminfopair pair = {
            .node = bnode,
            .program_info = (void *)info,
        };
        snprintf(pair.name, sizeof(pair.name), "%s", entry->key);
        if (!ngli_darray_push(&s->block_pairs, &pair)) {
            ngli_node_block_unref(bnode);
            return -1;
        }
    }
    return 0;
}

static int set_blocks(struct pipeline *s)
{
    struct glcontext *gl = s->gl;
    const struct darray *block_pairs = &s->block_pairs;
    const struct nodeprograminfopair *pairs = ngli_darray_data(block_pairs);
    for (int i = 0; i < ngli_darray_count(block_pairs); i++) {
        const struct nodeprograminfopair *pair = &pairs[i];
        const struct ngl_node *bnode = pair->node;
        const struct block_priv *block = bnode->priv_data;
        const struct blockprograminfo *info = pair->program_info;

        ngli_glBindBufferBase(gl, info->type, info->binding, block->buffer.id);
    }

    return 0;
}

int ngli_pipeline_init(struct pipeline *s, struct ngl_ctx *ctx, const struct pipeline_params *params)
{
    int ret;

    s->ctx = ctx;
    s->gl = ctx->glcontext;
    s->params = *params;

    if ((ret = build_uniform_pairs(s)) < 0 ||
        (ret = build_texture_pairs(s)) < 0 ||
        (ret = build_block_pairs(s)) < 0)
        return ret;

    return 0;
}

void ngli_pipeline_uninit(struct pipeline *s)
{
    if (!s->gl)
        return;

    ngli_free(s->textureprograminfos);

    ngli_darray_reset(&s->texture_pairs);
    ngli_darray_reset(&s->uniform_pairs);

    struct darray *block_pairs = &s->block_pairs;
    struct nodeprograminfopair *pairs = ngli_darray_data(block_pairs);
    for (int i = 0; i < ngli_darray_count(block_pairs); i++) {
        struct nodeprograminfopair *pair = &pairs[i];
        struct ngl_node *bnode = pair->node;
        ngli_node_block_unref(bnode);
    }
    ngli_darray_reset(&s->block_pairs);

    memset(s, 0, sizeof(*s));
}

int ngli_pipeline_update(struct pipeline *s, double t)
{
    struct darray *texture_pairs = &s->texture_pairs;
    struct nodeprograminfopair *tpairs = ngli_darray_data(texture_pairs);
    for (int i = 0; i < ngli_darray_count(texture_pairs); i++) {
        struct nodeprograminfopair *pair = &tpairs[i];
        struct ngl_node *tnode = pair->node;
        int ret = ngli_node_update(tnode, t);
        if (ret < 0)
            return ret;
    }

    struct darray *uniform_pairs = &s->uniform_pairs;
    struct nodeprograminfopair *upairs = ngli_darray_data(uniform_pairs);
    for (int i = 0; i < ngli_darray_count(uniform_pairs); i++) {
        struct nodeprograminfopair *pair = &upairs[i];
        struct ngl_node *unode = pair->node;
        int ret = ngli_node_update(unode, t);
        if (ret < 0)
            return ret;
    }

    struct darray *block_pairs = &s->block_pairs;
    struct nodeprograminfopair *bpairs = ngli_darray_data(block_pairs);
    for (int i = 0; i < ngli_darray_count(block_pairs); i++) {
        struct nodeprograminfopair *pair = &bpairs[i];
        struct ngl_node *bnode = pair->node;
        int ret = ngli_node_update(bnode, t);
        if (ret < 0)
            return ret;
        ret = ngli_node_block_upload(bnode);
        if (ret < 0)
            return ret;
    }

    struct pipeline_params *params = &s->params;
    return ngli_node_update(params->program, t);
}

int ngli_pipeline_upload_data(struct pipeline *s)
{
    int ret;

    if ((ret = set_uniforms(s)) < 0 ||
        (ret = set_textures(s)) < 0 ||
        (ret = set_blocks(s)) < 0)
        return ret;

    return 0;
}
