/*
 * Copyright 2016 GoPro Inc.
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

#ifndef NODES_H
#define NODES_H

#include <stdlib.h>
#include <sxplayer.h>
#include <pthread.h>

#if defined(HAVE_VAAPI_X11)
#include <X11/Xlib.h>
#include <va/va_x11.h>
#endif

#if defined(TARGET_ANDROID)
#include "android_handlerthread.h"
#include "android_surface.h"
#endif

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "animation.h"
#include "drawutils.h"
#include "glincludes.h"
#include "glcontext.h"
#include "glstate.h"
#include "hmap.h"
#include "image.h"
#include "nodegl.h"
#include "params.h"
#include "darray.h"
#include "buffer.h"
#include "format.h"
#include "fbo.h"
#include "texture.h"

struct node_class;

typedef int (*cmd_func_type)(struct ngl_ctx *s, void *arg);

typedef void (*capture_func_type)(struct ngl_ctx *s);

struct ngl_ctx {
    /* Controller-only fields */
    const struct backend *backend;
    int configured;
    pthread_t worker_tid;

    /* Worker-only fields */
    struct glcontext *glcontext;
    struct glstate current_glstate;
    struct glstate pending_glstate;
    struct ngl_node *scene;
    struct ngl_config config;
    int timer_active;
    struct darray modelview_matrix_stack;
    struct darray projection_matrix_stack;
    struct darray activitycheck_nodes;
#if defined(HAVE_VAAPI_X11)
    Display *x11_display;
    VADisplay va_display;
    int va_version;
#endif
    /* Offscreen framebuffer */
    struct fbo fbo;
    struct texture fbo_color;
    struct texture fbo_depth;
    /* Capture offscreen framebuffer */
    capture_func_type capture_func;
    struct fbo oes_resolve_fbo;
    struct texture oes_resolve_fbo_color;
    struct fbo capture_fbo;
    struct texture capture_fbo_color;
    uint8_t *capture_buffer;
#if defined(TARGET_IPHONE)
    CVPixelBufferRef capture_cvbuffer;
    CVOpenGLESTextureRef capture_cvtexture;
#endif

    /* Shared fields */
    pthread_mutex_t lock;
    pthread_cond_t cond_ctl;
    pthread_cond_t cond_wkr;
    cmd_func_type cmd_func;
    void *cmd_arg;
    int cmd_ret;
};

struct ngl_node {
    const struct node_class *class;
    struct ngl_ctx *ctx;

    int state;
    int is_active;

    double visit_time;
    double last_update_time;

    int draw_count;

    int refcount;
    int ctx_refcount;

    struct darray children;

    char *label;

    void *priv_data;
};

#define TRANSFORM_TYPES_LIST (const int[]){NGL_NODE_ROTATE,    \
                                           NGL_NODE_TRANSFORM, \
                                           NGL_NODE_TRANSLATE, \
                                           NGL_NODE_SCALE,     \
                                           NGL_NODE_IDENTITY,  \
                                           -1}

struct graphicconfig_priv {
    struct ngl_node *child;

    int blend;
    int blend_src_factor;
    int blend_dst_factor;
    int blend_src_factor_a;
    int blend_dst_factor_a;
    int blend_op;
    int blend_op_a;

    int color_write_mask;

    int depth_test;
    int depth_write_mask;
    int depth_func;

    int stencil_test;
    int stencil_write_mask;
    int stencil_func;
    int stencil_ref;
    int stencil_read_mask;
    int stencil_fail;
    int stencil_depth_fail;
    int stencil_depth_pass;

    int cull_face;
    int cull_face_mode;

    int scissor_test;
    float scissor[4];
    int use_scissor;

    struct glstate state;
};

struct camera_priv {
    struct ngl_node *child;
    float eye[3];
    float center[3];
    float up[3];
    float perspective[2];
    float orthographic[4];
    float clipping[2];

    struct ngl_node *eye_transform;
    struct ngl_node *center_transform;
    struct ngl_node *up_transform;

    struct ngl_node *fov_anim;

    int use_perspective;
    int use_orthographic;

    const float *eye_transform_matrix;
    const float *center_transform_matrix;
    const float *up_transform_matrix;

    float ground[3];

    NGLI_ALIGNED_MAT(modelview_matrix);
    NGLI_ALIGNED_MAT(projection_matrix);

    int pipe_fd;
    int pipe_width, pipe_height;
    uint8_t *pipe_buf;

    int samples;
    struct fbo fbo;
    struct texture fbo_color;
};

struct geometry_priv {
    /* quad params */
    float quad_corner[3];
    float quad_width[3];
    float quad_height[3];

    float quad_uv_corner[2];
    float quad_uv_width[2];
    float quad_uv_height[2];

    /* triangle params */
    float triangle_edges[9];
    float triangle_uvs[6];

    /* circle params */
    double radius;
    int npoints;

    /* geometry params */
    struct ngl_node *vertices_buffer;
    struct ngl_node *uvcoords_buffer;
    struct ngl_node *normals_buffer;
    struct ngl_node *indices_buffer;

    GLenum topology;
};

struct ngl_node *ngli_node_geometry_generate_buffer(struct ngl_ctx *ctx, int type, int count, int size, void *data);

struct buffer_priv {
    int count;              // number of elements
    uint8_t *data;          // buffer of <count> elements
    int data_size;          // total buffer data size in bytes
    char *filename;         // filename from which the data will be read
    int data_comp;          // number of components per element
    int data_stride;        // stride of 1 element, in bytes
    struct ngl_node *block;
    int block_field;
    int usage;              // flags defining buffer use
    int data_format;        // any of NGLI_FORMAT_*

    /* animatedbuffer */
    struct ngl_node **animkf;
    int nb_animkf;
    struct animation anim;

    int fd;
    int dynamic;

    struct buffer buffer;
    int buffer_refcount;
    double buffer_last_upload_time;
};

int ngli_node_buffer_ref(struct ngl_node *node);
void ngli_node_buffer_unref(struct ngl_node *node);
int ngli_node_buffer_upload(struct ngl_node *node);

struct uniform_priv {
    double scalar;
    float vector[4];
    float matrix[4*4];
    int ival;
    struct ngl_node *anim;
    struct ngl_node *transform;
    const float *transform_matrix;
    int dynamic;
    int live_changed;
};

struct block_field_info {
    int spec_id;
    int offset;
    int size;
    int stride;
};

struct block_priv {
    struct ngl_node **fields;
    int nb_fields;
    int layout;

    struct block_field_info *field_info;
    int nb_field_info;
    uint8_t *data;
    int data_size;
    int usage;

    struct buffer buffer;
    int buffer_refcount;
    int has_changed;
    double buffer_last_upload_time;
};

int ngli_node_block_ref(struct ngl_node *node);
void ngli_node_block_unref(struct ngl_node *node);
int ngli_node_block_upload(struct ngl_node *node);

struct rtt_priv {
    struct ngl_node *child;
    struct ngl_node **color_textures;
    int nb_color_textures;
    struct ngl_node *depth_texture;
    int samples;
    float clear_color[4];
    int features;
    int vflip;

    int use_clear_color;
    int width;
    int height;

    struct fbo fbo;
    struct texture fbo_depth;

    struct fbo fbo_ms;
    struct darray fbo_ms_colors;
    struct texture fbo_ms_depth;
};

struct program_priv {
    const char *vertex;
    const char *fragment;
    const char *compute;

    GLuint program_id;
    struct hmap *active_uniforms;
    struct hmap *active_attributes;
    struct hmap *active_buffer_blocks;
};

extern const struct param_choices ngli_mipmap_filter_choices;
extern const struct param_choices ngli_filter_choices;

struct texture_priv {
    struct texture_params params;
    struct ngl_node *data_src;
    int direct_rendering;

    struct texture texture;
    struct image image;

    const struct hwmap_class *hwupload_map_class;
    void *hwupload_priv_data;
};

struct uniformprograminfo {
    GLint location;
    GLint size;
    GLenum type;
    int binding;
};

struct attributeprograminfo {
    GLint location;
    GLint size;
    GLenum type;
};

struct blockprograminfo {
    GLint binding;
    GLenum type;
};

#define NGLI_SAMPLING_MODE_NONE         0
#define NGLI_SAMPLING_MODE_DEFAULT      1
#define NGLI_SAMPLING_MODE_EXTERNAL_OES 2
#define NGLI_SAMPLING_MODE_NV12         3

struct textureprograminfo {
    int sampling_mode_location;
    int sampler_value;
    int sampler_type;
    int sampler_location;
    int external_sampler_location;
    int y_sampler_location;
    int uv_sampler_location;
    int coord_matrix_location;
    int dimensions_location;
    int dimensions_type;
    int ts_location;
};

typedef void (*nodeprograminfopair_handle_func)(struct glcontext *gl, GLint loc, void *priv);

#define MAX_ID_LEN 128
struct nodeprograminfopair {
    char name[MAX_ID_LEN];
    struct ngl_node *node;
    void *program_info;
    nodeprograminfopair_handle_func handle;
};

struct pipeline_params {
    const char *label;
    struct ngl_node *program;
    struct hmap *textures;
    struct hmap *uniforms;
    struct hmap *blocks;
    int nb_instances;
    struct hmap *attributes;
    struct hmap *instance_attributes;
};

enum {
    NGLI_PIPELINE_TYPE_GRAPHIC,
    NGLI_PIPELINE_TYPE_COMPUTE,
};

struct pipeline {
    struct ngl_ctx *ctx;
    struct glcontext *gl;
    struct pipeline_params params;
    int type;

    struct textureprograminfo *textureprograminfos;
    int nb_textureprograminfos;
    struct darray texture_pairs; // nodeprograminfopair (texture, textureprograminfo)

    uint64_t used_texture_units;
    int disabled_texture_unit[2]; /* 2D, OES */

    struct darray uniform_pairs; // nodeprograminfopair (uniform, uniformprograminfo)
    struct darray block_pairs; // nodeprograminfopair (block, uniformprograminfo)

    struct darray attribute_pairs; // nodeprograminfopair (attribute, attributeprograminfo)
    struct darray instance_attribute_pairs; // nodeprograminfopair (instance attribute, attributeprograminfo)

    GLint modelview_matrix_location;
    GLint projection_matrix_location;
    GLint normal_matrix_location;

    GLuint vao_id;
};

struct render_priv {
    struct ngl_node *geometry;
    struct ngl_node *program;
    struct hmap *textures;
    struct hmap *uniforms;
    struct hmap *blocks;
    struct hmap *attributes;
    struct hmap *instance_attributes;
    int nb_instances;

    struct hmap *pipeline_attributes;
    struct pipeline pipeline;

    int has_indices_buffer_ref;
    GLenum indices_type;

    void (*draw)(struct glcontext *gl, struct render_priv *render);
};

struct compute_priv {
    int nb_group_x;
    int nb_group_y;
    int nb_group_z;
    struct ngl_node *program;
    struct hmap *textures;
    struct hmap *uniforms;
    struct hmap *blocks;

    struct pipeline pipeline;
};

int ngli_pipeline_init(struct pipeline *s, struct ngl_ctx *ctx, const struct pipeline_params *params);
void ngli_pipeline_uninit(struct pipeline *s);
int ngli_pipeline_update(struct pipeline *s, double t);
int ngli_pipeline_bind(struct pipeline *s);
int ngli_pipeline_unbind(struct pipeline *s);

struct media_priv {
    const char *filename;
    int sxplayer_min_level;
    struct ngl_node *anim;
    int audio_tex;
    int max_nb_packets;
    int max_nb_frames;
    int max_nb_sink;
    int max_pixels;
    int stream_idx;

    struct sxplayer_ctx *player;
    struct sxplayer_frame *frame;

#if defined(TARGET_ANDROID)
    struct texture android_texture;
    struct android_surface *android_surface;
    struct android_handlerthread *android_handlerthread;
#endif
};

struct timerangemode_priv {
    double start_time;
    double render_time;
    int updated;
};

struct transform_priv {
    struct ngl_node *child;
    NGLI_ALIGNED_MAT(matrix);
};

struct rotate_priv {
    struct transform_priv trf;
    double angle;
    float axis[3];
    float normed_axis[3];
    float anchor[3];
    struct ngl_node *anim;
    int use_anchor;
};

struct translate_priv {
    struct transform_priv trf;
    float vector[3];
    struct ngl_node *anim;
};

struct scale_priv {
    struct transform_priv trf;
    float factors[3];
    float anchor[3];
    struct ngl_node *anim;
    int use_anchor;
};

struct identity {
    NGLI_ALIGNED_MAT(modelview_matrix);
};

enum easing_id {
    EASING_LINEAR,
    EASING_QUADRATIC_IN,
    EASING_QUADRATIC_OUT,
    EASING_QUADRATIC_IN_OUT,
    EASING_QUADRATIC_OUT_IN,
    EASING_CUBIC_IN,
    EASING_CUBIC_OUT,
    EASING_CUBIC_IN_OUT,
    EASING_CUBIC_OUT_IN,
    EASING_QUARTIC_IN,
    EASING_QUARTIC_OUT,
    EASING_QUARTIC_IN_OUT,
    EASING_QUARTIC_OUT_IN,
    EASING_QUINTIC_IN,
    EASING_QUINTIC_OUT,
    EASING_QUINTIC_IN_OUT,
    EASING_QUINTIC_OUT_IN,
    EASING_POWER_IN,
    EASING_POWER_OUT,
    EASING_POWER_IN_OUT,
    EASING_POWER_OUT_IN,
    EASING_SINUS_IN,
    EASING_SINUS_OUT,
    EASING_SINUS_IN_OUT,
    EASING_SINUS_OUT_IN,
    EASING_EXP_IN,
    EASING_EXP_OUT,
    EASING_EXP_IN_OUT,
    EASING_EXP_OUT_IN,
    EASING_CIRCULAR_IN,
    EASING_CIRCULAR_OUT,
    EASING_CIRCULAR_IN_OUT,
    EASING_CIRCULAR_OUT_IN,
    EASING_BOUNCE_IN,
    EASING_BOUNCE_OUT,
    EASING_ELASTIC_IN,
    EASING_ELASTIC_OUT,
    EASING_BACK_IN,
    EASING_BACK_OUT,
    EASING_BACK_IN_OUT,
    EASING_BACK_OUT_IN,
};

typedef double easing_type;
typedef easing_type (*easing_function)(easing_type, int, const easing_type *);

struct animation_priv {
    struct ngl_node **animkf;
    int nb_animkf;
    struct animation anim;
    struct animation anim_eval;
    float values[4];
    double scalar;
};

struct animkeyframe_priv {
    double time;
    float value[4];
    double scalar;
    uint8_t *data;
    int data_size;
    int easing;
    easing_function function;
    easing_function resolution;
    double *args;
    int nb_args;
    double offsets[2];
    int scale_boundaries;
    double boundaries[2];
};

struct hud_priv {
    struct ngl_node *child;
    int measure_window;
    int refresh_rate[2];
    char *export_filename;
    float bg_color[4];
    int aspect_ratio[2];

    struct darray widgets;
    uint32_t bg_color_u32;
    int fd_export;
    struct bstr *csv_line;
    struct canvas canvas;
    double refresh_rate_interval;
    double last_refresh_time;
    int need_refresh;
};

/**
 *   Operation        State result
 * -----------------------------------
 * I Init           STATE_INITIALIZED
 * P Prefetch       STATE_READY
 * D Update/Draw
 * R Release        STATE_INITIALIZED
 * U Uninit         STATE_UNINITIALIZED
 *
 * Dependency callgraph:
 *
 *             .------------.
 *             v            |
 *     (I) <- [P] <- [D]   (R) <- [U]
 *      |                          ^
 *      `--------------------------'
 *
 * The starting state is [U].
 *
 * .--[ Legend ]-------
 * |
 * | X:      Operation X
 * | X -> Y: X call depends on Y state result
 * | [X]:    if X dependency is not met, the change state call will be made
 * | (X):    if X dependency is not met, it will noop
 * |
 * `-------------------
 *
 * Some examples:
 *  - calling prefetch() will always call init() if necessary
 *  - release() has a weak dependency to prefetch(), so it will noop if not in
 *    the READY state.
 *
 * Note: nodes implementation do NOT have to implement this logic, but they can
 * rely on these properties in their callback implementations.
 */
struct node_class {
    int id;
    const char *name;
    int (*init)(struct ngl_node *node);
    int (*visit)(struct ngl_node *node, int is_active, double t);
    int (*prefetch)(struct ngl_node *node);
    int (*update)(struct ngl_node *node, double t);
    void (*draw)(struct ngl_node *node);
    void (*release)(struct ngl_node *node);
    void (*uninit)(struct ngl_node *node);
    char *(*info_str)(const struct ngl_node *node);
    size_t priv_size;
    const struct node_param *params;
    const char *params_id;
    const char *file;
};

void ngli_node_print_specs(void);

int ngli_node_visit(struct ngl_node *node, int is_active, double t);
int ngli_node_honor_release_prefetch(struct darray *nodes_array);
int ngli_node_update(struct ngl_node *node, double t);
int ngli_prepare_draw(struct ngl_ctx *s, double t);
void ngli_node_draw(struct ngl_node *node);

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);
void ngli_node_detach_ctx(struct ngl_node *node);

char *ngli_node_default_label(const char *class_name);
int ngli_is_default_label(const char *class_name, const char *str);
struct ngl_node *ngli_node_create_noconstructor(int type);
const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp);

#endif
