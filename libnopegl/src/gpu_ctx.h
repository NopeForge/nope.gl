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

#ifndef GPU_CTX_H
#define GPU_CTX_H

#include <stdint.h>

#include "gpu_bindgroup.h"
#include "gpu_buffer.h"
#include "gpu_limits.h"
#include "gpu_pipeline.h"
#include "gpu_rendertarget.h"
#include "gpu_texture.h"
#include "gpu_graphics_state.h"
#include "nopegl.h"

const char *ngli_backend_get_string_id(int backend);
const char *ngli_backend_get_full_name(int backend);

struct gpu_viewport {
    int32_t x, y, width, height;
};

struct gpu_scissor {
    int32_t x, y, width, height;
};

int ngli_gpu_viewport_is_valid(const struct gpu_viewport *viewport);

enum {
    NGLI_GPU_PRIMITIVE_TOPOLOGY_POINT_LIST,
    NGLI_GPU_PRIMITIVE_TOPOLOGY_LINE_LIST,
    NGLI_GPU_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    NGLI_GPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    NGLI_GPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    NGLI_GPU_PRIMITIVE_TOPOLOGY_NB
};

#define NGLI_GPU_FEATURE_SOFTWARE                          (1U << 0)
#define NGLI_GPU_FEATURE_COMPUTE                           (1U << 1)
#define NGLI_GPU_FEATURE_IMAGE_LOAD_STORE                  (1U << 2)
#define NGLI_GPU_FEATURE_STORAGE_BUFFER                    (1U << 3)
#define NGLI_GPU_FEATURE_BUFFER_MAP_PERSISTENT             (1U << 4)
#define NGLI_GPU_FEATURE_DEPTH_STENCIL_RESOLVE             (1U << 5)

struct gpu_ctx_class {
    uint32_t id;

    struct gpu_ctx *(*create)(const struct ngl_config *config);
    int (*init)(struct gpu_ctx *s);
    int (*resize)(struct gpu_ctx *s, int32_t width, int32_t height);
    int (*set_capture_buffer)(struct gpu_ctx *s, void *capture_buffer);
    int (*begin_update)(struct gpu_ctx *s, double t);
    int (*end_update)(struct gpu_ctx *s, double t);
    int (*begin_draw)(struct gpu_ctx *s, double t);
    int (*end_draw)(struct gpu_ctx *s, double t);
    int (*query_draw_time)(struct gpu_ctx *s, int64_t *time);
    void (*wait_idle)(struct gpu_ctx *s);
    void (*destroy)(struct gpu_ctx *s);

    int (*transform_cull_mode)(struct gpu_ctx *s, int cull_mode);
    void (*transform_projection_matrix)(struct gpu_ctx *s, float *dst);
    void (*get_rendertarget_uvcoord_matrix)(struct gpu_ctx *s, float *dst);

    struct gpu_rendertarget *(*get_default_rendertarget)(struct gpu_ctx *s, int load_op);
    const struct gpu_rendertarget_layout *(*get_default_rendertarget_layout)(struct gpu_ctx *s);
    void (*get_default_rendertarget_size)(struct gpu_ctx *s, int32_t *width, int32_t *height);

    void (*begin_render_pass)(struct gpu_ctx *s, struct gpu_rendertarget *rt);
    void (*end_render_pass)(struct gpu_ctx *s);

    void (*set_viewport)(struct gpu_ctx *s, const struct gpu_viewport *viewport);
    void (*set_scissor)(struct gpu_ctx *s, const struct gpu_scissor *scissor);

    int (*get_preferred_depth_format)(struct gpu_ctx *s);
    int (*get_preferred_depth_stencil_format)(struct gpu_ctx *s);
    uint32_t (*get_format_features)(struct gpu_ctx *s, int format);

    void (*set_vertex_buffer)(struct gpu_ctx *s, uint32_t index, const struct gpu_buffer *buffer);
    void (*set_index_buffer)(struct gpu_ctx *s, const struct gpu_buffer *buffer, int format);

    void (*generate_texture_mipmap)(struct gpu_ctx *s, struct gpu_texture *texture);

    void (*set_pipeline)(struct gpu_ctx *s, struct gpu_pipeline *pipeline);
    void (*set_bindgroup)(struct gpu_ctx *s, struct gpu_bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets);
    void (*draw)(struct gpu_ctx *s, int nb_vertices, int nb_instances);
    void (*draw_indexed)(struct gpu_ctx *s, int nb_indices, int nb_instances);
    void (*dispatch)(struct gpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);

    struct gpu_buffer *(*buffer_create)(struct gpu_ctx *ctx);
    int (*buffer_init)(struct gpu_buffer *s);
    int (*buffer_upload)(struct gpu_buffer *s, const void *data, size_t offset, size_t size);
    int (*buffer_map)(struct gpu_buffer *s, size_t offset, size_t size, void **datap);
    void (*buffer_unmap)(struct gpu_buffer *s);
    void (*buffer_freep)(struct gpu_buffer **sp);

    struct gpu_bindgroup_layout *(*bindgroup_layout_create)(struct gpu_ctx *gpu_ctx);
    int (*bindgroup_layout_init)(struct gpu_bindgroup_layout *s);
    void (*bindgroup_layout_freep)(struct gpu_bindgroup_layout **sp);

    struct gpu_bindgroup *(*bindgroup_create)(struct gpu_ctx *gpu_ctx);
    int (*bindgroup_init)(struct gpu_bindgroup *s, const struct gpu_bindgroup_params *params);
    int (*bindgroup_update_texture)(struct gpu_bindgroup *s, int32_t index, const struct gpu_texture_binding *binding);
    int (*bindgroup_update_buffer)(struct gpu_bindgroup *s, int32_t index, const struct gpu_buffer_binding *binding);
    void (*bindgroup_freep)(struct gpu_bindgroup **sp);

    struct gpu_pipeline *(*pipeline_create)(struct gpu_ctx *ctx);
    int (*pipeline_init)(struct gpu_pipeline *s);
    void (*pipeline_freep)(struct gpu_pipeline **sp);

    struct gpu_program *(*program_create)(struct gpu_ctx *ctx);
    int (*program_init)(struct gpu_program *s, const struct gpu_program_params *params);
    void (*program_freep)(struct gpu_program **sp);

    struct gpu_rendertarget *(*rendertarget_create)(struct gpu_ctx *ctx);
    int (*rendertarget_init)(struct gpu_rendertarget *s);
    void (*rendertarget_freep)(struct gpu_rendertarget **sp);

    struct gpu_texture *(*texture_create)(struct gpu_ctx *ctx);
    int (*texture_init)(struct gpu_texture *s, const struct gpu_texture_params *params);
    int (*texture_upload)(struct gpu_texture *s, const uint8_t *data, int linesize);
    int (*texture_generate_mipmap)(struct gpu_texture *s);
    void (*texture_freep)(struct gpu_texture **sp);
};

struct gpu_ctx {
    struct ngl_config config;
    const struct gpu_ctx_class *cls;

    int version;
    int language_version;
    uint64_t features;
    struct gpu_limits limits;

#if DEBUG_GPU_CAPTURE
    struct gpu_capture_ctx *gpu_capture_ctx;
    int gpu_capture;
#endif

    /* State */
    struct gpu_rendertarget *rendertarget;
    struct gpu_pipeline *pipeline;
    struct gpu_bindgroup *bindgroup;
    uint32_t dynamic_offsets[NGLI_GPU_MAX_DYNAMIC_OFFSETS];
    size_t nb_dynamic_offsets;
    const struct gpu_buffer *vertex_buffers[NGLI_GPU_MAX_VERTEX_BUFFERS];
    const struct gpu_buffer *index_buffer;
    int index_format;
};

struct gpu_ctx *ngli_gpu_ctx_create(const struct ngl_config *config);
int ngli_gpu_ctx_init(struct gpu_ctx *s);
int ngli_gpu_ctx_resize(struct gpu_ctx *s, int32_t width, int32_t height);
int ngli_gpu_ctx_set_capture_buffer(struct gpu_ctx *s, void *capture_buffer);
int ngli_gpu_ctx_begin_update(struct gpu_ctx *s, double t);
int ngli_gpu_ctx_end_update(struct gpu_ctx *s, double t);
int ngli_gpu_ctx_begin_draw(struct gpu_ctx *s, double t);
int ngli_gpu_ctx_query_draw_time(struct gpu_ctx *s, int64_t *time);
int ngli_gpu_ctx_end_draw(struct gpu_ctx *s, double t);
void ngli_gpu_ctx_wait_idle(struct gpu_ctx *s);
void ngli_gpu_ctx_freep(struct gpu_ctx **sp);

int ngli_gpu_ctx_transform_cull_mode(struct gpu_ctx *s, int cull_mode);
void ngli_gpu_ctx_transform_projection_matrix(struct gpu_ctx *s, float *dst);
void ngli_gpu_ctx_get_rendertarget_uvcoord_matrix(struct gpu_ctx *s, float *dst);

struct gpu_rendertarget *ngli_gpu_ctx_get_default_rendertarget(struct gpu_ctx *s, int load_op);
const struct gpu_rendertarget_layout *ngli_gpu_ctx_get_default_rendertarget_layout(struct gpu_ctx *s);
void ngli_gpu_ctx_get_default_rendertarget_size(struct gpu_ctx *s, int32_t *width, int32_t *height);

void ngli_gpu_ctx_begin_render_pass(struct gpu_ctx *s, struct gpu_rendertarget *rt);
void ngli_gpu_ctx_end_render_pass(struct gpu_ctx *s);

void ngli_gpu_ctx_set_viewport(struct gpu_ctx *s, const struct gpu_viewport *viewport);
void ngli_gpu_ctx_set_scissor(struct gpu_ctx *s, const struct gpu_scissor *scissor);

int ngli_gpu_ctx_get_preferred_depth_format(struct gpu_ctx *s);
int ngli_gpu_ctx_get_preferred_depth_stencil_format(struct gpu_ctx *s);
uint32_t ngli_gpu_ctx_get_format_features(struct gpu_ctx *s, int format);

void ngli_gpu_ctx_generate_texture_mipmap(struct gpu_ctx *s, struct gpu_texture *texture);

void ngli_gpu_ctx_set_pipeline(struct gpu_ctx *s, struct gpu_pipeline *pipeline);
void ngli_gpu_ctx_set_bindgroup(struct gpu_ctx *s, struct gpu_bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets);
void ngli_gpu_ctx_draw(struct gpu_ctx *s, int nb_vertices, int nb_instances);
void ngli_gpu_ctx_draw_indexed(struct gpu_ctx *s, int nb_indices, int nb_instances);
void ngli_gpu_ctx_dispatch(struct gpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);

void ngli_gpu_ctx_set_vertex_buffer(struct gpu_ctx *s, uint32_t index, const struct gpu_buffer *buffer);
void ngli_gpu_ctx_set_index_buffer(struct gpu_ctx *s, const struct gpu_buffer *buffer, int format);

#endif
