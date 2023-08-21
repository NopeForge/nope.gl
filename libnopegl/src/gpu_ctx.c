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

#include "gpu_ctx.h"
#include "log.h"
#include "memory.h"
#include "internal.h"
#include "rendertarget.h"

int ngli_viewport_is_valid(const struct viewport *viewport)
{
    return viewport->width > 0 && viewport->height > 0;
}

extern const struct gpu_ctx_class ngli_gpu_ctx_gl;
extern const struct gpu_ctx_class ngli_gpu_ctx_gles;
extern const struct gpu_ctx_class ngli_gpu_ctx_vk;

static const struct {
    const char *string_id;
    const struct gpu_ctx_class *cls;
} backend_map[] = {
    [NGL_BACKEND_OPENGL] = {
        .string_id = "opengl",
#ifdef BACKEND_GL
        .cls = &ngli_gpu_ctx_gl,
#endif
    },
    [NGL_BACKEND_OPENGLES] = {
        .string_id = "opengles",
#ifdef BACKEND_GLES
        .cls = &ngli_gpu_ctx_gles,
#endif
    },
    [NGL_BACKEND_VULKAN] = {
        .string_id = "vulkan",
#ifdef BACKEND_VK
        .cls = &ngli_gpu_ctx_vk,
#endif
    },
};

struct gpu_ctx *ngli_gpu_ctx_create(const struct ngl_config *config)
{
    if (config->backend < 0 ||
        config->backend >= NGLI_ARRAY_NB(backend_map)) {
        LOG(ERROR, "unknown backend %d", config->backend);
        return NULL;
    }
    if (!backend_map[config->backend].cls) {
        LOG(ERROR, "backend \"%s\" not available with this build",
            backend_map[config->backend].string_id);
        return NULL;
    }

    struct ngl_config ctx_config = {0};
    int ret = ngli_config_copy(&ctx_config, config);
    if (ret < 0)
        return NULL;

    const struct gpu_ctx_class *cls = backend_map[config->backend].cls;
    struct gpu_ctx *s = cls->create(config);
    if (!s) {
        ngli_config_reset(&ctx_config);
        return NULL;
    }
    s->config = ctx_config;
    s->backend_str = backend_map[config->backend].string_id;
    s->cls = cls;
    return s;
}

int ngli_gpu_ctx_init(struct gpu_ctx *s)
{
    int ret = s->cls->init(s);
    if (ret < 0)
        return ret;

    const struct gpu_limits *limits = &s->limits;
    s->vertex_buffers = ngli_calloc(limits->max_vertex_attributes, sizeof(struct buffer *));
    if (!s->vertex_buffers)
        return NGL_ERROR_MEMORY;

    return 0;
}

int ngli_gpu_ctx_resize(struct gpu_ctx *s, int32_t width, int32_t height, const int32_t *viewport)
{
    const struct gpu_ctx_class *cls = s->cls;
    return cls->resize(s, width, height, viewport);
}

int ngli_gpu_ctx_set_capture_buffer(struct gpu_ctx *s, void *capture_buffer)
{
    const struct gpu_ctx_class *cls = s->cls;
    return cls->set_capture_buffer(s, capture_buffer);
}

int ngli_gpu_ctx_begin_update(struct gpu_ctx *s, double t)
{
    return s->cls->begin_update(s, t);
}

int ngli_gpu_ctx_end_update(struct gpu_ctx *s, double t)
{
    return s->cls->end_update(s, t);
}

int ngli_gpu_ctx_begin_draw(struct gpu_ctx *s, double t)
{
    return s->cls->begin_draw(s, t);
}

int ngli_gpu_ctx_end_draw(struct gpu_ctx *s, double t)
{
    return s->cls->end_draw(s, t);
}

int ngli_gpu_ctx_query_draw_time(struct gpu_ctx *s, int64_t *time)
{
    return s->cls->query_draw_time(s, time);
}

void ngli_gpu_ctx_wait_idle(struct gpu_ctx *s)
{
    s->cls->wait_idle(s);
}

void ngli_gpu_ctx_freep(struct gpu_ctx **sp)
{
    if (!*sp)
        return;

    struct gpu_ctx *s = *sp;
    ngli_freep(&s->vertex_buffers);

    const struct gpu_ctx_class *cls = s->cls;
    if (cls)
        cls->destroy(s);

    ngli_config_reset(&s->config);
    ngli_freep(sp);
}

int ngli_gpu_ctx_transform_cull_mode(struct gpu_ctx *s, int cull_mode)
{
    return s->cls->transform_cull_mode(s, cull_mode);
}

void ngli_gpu_ctx_transform_projection_matrix(struct gpu_ctx *s, float *dst)
{
    s->cls->transform_projection_matrix(s, dst);
}

void ngli_gpu_ctx_begin_render_pass(struct gpu_ctx *s, struct rendertarget *rt)
{
    ngli_assert(rt);
    ngli_assert(!s->rendertarget);

    s->rendertarget = rt;
    s->cls->begin_render_pass(s, rt);
}

void ngli_gpu_ctx_end_render_pass(struct gpu_ctx *s)
{
    s->cls->end_render_pass(s);
    s->rendertarget = NULL;
}

void ngli_gpu_ctx_get_rendertarget_uvcoord_matrix(struct gpu_ctx *s, float *dst)
{
    s->cls->get_rendertarget_uvcoord_matrix(s, dst);
}

struct rendertarget *ngli_gpu_ctx_get_default_rendertarget(struct gpu_ctx *s, int load_op)
{
    return s->cls->get_default_rendertarget(s, load_op);
}

const struct rendertarget_layout *ngli_gpu_ctx_get_default_rendertarget_layout(struct gpu_ctx *s)
{
    return s->cls->get_default_rendertarget_layout(s);
}

void ngli_gpu_ctx_set_viewport(struct gpu_ctx *s, const struct viewport *viewport)
{
    s->viewport = *viewport;
}

struct viewport ngli_gpu_ctx_get_viewport(struct gpu_ctx *s)
{
    return s->viewport;
}

void ngli_gpu_ctx_set_scissor(struct gpu_ctx *s, const struct scissor *scissor)
{
    s->scissor = *scissor;
}

struct scissor ngli_gpu_ctx_get_scissor(struct gpu_ctx *s)
{
    return s->scissor;
}

int ngli_gpu_ctx_get_preferred_depth_format(struct gpu_ctx *s)
{
    return s->cls->get_preferred_depth_format(s);
}

int ngli_gpu_ctx_get_preferred_depth_stencil_format(struct gpu_ctx *s)
{
    return s->cls->get_preferred_depth_stencil_format(s);
}

void ngli_gpu_ctx_set_pipeline(struct gpu_ctx *s, struct pipeline *pipeline)
{
    s->pipeline = pipeline;
    s->cls->set_pipeline(s, pipeline);
}

void ngli_gpu_ctx_set_bindgroup(struct gpu_ctx *s, struct bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets)
{
    s->bindgroup = bindgroup;

    ngli_assert(bindgroup->layout->nb_dynamic_offsets == nb_offsets);
    memcpy(s->dynamic_offsets, offsets, nb_offsets * sizeof(*s->dynamic_offsets));
    s->nb_dynamic_offsets = nb_offsets;

    s->cls->set_bindgroup(s, bindgroup, offsets, nb_offsets);
}

static void validate_vertex_buffers(struct gpu_ctx *s)
{
    const struct pipeline *pipeline = s->pipeline;
    const struct pipeline_graphics *graphics = &pipeline->graphics;
    const struct vertex_state *vertex_state = &graphics->vertex_state;
    for (size_t i = 0; i < vertex_state->nb_buffers; i++) {
        ngli_assert(s->vertex_buffers[i]);
    }
}

void ngli_gpu_ctx_draw(struct gpu_ctx *s, int nb_vertices, int nb_instances)
{
    ngli_assert(s->pipeline);
    validate_vertex_buffers(s);
    ngli_assert(s->bindgroup);
    const struct bindgroup_layout *p_layout = s->pipeline->layout.bindgroup_layout;
    const struct bindgroup_layout *b_layout = s->bindgroup->layout;
    ngli_assert(ngli_bindgroup_layout_is_compatible(p_layout, b_layout));

    s->cls->draw(s, nb_vertices, nb_instances);
}

void ngli_gpu_ctx_draw_indexed(struct gpu_ctx *s, int nb_indices, int nb_instances)
{
    ngli_assert(s->pipeline);
    validate_vertex_buffers(s);
    ngli_assert(s->bindgroup);
    const struct bindgroup_layout *p_layout = s->pipeline->layout.bindgroup_layout;
    const struct bindgroup_layout *b_layout = s->bindgroup->layout;
    ngli_assert(ngli_bindgroup_layout_is_compatible(p_layout, b_layout));

    s->cls->draw_indexed(s, nb_indices, nb_instances);
}

void ngli_gpu_ctx_dispatch(struct gpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    ngli_assert(s->pipeline);
    ngli_assert(s->bindgroup);
    const struct bindgroup_layout *p_layout = s->pipeline->layout.bindgroup_layout;
    const struct bindgroup_layout *b_layout = s->bindgroup->layout;
    ngli_assert(ngli_bindgroup_layout_is_compatible(p_layout, b_layout));

    s->cls->dispatch(s, nb_group_x, nb_group_y, nb_group_z);
}

void ngli_gpu_ctx_set_vertex_buffer(struct gpu_ctx *s, uint32_t index, const struct buffer *buffer)
{
    struct gpu_limits *limits = &s->limits;
    ngli_assert(index < limits->max_vertex_attributes);
    s->vertex_buffers[index] = buffer;
    s->cls->set_vertex_buffer(s, index, buffer);
}

void ngli_gpu_ctx_set_index_buffer(struct gpu_ctx *s, const struct buffer *buffer, int format)
{
    s->index_buffer = buffer;
    s->index_format = format;
    s->cls->set_index_buffer(s, buffer, format);
}
