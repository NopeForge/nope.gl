/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2016-2022 GoPro Inc.
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
#include <stdint.h>
#include <inttypes.h>

#include "blending.h"
#include "block.h"
#include "buffer.h"
#include "darray.h"
#include "geometry.h"
#include "gpu_ctx.h"
#include "hmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nopegl.h"
#include "internal.h"
#include "pass.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "program.h"
#include "texture.h"
#include "type.h"
#include "utils.h"

struct uniform_map {
    int32_t index;
    const void *data;
};

struct resource_map {
    int32_t index;
    const struct block_info *info;
    size_t buffer_rev;
};

struct texture_map {
    const struct image *image;
    size_t image_rev;
};

struct pipeline_desc {
    struct pgcraft *crafter;
    struct pipeline_compat *pipeline_compat;
    int32_t modelview_matrix_index;
    int32_t projection_matrix_index;
    int32_t normal_matrix_index;
    int32_t resolution_index;
    struct darray uniforms_map;
    struct darray blocks_map;
    struct darray textures_map;
};

static int register_uniform(struct pass *s, const char *name, struct ngl_node *uniform, int stage)
{
    struct pgcraft_uniform crafter_uniform = {.stage = stage};
    snprintf(crafter_uniform.name, sizeof(crafter_uniform.name), "%s", name);

    if (uniform->cls->category == NGLI_NODE_CATEGORY_BUFFER) {
        struct buffer_info *buffer_info = uniform->priv_data;
        crafter_uniform.type  = buffer_info->layout.type;
        crafter_uniform.count = buffer_info->layout.count;
        crafter_uniform.data  = buffer_info->data;
    } else if (uniform->cls->category == NGLI_NODE_CATEGORY_VARIABLE) {
        struct variable_info *variable_info = uniform->priv_data;
        crafter_uniform.type  = variable_info->data_type;
        crafter_uniform.data  = variable_info->data;
    } else {
        ngli_assert(0);
    }

    const struct pass_params *params = &s->params;
    if (params->properties) {
        struct ngl_node *resprops_node = ngli_hmap_get_str(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_opts *resprops = resprops_node->opts;
            crafter_uniform.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_uniforms, &crafter_uniform))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_builtin_uniforms(struct pass *s)
{
    struct pgcraft_uniform crafter_uniforms[] = {
        {.name = "ngl_modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage=NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "ngl_projection_matrix", .type = NGLI_TYPE_MAT4, .stage=NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "ngl_normal_matrix",     .type = NGLI_TYPE_MAT3, .stage=NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "ngl_resolution",        .type = NGLI_TYPE_VEC2, .stage=NGLI_PROGRAM_SHADER_FRAG, .data = NULL},
    };

    for (size_t i = 0; i < NGLI_ARRAY_NB(crafter_uniforms); i++) {
        struct pgcraft_uniform *crafter_uniform = &crafter_uniforms[i];
        if (!ngli_darray_push(&s->crafter_uniforms, crafter_uniform))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int register_texture(struct pass *s, const char *name, struct ngl_node *texture, int stage)
{
    struct texture_priv *texture_priv = texture->priv_data;
    struct texture_opts *texture_opts = texture->opts;

    /*
     * Note: we do not assign crafter_texture.texture here since it is not
     * initialized yet: non-media texture are initialized during prefetch and
     * media-texture are initialized during update.
     */
    struct pgcraft_texture crafter_texture = {
        .stage  = stage,
        .image  = &texture_priv->image,
        .format = texture_priv->params.format,
        .clamp_video = texture_opts->clamp_video,
    };
    snprintf(crafter_texture.name, sizeof(crafter_texture.name), "%s", name);

    switch (texture->cls->id) {
    case NGL_NODE_TEXTURE2D:
        if (texture_opts->data_src && texture_opts->data_src->cls->id == NGL_NODE_MEDIA)
            crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO;
        else
            crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D;
        break;
    case NGL_NODE_TEXTURE2DARRAY:
        crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D_ARRAY;
        break;
    case NGL_NODE_TEXTURE3D:
        crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_3D;
        break;
    case NGL_NODE_TEXTURECUBE:
        crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE;
        break;
    default:
        ngli_assert(0);
    }

    const struct pass_params *params = &s->params;
    if (params->properties) {
        const struct ngl_node *resprops_node = ngli_hmap_get_str(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_opts *resprops = resprops_node->opts;
            if (resprops->as_image) {
                /* Disable direct rendering when using image load/store */
                texture_priv->supported_image_layouts = 1 << NGLI_IMAGE_LAYOUT_DEFAULT;
                texture_priv->params.usage |= NGLI_TEXTURE_USAGE_STORAGE_BIT;

                if (texture->cls->id == NGL_NODE_TEXTURE2D)
                    crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D;
                else if (texture->cls->id == NGL_NODE_TEXTURE2DARRAY)
                    crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D_ARRAY;
                else if (texture->cls->id == NGL_NODE_TEXTURE3D)
                    crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_3D;
                else if (texture->cls->id == NGL_NODE_TEXTURECUBE)
                    crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_CUBE;
                else
                    ngli_assert(0);
            }
            crafter_texture.writable  = resprops->writable;
            crafter_texture.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_textures, &crafter_texture))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_block(struct pass *s, const char *name, struct ngl_node *block_node, int stage)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct gpu_limits *limits = &gpu_ctx->limits;

    struct block_info *block_info = block_node->priv_data;
    struct block *block = &block_info->block;
    const size_t block_size = ngli_block_get_size(block, 0);

    /*
     * Select buffer type. We prefer UBO over SSBO, but in the following
     * situations, UBO is not possible.
     */
    int type = NGLI_TYPE_UNIFORM_BUFFER;
    if (block->layout == NGLI_BLOCK_LAYOUT_STD430) {
        LOG(DEBUG, "block %s has a std430 layout, declaring it as SSBO", name);
        type = NGLI_TYPE_STORAGE_BUFFER;
    } else if (block_size > limits->max_uniform_block_size) {
        LOG(DEBUG, "block %s is larger than the max UBO size (%zu > %d), declaring it as SSBO",
            name, block_size, limits->max_uniform_block_size);
        if (block_size > limits->max_storage_block_size) {
            LOG(ERROR, "block %s is larger than the max SSBO size (%zd > %d)",
                name, block_size, limits->max_storage_block_size);
            return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
        }
        type = NGLI_TYPE_STORAGE_BUFFER;
    }

    int writable = 0;
    const struct pass_params *params = &s->params;
    if (params->properties) {
        const struct ngl_node *resprops_node = ngli_hmap_get_str(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_opts *resprops = resprops_node->opts;
            if (resprops->writable)
                type = NGLI_TYPE_STORAGE_BUFFER;
            writable = resprops->writable;
        }
    }

    if (type == NGLI_TYPE_UNIFORM_BUFFER)
        ngli_node_block_extend_usage(block_node, NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    else if (type == NGLI_TYPE_STORAGE_BUFFER)
        ngli_node_block_extend_usage(block_node, NGLI_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    else
        ngli_assert(0);

    const struct buffer *buffer = block_info->buffer;
    const size_t buffer_size = buffer ? buffer->size : 0;
    struct pgcraft_block crafter_block = {
        .type     = type,
        .stage    = stage,
        .writable = writable,
        .block    = block,
        .buffer   = {
            .buffer = buffer,
            .size   = buffer_size,
        },
    };
    snprintf(crafter_block.name, sizeof(crafter_block.name), "%s", name);

    if (!ngli_darray_push(&s->crafter_blocks, &crafter_block))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_attribute_from_buffer(struct pass *s, const char *name,
                                          struct buffer *buffer, const struct buffer_layout *layout)
{
    if (!buffer)
        return 0;

    struct pgcraft_attribute crafter_attribute = {
        .type   = layout->type,
        .format = layout->format,
        .stride = layout->stride,
        .offset = layout->offset,
        .buffer = buffer,
    };
    snprintf(crafter_attribute.name, sizeof(crafter_attribute.name), "%s", name);

    const struct pass_params *params = &s->params;
    if (params->properties) {
        const struct ngl_node *resprops_node = ngli_hmap_get_str(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_opts *resprops = resprops_node->opts;
            crafter_attribute.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_attributes, &crafter_attribute))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_attribute(struct pass *s, const char *name, struct ngl_node *attribute, int rate)
{
    if (!attribute)
        return 0;

    ngli_node_buffer_extend_usage(attribute, NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    struct buffer_info *attribute_priv = attribute->priv_data;
    struct pgcraft_attribute crafter_attribute = {
        .type   = attribute_priv->layout.type,
        .format = attribute_priv->layout.format,
        .stride = attribute_priv->layout.stride,
        .offset = attribute_priv->layout.offset,
        .rate   = rate,
        .buffer = attribute_priv->buffer,
    };
    snprintf(crafter_attribute.name, sizeof(crafter_attribute.name), "%s", name);

    attribute_priv->flags |= NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD;

    const struct pass_params *params = &s->params;
    if (params->properties) {
        const struct ngl_node *resprops_node = ngli_hmap_get_str(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_opts *resprops = resprops_node->opts;
            crafter_attribute.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_attributes, &crafter_attribute))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_resource(struct pass *s, const char *name, struct ngl_node *node, int stage)
{
    switch (node->cls->category) {
    case NGLI_NODE_CATEGORY_VARIABLE:
    case NGLI_NODE_CATEGORY_BUFFER:  return register_uniform(s, name, node, stage);
    case NGLI_NODE_CATEGORY_TEXTURE: return register_texture(s, name, node, stage);
    case NGLI_NODE_CATEGORY_BLOCK:   return register_block(s, name, node, stage);
    default:
        ngli_assert(0);
    }
}

static int register_resources(struct pass *s, const struct hmap *resources, int stage)
{
    if (!resources)
        return 0;

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(resources, entry))) {
        int ret = register_resource(s, entry->key.str, entry->data, stage);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int pass_graphics_init(struct pass *s)
{
    const struct pass_params *params = &s->params;
    const struct geometry *geometry = params->geometry;

    s->pipeline_type = NGLI_PIPELINE_TYPE_GRAPHICS;
    s->topology = geometry->topology;

    if (geometry->indices_buffer) {
        s->indices = geometry->indices_buffer;
        s->indices_layout = &geometry->indices_layout;
    } else {
        s->nb_vertices = (int)geometry->vertices_layout.count;
    }
    s->nb_instances = s->params.nb_instances;

    int ret;

    if ((ret = register_resources(s, params->vert_resources, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = register_resources(s, params->frag_resources, NGLI_PROGRAM_SHADER_FRAG)) < 0)
        return ret;

    if ((ret = register_attribute_from_buffer(s, "ngl_position", geometry->vertices_buffer, &geometry->vertices_layout)) < 0 ||
        (ret = register_attribute_from_buffer(s, "ngl_uvcoord",  geometry->uvcoords_buffer, &geometry->uvcoords_layout)) < 0 ||
        (ret = register_attribute_from_buffer(s, "ngl_normal",   geometry->normals_buffer,  &geometry->normals_layout)) < 0)
        return ret;

    if (params->attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->attributes, entry))) {
            int ret = register_attribute(s, entry->key.str, entry->data, 0);
            if (ret < 0)
                return ret;
        }
    }

    if (params->instance_attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->instance_attributes, entry))) {
            int ret = register_attribute(s, entry->key.str, entry->data, 1);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int pass_compute_init(struct pass *s)
{
    const struct pass_params *params = &s->params;

    int ret = register_resources(s, params->compute_resources, NGLI_PROGRAM_SHADER_COMP);
    if (ret < 0)
        return ret;

    s->pipeline_type = NGLI_PIPELINE_TYPE_COMPUTE;

    return 0;
}

static int build_uniforms_map(struct pipeline_desc *desc, struct darray *crafter_uniforms)
{
    ngli_darray_init(&desc->uniforms_map, sizeof(struct uniform_map), 0);

    struct pgcraft_uniform *uniforms = ngli_darray_data(crafter_uniforms);
    for (size_t i = 0; i < ngli_darray_count(crafter_uniforms); i++) {
        const struct pgcraft_uniform *uniform = &uniforms[i];
        const int32_t index = ngli_pgcraft_get_uniform_index(desc->crafter, uniform->name, uniform->stage);

        /* The following can happen if the driver makes optimisation and
         * removes unused uniforms */
        if (index < 0)
            continue;

        /* This skips unwanted uniforms such as modelview and projection which
         * are handled separately */
        if (!uniform->data)
            continue;

        const struct uniform_map map = {.index=index, .data=uniform->data};
        if (!ngli_darray_push(&desc->uniforms_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int build_blocks_map(struct pass *s, struct pipeline_desc *desc)
{
    ngli_darray_init(&desc->blocks_map, sizeof(struct resource_map), 0);

    struct pipeline_compat_layout layout = ngli_pgcraft_get_pipeline_layout(desc->crafter);

    for (size_t i = 0; i < layout.nb_buffers; i++) {
        const struct bindgroup_layout_entry *entry = &layout.buffers[i];
        const struct hmap *resources = NULL;
        if (entry->stage == NGLI_PROGRAM_SHADER_VERT)
            resources = s->params.vert_resources;
        else if (entry->stage == NGLI_PROGRAM_SHADER_FRAG)
            resources = s->params.frag_resources;
        else if (entry->stage == NGLI_PROGRAM_SHADER_COMP)
            resources = s->params.compute_resources;
        else
            ngli_assert(0);

        if (!resources)
            continue;

        const char *name = ngli_pgcraft_get_symbol_name(desc->crafter, entry->id);
        const int32_t index = ngli_pgcraft_get_block_index(desc->crafter, name, entry->stage);

        const struct ngl_node *node = ngli_hmap_get_str(resources, name);
        if (!node)
            continue;

        if (node->cls->category != NGLI_NODE_CATEGORY_BLOCK)
            continue;

        const struct block_info *info = node->priv_data;
        const struct resource_map map = {.index = index, .info = info, .buffer_rev = SIZE_MAX};
        if (!ngli_darray_push(&desc->blocks_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

int ngli_pass_prepare(struct pass *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct rnode *rnode = ctx->rnode_pos;

    const int format = rnode->rendertarget_layout.depth_stencil.format;
    if (rnode->graphics_state.depth_test && !ngli_format_has_depth(format)) {
        LOG(ERROR, "depth testing is not supported on rendertargets with no depth attachment");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (rnode->graphics_state.stencil_test && !ngli_format_has_stencil(format)) {
        LOG(ERROR, "stencil operations are not supported on rendertargets with no stencil attachment");
        return NGL_ERROR_INVALID_USAGE;
    }

    struct graphics_state state = rnode->graphics_state;
    int ret = ngli_blending_apply_preset(&state, s->params.blending);
    if (ret < 0)
        return ret;

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    ctx->rnode_pos->id = ngli_darray_count(&s->pipeline_descs) - 1;

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    const struct pgcraft_params crafter_params = {
        .program_label     = s->params.program_label,
        .vert_base         = s->params.vert_base,
        .frag_base         = s->params.frag_base,
        .comp_base         = s->params.comp_base,
        .uniforms          = ngli_darray_data(&s->crafter_uniforms),
        .nb_uniforms       = ngli_darray_count(&s->crafter_uniforms),
        .textures          = ngli_darray_data(&s->crafter_textures),
        .nb_textures       = ngli_darray_count(&s->crafter_textures),
        .attributes        = ngli_darray_data(&s->crafter_attributes),
        .nb_attributes     = ngli_darray_count(&s->crafter_attributes),
        .blocks            = ngli_darray_data(&s->crafter_blocks),
        .nb_blocks         = ngli_darray_count(&s->crafter_blocks),
        .vert_out_vars     = s->params.vert_out_vars,
        .nb_vert_out_vars  = s->params.nb_vert_out_vars,
        .nb_frag_output    = s->params.nb_frag_output,
        .workgroup_size    = {NGLI_ARG_VEC3(s->params.workgroup_size)},
    };

    ret = ngli_pgcraft_craft(desc->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!desc->pipeline_compat)
        return NGL_ERROR_MEMORY;


    const struct pipeline_compat_params params = {
        .type = s->pipeline_type,
        .graphics = {
            .topology     = s->topology,
            .state        = state,
            .rt_layout    = rnode->rendertarget_layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(desc->crafter),
        },
        .program     = ngli_pgcraft_get_program(desc->crafter),
        .layout      = ngli_pgcraft_get_pipeline_layout(desc->crafter),
        .resources   = ngli_pgcraft_get_pipeline_resources(desc->crafter),
        .compat_info = ngli_pgcraft_get_compat_info(desc->crafter),
    };
    ret = ngli_pipeline_compat_init(desc->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    ret = build_uniforms_map(desc, &s->crafter_uniforms);
    if (ret < 0)
        return ret;

    ret = build_blocks_map(s, desc);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->normal_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_normal_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->resolution_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_resolution", NGLI_PROGRAM_SHADER_FRAG);

    ngli_darray_init(&desc->textures_map, sizeof(struct texture_map), 0);
    const struct pgcraft_compat_info *info = ngli_pgcraft_get_compat_info(desc->crafter);
    for (size_t i = 0; i < info->nb_texture_infos; i++) {
        const struct texture_map map = {.image = info->images[i], .image_rev = SIZE_MAX};
        if (!ngli_darray_push(&desc->textures_map, &map))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

int ngli_pass_init(struct pass *s, struct ngl_ctx *ctx, const struct pass_params *params)
{
    s->ctx = ctx;
    s->params = *params;

    ngli_darray_init(&s->crafter_attributes, sizeof(struct pgcraft_attribute), 0);
    ngli_darray_init(&s->crafter_textures, sizeof(struct pgcraft_texture), 0);
    ngli_darray_init(&s->crafter_uniforms, sizeof(struct pgcraft_uniform), 0);
    ngli_darray_init(&s->crafter_blocks, sizeof(struct pgcraft_block), 0);

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    int ret = register_builtin_uniforms(s);
    if (ret < 0)
        return ret;

    ret = params->geometry ? pass_graphics_init(s)
                           : pass_compute_init(s);
    if (ret < 0)
        return ret;

    return 0;
}

void ngli_pass_uninit(struct pass *s)
{
    if (!s->ctx)
        return;

    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    for (size_t i = 0; i < ngli_darray_count(&s->pipeline_descs); i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_compat_freep(&desc->pipeline_compat);
        ngli_pgcraft_freep(&desc->crafter);
        ngli_darray_reset(&desc->uniforms_map);
        ngli_darray_reset(&desc->blocks_map);
        ngli_darray_reset(&desc->textures_map);
    }
    ngli_darray_reset(&s->pipeline_descs);

    ngli_darray_reset(&s->crafter_attributes);
    ngli_darray_reset(&s->crafter_textures);
    ngli_darray_reset(&s->crafter_uniforms);
    ngli_darray_reset(&s->crafter_blocks);

    memset(s, 0, sizeof(*s));
}

int ngli_pass_exec(struct pass *s)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct pass_params *params = &s->params;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline_compat *pipeline_compat = desc->pipeline_compat;

    const float *modelview_matrix = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_compat_update_uniform(pipeline_compat, desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_compat_update_uniform(pipeline_compat, desc->projection_matrix_index, projection_matrix);

    const struct viewport viewport = ngli_gpu_ctx_get_viewport(ctx->gpu_ctx);

    const float resolution[2] = {(float)viewport.width, (float)viewport.height};
    ngli_pipeline_compat_update_uniform(pipeline_compat, desc->resolution_index, resolution);

    if (desc->normal_matrix_index >= 0) {
        float normal_matrix[3*3];
        ngli_mat3_from_mat4(normal_matrix, modelview_matrix);
        ngli_mat3_inverse(normal_matrix, normal_matrix);
        ngli_mat3_transpose(normal_matrix, normal_matrix);
        ngli_pipeline_compat_update_uniform(pipeline_compat, desc->normal_matrix_index, normal_matrix);
    }

    const struct uniform_map *uniform_map = ngli_darray_data(&desc->uniforms_map);
    for (size_t i = 0; i < ngli_darray_count(&desc->uniforms_map); i++)
        ngli_pipeline_compat_update_uniform(pipeline_compat, uniform_map[i].index, uniform_map[i].data);

    struct texture_map *texture_map = ngli_darray_data(&desc->textures_map);
    for (size_t i = 0; i < ngli_darray_count(&desc->textures_map); i++) {
        if (texture_map[i].image_rev != texture_map[i].image->rev) {
            ngli_pipeline_compat_update_image(pipeline_compat, (int32_t)i, texture_map[i].image);
            texture_map[i].image_rev = texture_map[i].image->rev;
        }
    }

    struct resource_map *resource_map = ngli_darray_data(&desc->blocks_map);
    for (size_t i = 0; i < ngli_darray_count(&desc->blocks_map); i++) {
        const struct block_info *info = resource_map[i].info;
        if (resource_map[i].buffer_rev != info->buffer_rev) {
            ngli_pipeline_compat_update_buffer(pipeline_compat, resource_map[i].index, info->buffer, 0, 0);
            resource_map[i].buffer_rev = info->buffer_rev;
        }
    }

    if (s->pipeline_type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        if (!ctx->render_pass_started) {
            struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
            ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
            ctx->render_pass_started = 1;
        }

        if (s->indices)
            ngli_pipeline_compat_draw_indexed(pipeline_compat, s->indices, s->indices_layout->format,
                                              (int)s->indices_layout->count, s->nb_instances);
        else
            ngli_pipeline_compat_draw(pipeline_compat, s->nb_vertices, s->nb_instances);
    } else {
        if (ctx->render_pass_started) {
            struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
            ngli_gpu_ctx_end_render_pass(gpu_ctx);
            ctx->render_pass_started = 0;
            ctx->current_rendertarget = ctx->available_rendertargets[1];
        }

        ngli_pipeline_compat_dispatch(pipeline_compat, NGLI_ARG_VEC3(params->workgroup_count));
    }

    return 0;
}
