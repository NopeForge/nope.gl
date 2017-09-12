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

#ifdef TARGET_ANDROID
#include "android_surface.h"
#endif

#ifdef TARGET_IPHONE
#include <CoreVideo/CoreVideo.h>
#endif

#include "glincludes.h"
#include "glcontext.h"
#include "params.h"

struct node_class;

enum {
    STATE_UNINITIALIZED = 0, /* post uninit(), default */
    STATE_INITIALIZED   = 1, /* post init() */
    STATE_READY         = 2, /* post prefetch() */
    STATE_IDLE          = 3, /* post release() */
};

struct ngl_ctx {
    struct glcontext *glcontext;
    struct ngl_node *scene;

    struct ngl_node **glstates;
    int nb_glstates;
};

struct ngl_node {
    const struct node_class *class;
    struct ngl_ctx *ctx;

    int refcount;
    NGLI_ALIGNED_MAT(modelview_matrix);
    NGLI_ALIGNED_MAT(projection_matrix);
    int state;

    double last_update_time;
    int drawme;

    struct ngl_node **glstates;
    int nb_glstates;

    struct ngl_node **ranges;
    int nb_ranges;
    int current_range;

    int is_active;
    double active_time;

    char *name;

    void *priv_data;
};

#define TRANSFORM_TYPES_LIST (const int[]){NGL_NODE_ROTATE,    \
                                           NGL_NODE_TRANSLATE, \
                                           NGL_NODE_SCALE,     \
                                           -1}

struct glstate {
    GLenum capability;
    int enabled[2];

    /* Blending */
    GLenum src_rgb[2];
    GLenum dst_rgb[2];
    GLenum src_alpha[2];
    GLenum dst_alpha[2];

    GLenum mode_rgb[2];
    GLenum mode_alpha[2];

    /* Color */
    GLint rgba[2][4];

    /* Stencil */
    GLuint writemask[2];

    GLenum func[2];
    GLint  func_ref[2];
    GLuint func_mask[2];

    GLenum op_sfail[2];
    GLenum op_dpfail[2];
    GLenum op_dppass[2];
};

struct camera {
    struct ngl_node *child;
    float eye[3];
    float center[3];
    float up[3];
    float perspective[4];

    struct ngl_node *eye_transform;
    struct ngl_node *center_transform;
    struct ngl_node *up_transform;

    struct ngl_node **fov_animkf;
    int nb_fov_animkf;
    int current_fov_kf;

    int pipe_fd;
    int pipe_width, pipe_height;
    uint8_t *pipe_buf;

    GLuint framebuffer_id;
    GLuint texture_id;
};

struct shapeprimitive {
    float coordinates[3];
    float texture_coordinates[2];
    float normals[3];
};

#define NGLI_SHAPE_COORDS_NB        3
#define NGLI_SHAPE_TEXCOORDS_NB     2
#define NGLI_SHAPE_NORMALS_NB       3

#define NGLI_SHAPE_TEXCOORDS_OFFSET 3
#define NGLI_SHAPE_NORMALS_OFFSET   5

#define NGLI_SHAPE_VERTICES_STRIDE(s) ((NGLI_SHAPE_COORDS_NB + NGLI_SHAPE_TEXCOORDS_NB + NGLI_SHAPE_NORMALS_NB) * sizeof(*(s)->vertices))
#define NGLI_SHAPE_VERTICES_SIZE(s) ((s)->nb_vertices * NGLI_SHAPE_VERTICES_STRIDE(s))

struct shape {
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

    /* shape params */
    struct ngl_node **primitives;
    int nb_primitives;

    GLfloat *vertices;
    int nb_vertices;
    GLuint vertices_buffer_id;
    GLuint texcoords_buffer_id;
    GLuint normals_buffer_id;

    GLushort *indices;
    int nb_indices;
    GLuint indices_buffer_id;

    GLenum draw_mode;
    GLenum draw_type;
};

void ngli_shape_generate_buffers(struct ngl_node *node);

struct uniform {
    double scalar;
    float vector[4];
    float matrix[4*4];
    int ival;
    int type;
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;
    struct ngl_node *transform;
};

struct attribute {
    const char *name;
};

struct rtt {
    struct ngl_node *child;
    struct ngl_node *color_texture;
    struct ngl_node *depth_texture;
    int width;
    int height;
    GLuint framebuffer_id;
    GLuint renderbuffer_id;
};

struct shader {
    const char *vertex_data;
    const char *fragment_data;

    GLuint program_id;
    GLint position_location_id;
    GLint normal_location_id;
    GLint modelview_matrix_location_id;
    GLint projection_matrix_location_id;
    GLint normal_matrix_location_id;
};

struct texture {
    GLenum target;
    GLint format;
    GLint internal_format;
    GLint type;
    int width;
    int height;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
    struct ngl_node *data_src;
    GLuint external_id;

    NGLI_ALIGNED_MAT(coordinates_matrix);
    GLuint id;
    GLuint local_id;
    GLenum local_target;

    int upload_fmt;
    struct ngl_node *quad;
    struct ngl_node *shader;
    struct ngl_node *tshape;
    struct ngl_node *textures[3];
    struct ngl_node *target_texture;
    struct ngl_node *rtt;

#ifdef TARGET_IPHONE
    CVOpenGLESTextureRef texture;
#endif
};

struct textureshaderinfo {
    int sampler_id;
    int coordinates_id;
    int coordinates_mvp_id;
    int dimensions_id;
};

struct texturedshape {
    struct ngl_node *shape;
    struct ngl_node *shader;

    struct ndict *textures;
    struct textureshaderinfo *textureshaderinfos;

    struct ndict *uniforms;
    GLint *uniform_ids;

    struct ndict *attributes;
    GLint *attribute_ids;

    GLuint vao_id;
};

struct media {
    const char *filename;
    double start;
    double initial_seek;
    const char *sxplayer_min_level_str;
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;
    int audio_tex;

    int sxplayer_min_level;

    struct sxplayer_ctx *player;
    struct sxplayer_frame *frame;

#ifdef TARGET_ANDROID
    GLuint android_texture_id;
    GLenum android_texture_target;
    struct android_surface *android_surface;
#endif
};

struct renderrange {
    double start_time;
    double render_time;
    int updated;
};

struct rotate {
    struct ngl_node *child;
    double angle;
    float axis[3];
    float anchor[3];
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;
};

struct translate {
    struct ngl_node *child;
    float vector[3];
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;
};

struct scale {
    struct ngl_node *child;
    float factors[3];
    float anchor[3];
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;
};

typedef double easing_type;
typedef easing_type (*easing_function)(easing_type, int, const easing_type *);

void ngli_animkf_interpolate(float *dst, struct ngl_node **animkf, int nb_animkf,
                             int *current_kf, double t);

struct animkeyframe {
    double time;
    float value[4];
    double scalar;
    const char *easing;
    easing_function function;
    easing_function resolution;
    double *args;
    int nb_args;
};

struct fps {
    struct ngl_node *child;
    int measure_update;
    int measure_draw;
    int create_databuf;
    int64_t update_start;
    int64_t update_end;
    uint8_t *data_buf;
    int data_w, data_h;
};

/**
 *   Operation        State result
 * -----------------------------------
 * I Init           STATE_INITIALIZED
 * P Prefetch       STATE_READY
 * D Update/Draw
 * R Release        STATE_IDLE
 * U Uninit         STATE_INITIALIZED
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
    void (*prefetch)(struct ngl_node *node);
    void (*update)(struct ngl_node *node, double t);
    void (*draw)(struct ngl_node *node);
    void (*release)(struct ngl_node *node);
    void (*uninit)(struct ngl_node *node);
    char *(*info_str)(const struct ngl_node *node);
    size_t priv_size;
    const struct node_param *params;
};

void ngli_node_print_specs(void);

int ngli_node_init(struct ngl_node *node);
void ngli_node_prefetch(struct ngl_node *node);
void ngli_node_check_resources(struct ngl_node *node, double t);
void ngli_node_update(struct ngl_node *node, double t);
void ngli_node_draw(struct ngl_node *node);
void ngli_node_release(struct ngl_node *node);

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);
void ngli_node_detach_ctx(struct ngl_node *node);

char *ngli_node_default_name(const char *class_name);
struct ngl_node *ngli_node_create_noconstructor(int type);
const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp);


void ngli_honor_glstates(struct ngl_ctx *ctx, int nb_glstates, struct ngl_node **glstates);
void ngli_restore_glstates(struct ngl_ctx *ctx, int nb_glstates, struct ngl_node **glstates);

#endif
