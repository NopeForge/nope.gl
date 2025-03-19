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

#ifndef NGPU_CTX_H
#define NGPU_CTX_H

#include <stdint.h>

#include "bindgroup.h"
#include "buffer.h"
#include "graphics_state.h"
#include "limits.h"
#include "nopegl.h"
#include "pipeline.h"
#include "rendertarget.h"
#include "texture.h"

const char *ngli_backend_get_string_id(int backend);
const char *ngli_backend_get_full_name(int backend);

struct ngpu_viewport {
    int32_t x, y, width, height;
};

struct ngpu_scissor {
    int32_t x, y, width, height;
};

int ngpu_viewport_is_valid(const struct ngpu_viewport *viewport);

#define NGPU_FEATURE_SOFTWARE                          (1U << 0)
#define NGPU_FEATURE_COMPUTE                           (1U << 1)
#define NGPU_FEATURE_IMAGE_LOAD_STORE                  (1U << 2)
#define NGPU_FEATURE_STORAGE_BUFFER                    (1U << 3)
#define NGPU_FEATURE_BUFFER_MAP_PERSISTENT             (1U << 4)
#define NGPU_FEATURE_DEPTH_STENCIL_RESOLVE             (1U << 5)

struct ngpu_ctx_class {
    uint32_t id;

    struct ngpu_ctx *(*create)(const struct ngl_config *config);
    int (*init)(struct ngpu_ctx *s);
    int (*resize)(struct ngpu_ctx *s, int32_t width, int32_t height);
    int (*set_capture_buffer)(struct ngpu_ctx *s, void *capture_buffer);
    int (*begin_update)(struct ngpu_ctx *s);
    int (*end_update)(struct ngpu_ctx *s);
    int (*begin_draw)(struct ngpu_ctx *s);
    int (*end_draw)(struct ngpu_ctx *s, double t);
    int (*query_draw_time)(struct ngpu_ctx *s, int64_t *time);
    void (*wait_idle)(struct ngpu_ctx *s);
    void (*destroy)(struct ngpu_ctx *s);

    int (*transform_cull_mode)(struct ngpu_ctx *s, int cull_mode);
    void (*transform_projection_matrix)(struct ngpu_ctx *s, float *dst);
    void (*get_rendertarget_uvcoord_matrix)(struct ngpu_ctx *s, float *dst);

    struct ngpu_rendertarget *(*get_default_rendertarget)(struct ngpu_ctx *s, enum ngpu_load_op load_op);
    const struct ngpu_rendertarget_layout *(*get_default_rendertarget_layout)(struct ngpu_ctx *s);
    void (*get_default_rendertarget_size)(struct ngpu_ctx *s, int32_t *width, int32_t *height);

    void (*begin_render_pass)(struct ngpu_ctx *s, struct ngpu_rendertarget *rt);
    void (*end_render_pass)(struct ngpu_ctx *s);

    void (*set_viewport)(struct ngpu_ctx *s, const struct ngpu_viewport *viewport);
    void (*set_scissor)(struct ngpu_ctx *s, const struct ngpu_scissor *scissor);

    enum ngpu_format (*get_preferred_depth_format)(struct ngpu_ctx *s);
    enum ngpu_format (*get_preferred_depth_stencil_format)(struct ngpu_ctx *s);
    uint32_t (*get_format_features)(struct ngpu_ctx *s, enum ngpu_format format);

    void (*set_vertex_buffer)(struct ngpu_ctx *s, uint32_t index, const struct ngpu_buffer *buffer);
    void (*set_index_buffer)(struct ngpu_ctx *s, const struct ngpu_buffer *buffer, enum ngpu_format format);

    void (*generate_texture_mipmap)(struct ngpu_ctx *s, struct ngpu_texture *texture);

    void (*set_pipeline)(struct ngpu_ctx *s, struct ngpu_pipeline *pipeline);
    void (*set_bindgroup)(struct ngpu_ctx *s, struct ngpu_bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets);
    void (*draw)(struct ngpu_ctx *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex);
    void (*draw_indexed)(struct ngpu_ctx *s, uint32_t nb_indices, uint32_t nb_instances);
    void (*dispatch)(struct ngpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);

    struct ngpu_buffer *(*buffer_create)(struct ngpu_ctx *ctx);
    int (*buffer_init)(struct ngpu_buffer *s);
    int (*buffer_wait)(struct ngpu_buffer *s);
    int (*buffer_upload)(struct ngpu_buffer *s, const void *data, size_t offset, size_t size);
    int (*buffer_map)(struct ngpu_buffer *s, size_t offset, size_t size, void **datap);
    void (*buffer_unmap)(struct ngpu_buffer *s);
    void (*buffer_freep)(struct ngpu_buffer **sp);

    struct ngpu_bindgroup_layout *(*bindgroup_layout_create)(struct ngpu_ctx *gpu_ctx);
    int (*bindgroup_layout_init)(struct ngpu_bindgroup_layout *s);
    void (*bindgroup_layout_freep)(struct ngpu_bindgroup_layout **sp);

    struct ngpu_bindgroup *(*bindgroup_create)(struct ngpu_ctx *gpu_ctx);
    int (*bindgroup_init)(struct ngpu_bindgroup *s, const struct ngpu_bindgroup_params *params);
    int (*bindgroup_update_texture)(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_texture_binding *binding);
    int (*bindgroup_update_buffer)(struct ngpu_bindgroup *s, int32_t index, const struct ngpu_buffer_binding *binding);
    void (*bindgroup_freep)(struct ngpu_bindgroup **sp);

    struct ngpu_pipeline *(*pipeline_create)(struct ngpu_ctx *ctx);
    int (*pipeline_init)(struct ngpu_pipeline *s);
    void (*pipeline_freep)(struct ngpu_pipeline **sp);

    struct ngpu_program *(*program_create)(struct ngpu_ctx *ctx);
    int (*program_init)(struct ngpu_program *s, const struct ngpu_program_params *params);
    void (*program_freep)(struct ngpu_program **sp);

    struct ngpu_rendertarget *(*rendertarget_create)(struct ngpu_ctx *ctx);
    int (*rendertarget_init)(struct ngpu_rendertarget *s);
    void (*rendertarget_freep)(struct ngpu_rendertarget **sp);

    struct ngpu_texture *(*texture_create)(struct ngpu_ctx *ctx);
    int (*texture_init)(struct ngpu_texture *s, const struct ngpu_texture_params *params);
    int (*texture_upload)(struct ngpu_texture *s, const uint8_t *data, int linesize);
    int (*texture_upload_with_params)(struct ngpu_texture *s, const uint8_t *data, const struct ngpu_texture_transfer_params *transfer_params);
    int (*texture_generate_mipmap)(struct ngpu_texture *s);
    void (*texture_freep)(struct ngpu_texture **sp);
};

struct ngpu_ctx {
    struct ngl_config config;
    const struct ngpu_ctx_class *cls;

    int version;
    int language_version;
    uint64_t features;
    struct ngpu_limits limits;

    uint32_t nb_in_flight_frames;
    uint32_t current_frame_index;

#if DEBUG_GPU_CAPTURE
    struct ngpu_capture_ctx *gpu_capture_ctx;
    int gpu_capture;
#endif

    /* State */
    struct ngpu_rendertarget *rendertarget;
    struct ngpu_pipeline *pipeline;
    struct ngpu_bindgroup *bindgroup;
    uint32_t dynamic_offsets[NGPU_MAX_DYNAMIC_OFFSETS];
    size_t nb_dynamic_offsets;
    const struct ngpu_buffer *vertex_buffers[NGPU_MAX_VERTEX_BUFFERS];
    const struct ngpu_buffer *index_buffer;
    enum ngpu_format index_format;
};

struct ngpu_ctx *ngpu_ctx_create(const struct ngl_config *config);
int ngpu_ctx_init(struct ngpu_ctx *s);
int ngpu_ctx_resize(struct ngpu_ctx *s, int32_t width, int32_t height);
int ngpu_ctx_set_capture_buffer(struct ngpu_ctx *s, void *capture_buffer);
uint32_t ngpu_ctx_advance_frame(struct ngpu_ctx *s);
uint32_t ngpu_ctx_get_current_frame_index(struct ngpu_ctx *s);
uint32_t ngpu_ctx_get_nb_in_flight_frames(struct ngpu_ctx *s);
int ngpu_ctx_begin_update(struct ngpu_ctx *s);
int ngpu_ctx_end_update(struct ngpu_ctx *s);
int ngpu_ctx_begin_draw(struct ngpu_ctx *s);
int ngpu_ctx_end_draw(struct ngpu_ctx *s, double t);
int ngpu_ctx_query_draw_time(struct ngpu_ctx *s, int64_t *time);
void ngpu_ctx_wait_idle(struct ngpu_ctx *s);
void ngpu_ctx_freep(struct ngpu_ctx **sp);

int ngpu_ctx_transform_cull_mode(struct ngpu_ctx *s, int cull_mode);
void ngpu_ctx_transform_projection_matrix(struct ngpu_ctx *s, float *dst);
void ngpu_ctx_get_rendertarget_uvcoord_matrix(struct ngpu_ctx *s, float *dst);

struct ngpu_rendertarget *ngpu_ctx_get_default_rendertarget(struct ngpu_ctx *s, enum ngpu_load_op load_op);
const struct ngpu_rendertarget_layout *ngpu_ctx_get_default_rendertarget_layout(struct ngpu_ctx *s);
void ngpu_ctx_get_default_rendertarget_size(struct ngpu_ctx *s, int32_t *width, int32_t *height);

void ngpu_ctx_begin_render_pass(struct ngpu_ctx *s, struct ngpu_rendertarget *rt);
void ngpu_ctx_end_render_pass(struct ngpu_ctx *s);

void ngpu_ctx_set_viewport(struct ngpu_ctx *s, const struct ngpu_viewport *viewport);
void ngpu_ctx_set_scissor(struct ngpu_ctx *s, const struct ngpu_scissor *scissor);

enum ngpu_format ngpu_ctx_get_preferred_depth_format(struct ngpu_ctx *s);
enum ngpu_format ngpu_ctx_get_preferred_depth_stencil_format(struct ngpu_ctx *s);
uint32_t ngpu_ctx_get_format_features(struct ngpu_ctx *s, enum ngpu_format format);

void ngpu_ctx_generate_texture_mipmap(struct ngpu_ctx *s, struct ngpu_texture *texture);

void ngpu_ctx_set_pipeline(struct ngpu_ctx *s, struct ngpu_pipeline *pipeline);
void ngpu_ctx_set_bindgroup(struct ngpu_ctx *s, struct ngpu_bindgroup *bindgroup, const uint32_t *offsets, size_t nb_offsets);
void ngpu_ctx_draw(struct ngpu_ctx *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex);
void ngpu_ctx_draw_indexed(struct ngpu_ctx *s, uint32_t nb_indices, uint32_t nb_instances);
void ngpu_ctx_dispatch(struct ngpu_ctx *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z);

void ngpu_ctx_set_vertex_buffer(struct ngpu_ctx *s, uint32_t index, const struct ngpu_buffer *buffer);
void ngpu_ctx_set_index_buffer(struct ngpu_ctx *s, const struct ngpu_buffer *buffer, enum ngpu_format format);

#endif
