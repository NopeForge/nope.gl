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

#include "bindgroup.h"
#include "buffer.h"
#include "gpu_limits.h"
#include "nopegl.h"
#include "pipeline.h"
#include "rendertarget.h"
#include "texture.h"

struct viewport {
    int32_t x, y, width, height;
};

struct scissor {
    int32_t x, y, width, height;
};

int ngli_viewport_is_valid(const struct viewport *viewport);

#define NGLI_FEATURE_COMPUTE                           (1 << 0)
#define NGLI_FEATURE_SOFTWARE                          (1 << 4)
#define NGLI_FEATURE_IMAGE_LOAD_STORE                  (1 << 9)
#define NGLI_FEATURE_STORAGE_BUFFER                    (1 << 10)
#define NGLI_FEATURE_DEPTH_STENCIL_RESOLVE             (1 << 11)
#define NGLI_FEATURE_TEXTURE_FLOAT_RENDERABLE          (1 << 12)
#define NGLI_FEATURE_TEXTURE_HALF_FLOAT_RENDERABLE     (1 << 13)
#define NGLI_FEATURE_BUFFER_MAP_PERSISTENT             (1 << 14)

struct gpu_ctx_class {
    const char *name;

    struct gpu_ctx *(*create)(const struct ngl_config *config);
    int (*init)(struct gpu_ctx *s);
    int (*resize)(struct gpu_ctx *s, int32_t width, int32_t height, const int32_t *viewport);
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

    struct rendertarget *(*get_default_rendertarget)(struct gpu_ctx *s, int load_op);
    const struct rendertarget_layout *(*get_default_rendertarget_layout)(struct gpu_ctx *s);

    void (*begin_render_pass)(struct gpu_ctx *s, struct rendertarget *rt);
    void (*end_render_pass)(struct gpu_ctx *s);

    int (*get_preferred_depth_format)(struct gpu_ctx *s);
    int (*get_preferred_depth_stencil_format)(struct gpu_ctx *s);

    void (*set_vertex_buffer)(struct gpu_ctx *s, uint32_t index, const struct buffer *buffer);
    void (*set_index_buffer)(struct gpu_ctx *s, const struct buffer *buffer, int format);

    void (*set_pipeline)(struct gpu_ctx *s, struct pipeline *pipeline);
    void (*set_bindgroup)(struct gpu_ctx *s, struct bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets);
    void (*draw)(struct gpu_ctx *s, int nb_vertices, int nb_instances);
    void (*draw_indexed)(struct gpu_ctx *s, int nb_indices, int nb_instances);
    void (*dispatch)(struct gpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);

    struct buffer *(*buffer_create)(struct gpu_ctx *ctx);
    int (*buffer_init)(struct buffer *s);
    int (*buffer_upload)(struct buffer *s, const void *data, size_t offset, size_t size);
    int (*buffer_map)(struct buffer *s, size_t offset, size_t size, void **datap);
    void (*buffer_unmap)(struct buffer *s);
    void (*buffer_freep)(struct buffer **sp);

    struct bindgroup_layout *(*bindgroup_layout_create)(struct gpu_ctx *gpu_ctx);
    int (*bindgroup_layout_init)(struct bindgroup_layout *s);
    void (*bindgroup_layout_freep)(struct bindgroup_layout **sp);

    struct bindgroup *(*bindgroup_create)(struct gpu_ctx *gpu_ctx);
    int (*bindgroup_init)(struct bindgroup *s, const struct bindgroup_params *params);
    int (*bindgroup_update_texture)(struct bindgroup *s, int32_t index, const struct texture_binding *binding);
    int (*bindgroup_update_buffer)(struct bindgroup *s, int32_t index, const struct buffer_binding *binding);
    void (*bindgroup_freep)(struct bindgroup **sp);

    struct pipeline *(*pipeline_create)(struct gpu_ctx *ctx);
    int (*pipeline_init)(struct pipeline *s);
    int (*pipeline_update_texture)(struct pipeline *s, int32_t index, const struct texture *texture);
    int (*pipeline_update_buffer)(struct pipeline *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size);
    void (*pipeline_freep)(struct pipeline **sp);

    struct program *(*program_create)(struct gpu_ctx *ctx);
    int (*program_init)(struct program *s, const struct program_params *params);
    void (*program_freep)(struct program **sp);

    struct rendertarget *(*rendertarget_create)(struct gpu_ctx *ctx);
    int (*rendertarget_init)(struct rendertarget *s);
    void (*rendertarget_freep)(struct rendertarget **sp);

    struct texture *(*texture_create)(struct gpu_ctx *ctx);
    int (*texture_init)(struct texture *s, const struct texture_params *params);
    int (*texture_upload)(struct texture *s, const uint8_t *data, int linesize);
    int (*texture_generate_mipmap)(struct texture *s);
    void (*texture_freep)(struct texture **sp);
};

struct gpu_ctx {
    struct ngl_config config;
    const char *backend_str;
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
    struct rendertarget *rendertarget;
    struct pipeline *pipeline;
    struct bindgroup *bindgroup;
    uint32_t dynamic_offsets[NGLI_MAX_DYNAMIC_OFFSETS];
    size_t nb_dynamic_offsets;
    const struct buffer **vertex_buffers;
    const struct buffer *index_buffer;
    int index_format;
    struct viewport viewport;
    struct scissor scissor;
};

struct gpu_ctx *ngli_gpu_ctx_create(const struct ngl_config *config);
int ngli_gpu_ctx_init(struct gpu_ctx *s);
int ngli_gpu_ctx_resize(struct gpu_ctx *s, int32_t width, int32_t height, const int32_t *viewport);
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

struct rendertarget *ngli_gpu_ctx_get_default_rendertarget(struct gpu_ctx *s, int load_op);
const struct rendertarget_layout *ngli_gpu_ctx_get_default_rendertarget_layout(struct gpu_ctx *s);

void ngli_gpu_ctx_begin_render_pass(struct gpu_ctx *s, struct rendertarget *rt);
void ngli_gpu_ctx_end_render_pass(struct gpu_ctx *s);

void ngli_gpu_ctx_set_viewport(struct gpu_ctx *s, const struct viewport *viewport);
struct viewport ngli_gpu_ctx_get_viewport(struct gpu_ctx *s);
void ngli_gpu_ctx_set_scissor(struct gpu_ctx *s, const struct scissor *scissor);
struct scissor ngli_gpu_ctx_get_scissor(struct gpu_ctx *s);

int ngli_gpu_ctx_get_preferred_depth_format(struct gpu_ctx *s);
int ngli_gpu_ctx_get_preferred_depth_stencil_format(struct gpu_ctx *s);

void ngli_gpu_ctx_set_pipeline(struct gpu_ctx *s, struct pipeline *pipeline);
void ngli_gpu_ctx_set_bindgroup(struct gpu_ctx *s, struct bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets);
void ngli_gpu_ctx_draw(struct gpu_ctx *s, int nb_vertices, int nb_instances);
void ngli_gpu_ctx_draw_indexed(struct gpu_ctx *s, int nb_indices, int nb_instances);
void ngli_gpu_ctx_dispatch(struct gpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);

void ngli_gpu_ctx_set_vertex_buffer(struct gpu_ctx *s, uint32_t index, const struct buffer *buffer);
void ngli_gpu_ctx_set_index_buffer(struct gpu_ctx *s, const struct buffer *buffer, int format);

#endif
