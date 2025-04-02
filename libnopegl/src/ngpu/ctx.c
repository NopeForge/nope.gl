/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2019-2022 GoPro Inc.
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

#include <string.h>

#include "ctx.h"
#include "log.h"
#include "ngl_config.h"
#include "rendertarget.h"
#include "utils/memory.h"

const char *ngli_backend_get_string_id(int backend)
{
    switch (backend) {
    case NGL_BACKEND_AUTO:     return "auto";
    case NGL_BACKEND_OPENGL:   return "opengl";
    case NGL_BACKEND_OPENGLES: return "opengles";
    case NGL_BACKEND_VULKAN:   return "vulkan";
    default:                   return "unknown";
    }
}

const char *ngli_backend_get_full_name(int backend)
{
    switch (backend) {
    case NGL_BACKEND_AUTO:     return "Auto";
    case NGL_BACKEND_OPENGL:   return "OpenGL";
    case NGL_BACKEND_OPENGLES: return "OpenGL ES";
    case NGL_BACKEND_VULKAN:   return "Vulkan";
    default:                   return "Unknown";
    }
}

int ngpu_viewport_is_valid(const struct ngpu_viewport *viewport)
{
    return viewport->width > 0 && viewport->height > 0;
}

extern const struct ngpu_ctx_class ngpu_ctx_gl;
extern const struct ngpu_ctx_class ngpu_ctx_gles;
extern const struct ngpu_ctx_class ngpu_ctx_vk;

static const struct ngpu_ctx_class *backend_map[NGL_BACKEND_NB] = {
#ifdef BACKEND_GL
    [NGL_BACKEND_OPENGL] = &ngpu_ctx_gl,
#endif
#ifdef BACKEND_GLES
    [NGL_BACKEND_OPENGLES] = &ngpu_ctx_gles,
#endif
#ifdef BACKEND_VK
    [NGL_BACKEND_VULKAN] = &ngpu_ctx_vk,
#endif
};

struct ngpu_ctx *ngpu_ctx_create(const struct ngl_config *config)
{
    if (config->backend < 0 ||
        config->backend >= NGLI_ARRAY_NB(backend_map)) {
        LOG(ERROR, "unknown backend %d", config->backend);
        return NULL;
    }
    if (!backend_map[config->backend]) {
        LOG(ERROR, "backend \"%s\" not available with this build",
            ngli_backend_get_string_id(config->backend));
        return NULL;
    }

    struct ngl_config ctx_config = {0};
    int ret = ngli_config_copy(&ctx_config, config);
    if (ret < 0)
        return NULL;

    const struct ngpu_ctx_class *cls = backend_map[config->backend];
    struct ngpu_ctx *s = cls->create(config);
    if (!s) {
        ngli_config_reset(&ctx_config);
        return NULL;
    }
    s->config = ctx_config;
    s->cls = cls;
    return s;
}

int ngpu_ctx_init(struct ngpu_ctx *s)
{
    int ret = s->cls->init(s);
    if (ret < 0)
        return ret;

    return ngpu_pgcache_init(&s->program_cache, s);
}

int ngpu_ctx_resize(struct ngpu_ctx *s, int32_t width, int32_t height)
{
    const struct ngpu_ctx_class *cls = s->cls;
    return cls->resize(s, width, height);
}

int ngpu_ctx_set_capture_buffer(struct ngpu_ctx *s, void *capture_buffer)
{
    const struct ngpu_ctx_class *cls = s->cls;
    return cls->set_capture_buffer(s, capture_buffer);
}

uint32_t ngpu_ctx_advance_frame(struct ngpu_ctx *s)
{
    s->current_frame_index = (s->current_frame_index + 1) % s->nb_in_flight_frames;
    return s->current_frame_index;
}

uint32_t ngpu_ctx_get_current_frame_index(struct ngpu_ctx *s)
{
    return s->current_frame_index;
}

uint32_t ngpu_ctx_get_nb_in_flight_frames(struct ngpu_ctx *s)
{
    return s->nb_in_flight_frames;
}

int ngpu_ctx_begin_update(struct ngpu_ctx *s)
{
    return s->cls->begin_update(s);
}

int ngpu_ctx_end_update(struct ngpu_ctx *s)
{
    return s->cls->end_update(s);
}

int ngpu_ctx_begin_draw(struct ngpu_ctx *s)
{
    return s->cls->begin_draw(s);
}

int ngpu_ctx_end_draw(struct ngpu_ctx *s, double t)
{
    return s->cls->end_draw(s, t);
}

int ngpu_ctx_query_draw_time(struct ngpu_ctx *s, int64_t *time)
{
    return s->cls->query_draw_time(s, time);
}

void ngpu_ctx_wait_idle(struct ngpu_ctx *s)
{
    s->cls->wait_idle(s);
}

void ngpu_ctx_freep(struct ngpu_ctx **sp)
{
    if (!*sp)
        return;

    struct ngpu_ctx *s = *sp;

    ngpu_pgcache_reset(&s->program_cache);
    s->cls->destroy(s);

    ngli_config_reset(&s->config);
    ngli_freep(sp);
}

int ngpu_ctx_transform_cull_mode(struct ngpu_ctx *s, int cull_mode)
{
    return s->cls->transform_cull_mode(s, cull_mode);
}

void ngpu_ctx_transform_projection_matrix(struct ngpu_ctx *s, float *dst)
{
    s->cls->transform_projection_matrix(s, dst);
}

void ngpu_ctx_begin_render_pass(struct ngpu_ctx *s, struct ngpu_rendertarget *rt)
{
    ngli_assert(rt);
    ngli_assert(!s->rendertarget);

    s->rendertarget = rt;
    s->cls->begin_render_pass(s, rt);
}

void ngpu_ctx_end_render_pass(struct ngpu_ctx *s)
{
    s->cls->end_render_pass(s);
    s->rendertarget = NULL;
}

void ngpu_ctx_get_rendertarget_uvcoord_matrix(struct ngpu_ctx *s, float *dst)
{
    s->cls->get_rendertarget_uvcoord_matrix(s, dst);
}

struct ngpu_rendertarget *ngpu_ctx_get_default_rendertarget(struct ngpu_ctx *s, enum ngpu_load_op load_op)
{
    return s->cls->get_default_rendertarget(s, load_op);
}

const struct ngpu_rendertarget_layout *ngpu_ctx_get_default_rendertarget_layout(struct ngpu_ctx *s)
{
    return s->cls->get_default_rendertarget_layout(s);
}

void ngpu_ctx_get_default_rendertarget_size(struct ngpu_ctx *s, int32_t *width, int32_t *height)
{
    s->cls->get_default_rendertarget_size(s, width, height);
}

void ngpu_ctx_set_viewport(struct ngpu_ctx *s, const struct ngpu_viewport *viewport)
{
    ngli_assert(s->rendertarget);
    s->cls->set_viewport(s, viewport);
}

void ngpu_ctx_set_scissor(struct ngpu_ctx *s, const struct ngpu_scissor *scissor)
{
    ngli_assert(s->rendertarget);
    s->cls->set_scissor(s, scissor);
}

enum ngpu_format ngpu_ctx_get_preferred_depth_format(struct ngpu_ctx *s)
{
    return s->cls->get_preferred_depth_format(s);
}

enum ngpu_format ngpu_ctx_get_preferred_depth_stencil_format(struct ngpu_ctx *s)
{
    return s->cls->get_preferred_depth_stencil_format(s);
}

uint32_t ngpu_ctx_get_format_features(struct ngpu_ctx *s, enum ngpu_format format)
{
    return s->cls->get_format_features(s, format);
}

void ngpu_ctx_generate_texture_mipmap(struct ngpu_ctx *s, struct ngpu_texture *texture)
{
    ngli_assert(!s->rendertarget);
    s->cls->generate_texture_mipmap(s, texture);
}

void ngpu_ctx_set_pipeline(struct ngpu_ctx *s, struct ngpu_pipeline *pipeline)
{
    s->pipeline = pipeline;
    s->cls->set_pipeline(s, pipeline);
}

void ngpu_ctx_set_bindgroup(struct ngpu_ctx *s, struct ngpu_bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets)
{
    s->bindgroup = bindgroup;

    ngli_assert(bindgroup->layout->nb_dynamic_offsets == nb_offsets);
    memcpy(s->dynamic_offsets, offsets, nb_offsets * sizeof(*s->dynamic_offsets));
    s->nb_dynamic_offsets = nb_offsets;

    s->cls->set_bindgroup(s, bindgroup, offsets, nb_offsets);
}

static void validate_vertex_buffers(struct ngpu_ctx *s)
{
    const struct ngpu_pipeline *pipeline = s->pipeline;
    const struct ngpu_pipeline_graphics *graphics = &pipeline->graphics;
    const struct ngpu_vertex_state *vertex_state = &graphics->vertex_state;
    for (size_t i = 0; i < vertex_state->nb_buffers; i++) {
        ngli_assert(s->vertex_buffers[i]);
    }
}

void ngpu_ctx_draw(struct ngpu_ctx *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex)
{
    ngli_assert(s->pipeline);
    validate_vertex_buffers(s);
    ngli_assert(s->bindgroup);
    const struct ngpu_bindgroup_layout *p_layout = s->pipeline->layout.bindgroup_layout;
    const struct ngpu_bindgroup_layout *b_layout = s->bindgroup->layout;
    ngli_assert(ngpu_bindgroup_layout_is_compatible(p_layout, b_layout));

    s->cls->draw(s, nb_vertices, nb_instances, first_vertex);
}

void ngpu_ctx_draw_indexed(struct ngpu_ctx *s, uint32_t nb_indices, uint32_t nb_instances)
{
    ngli_assert(s->pipeline);
    validate_vertex_buffers(s);
    ngli_assert(s->bindgroup);
    const struct ngpu_bindgroup_layout *p_layout = s->pipeline->layout.bindgroup_layout;
    const struct ngpu_bindgroup_layout *b_layout = s->bindgroup->layout;
    ngli_assert(ngpu_bindgroup_layout_is_compatible(p_layout, b_layout));

    s->cls->draw_indexed(s, nb_indices, nb_instances);
}

void ngpu_ctx_dispatch(struct ngpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    ngli_assert(s->pipeline);
    ngli_assert(s->bindgroup);
    const struct ngpu_bindgroup_layout *p_layout = s->pipeline->layout.bindgroup_layout;
    const struct ngpu_bindgroup_layout *b_layout = s->bindgroup->layout;
    ngli_assert(ngpu_bindgroup_layout_is_compatible(p_layout, b_layout));

    s->cls->dispatch(s, nb_group_x, nb_group_y, nb_group_z);
}

void ngpu_ctx_set_vertex_buffer(struct ngpu_ctx *s, uint32_t index, const struct ngpu_buffer *buffer)
{
    struct ngpu_limits *limits = &s->limits;
    ngli_assert(index < limits->max_vertex_attributes);
    s->vertex_buffers[index] = buffer;
    s->cls->set_vertex_buffer(s, index, buffer);
}

void ngpu_ctx_set_index_buffer(struct ngpu_ctx *s, const struct ngpu_buffer *buffer, enum ngpu_format format)
{
    s->index_buffer = buffer;
    s->index_format = format;
    s->cls->set_index_buffer(s, buffer, format);
}
