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

#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdlib.h>
#include <sxplayer.h>

#include "config.h"

#if defined(HAVE_VAAPI)
#include "vaapi_ctx.h"
#endif

#if defined(TARGET_ANDROID)
#include "android_ctx.h"
#include "android_handlerthread.h"
#include "android_surface.h"
#include "android_imagereader.h"
#endif

#include "animation.h"
#include "block.h"
#include "drawutils.h"
#include "graphicstate.h"
#include "hmap.h"
#include "hud.h"
#include "hwconv.h"
#include "hwmap.h"
#include "image.h"
#include "nodegl.h"
#include "params.h"
#include "pgcache.h"
#include "program.h"
#include "pthread_compat.h"
#include "darray.h"
#include "buffer.h"
#include "format.h"
#include "rendertarget.h"
#include "rnode.h"
#include "texture.h"

struct node_class;

typedef int (*cmd_func_type)(struct ngl_ctx *s, void *arg);

struct api_impl {
    int (*configure)(struct ngl_ctx *s, const struct ngl_config *config);
    int (*resize)(struct ngl_ctx *s, int width, int height, const int *viewport);
    int (*set_capture_buffer)(struct ngl_ctx *s, void *capture_buffer);
    int (*set_scene)(struct ngl_ctx *s, struct ngl_node *scene);
    int (*prepare_draw)(struct ngl_ctx *s, double t);
    int (*draw)(struct ngl_ctx *s, double t);
    void (*reset)(struct ngl_ctx *s, int action);

    /* OpenGL */
    int (*gl_wrap_framebuffer)(struct ngl_ctx *s, uint32_t framebuffer);
};

struct ngl_ctx {
    /* Controller-only fields */
    int configured;
    pthread_t worker_tid;
    const struct api_impl *api_impl;

    /* Worker-only fields */
    struct gpu_ctx *gpu_ctx;
    struct rnode rnode;
    struct rnode *rnode_pos;
    struct ngl_node *scene;
    struct ngl_config config;
    struct rendertarget *available_rendertargets[2];
    struct rendertarget *current_rendertarget;
    int render_pass_started;
    struct darray modelview_matrix_stack;
    struct darray projection_matrix_stack;

    /*
     * Array of nodes that are candidate to either prefetch (active) or release
     * (non-active). Nodes are inserted from bottom (leaves) up to the top
     * (root).
     */
    struct darray activitycheck_nodes;

    struct texture *font_atlas;
    struct pgcache pgcache;
#if defined(HAVE_VAAPI)
    struct vaapi_ctx vaapi_ctx;
#endif
#if defined(TARGET_ANDROID)
    struct android_ctx android_ctx;
#endif
    struct hud *hud;
    int64_t cpu_update_time;
    int64_t cpu_draw_time;
    int64_t gpu_draw_time;

    /* Shared fields */
    pthread_mutex_t lock;
    pthread_cond_t cond_ctl;
    pthread_cond_t cond_wkr;
    cmd_func_type cmd_func;
    void *cmd_arg;
    int cmd_ret;
};

#define NGLI_ACTION_KEEP_SCENE  0
#define NGLI_ACTION_UNREF_SCENE 1

int ngli_ctx_dispatch_cmd(struct ngl_ctx *s, cmd_func_type cmd_func, void *arg);
int ngli_ctx_configure(struct ngl_ctx *s, const struct ngl_config *config);
int ngli_ctx_resize(struct ngl_ctx *s, int width, int height, const int *viewport);
int ngli_ctx_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer);
int ngli_ctx_set_scene(struct ngl_ctx *s, struct ngl_node *node);
int ngli_ctx_prepare_draw(struct ngl_ctx *s, double t);
int ngli_ctx_draw(struct ngl_ctx *s, double t);
void ngli_ctx_reset(struct ngl_ctx *s, int action);

struct ngl_node {
    const struct node_class *cls;
    struct ngl_ctx *ctx;

    void *opts;

    int state;
    int is_active;

    double visit_time;
    double last_update_time;

    int draw_count;

    int refcount;
    int ctx_refcount;

    struct darray children;
    struct darray parents;

    char *label;

    void *priv_data;
};

#define TRANSFORM_TYPES_LIST (const int[]){NGL_NODE_ROTATE,    \
                                           NGL_NODE_ROTATEQUAT,\
                                           NGL_NODE_TRANSFORM, \
                                           NGL_NODE_TRANSLATE, \
                                           NGL_NODE_SCALE,     \
                                           NGL_NODE_SKEW,      \
                                           NGL_NODE_IDENTITY,  \
                                           -1}

/* helper structure to specify the content (or a slice) of a buffer */
struct buffer_layout {
    int type;       // any of NGLI_TYPE_*
    int format;     // any of NGLI_FORMAT_*
    int stride;     // stride of 1 element, in bytes
    int comp;       // number of components per element
    int count;      // number of elements
    int offset;     // offset where the data starts in the buffer, in bytes
};

struct buffer_info {
    struct buffer_layout layout;

    uint8_t *data;          // buffer of <count> elements
    int data_size;          // total buffer data size in bytes

    struct ngl_node *block;
    int block_field;
    int usage;              // flags defining buffer use

    int dynamic;

    struct buffer *buffer;
    int buffer_refcount;
    double buffer_last_upload_time;
};

int ngli_node_buffer_ref(struct ngl_node *node);
void ngli_node_buffer_extend_usage(struct ngl_node *node, int usage);
int ngli_node_buffer_init(struct ngl_node *node);
void ngli_node_buffer_unref(struct ngl_node *node);
int ngli_node_buffer_upload(struct ngl_node *node);
int ngli_node_buffer_get_cpu_size(struct ngl_node *node);
int ngli_node_buffer_get_gpu_size(struct ngl_node *node);

struct livectl {
    union ngl_livectl_data val;
    char *id;
    union ngl_livectl_data min;
    union ngl_livectl_data max;
};

struct variable_opts {
    struct livectl live;

    struct ngl_node **animkf;
    int nb_animkf;

    union {
        struct ngl_node *path_node; /* AnimatedPath only */
        struct ngl_node *transform; /* UniformMat4 only */
        int as_mat4; /* UniformQuat and AnimatedQuat only */
        int space; /* UniformColor and AnimatedColor only */
    };
};

struct variable_info {
    void *data;
    int data_size;
    int data_type;          // any of NGLI_TYPE_*
    int dynamic;
};

int ngli_velocity_evaluate(struct ngl_node *node, void *dst, double t);

struct block_opts {
    struct ngl_node **fields;
    int nb_fields;
    int layout;
};

struct block_priv {
    struct block block;
    int force_update;

    uint8_t *data;
    int data_size;
    int usage;

    struct buffer *buffer;
    int buffer_refcount;
    int has_changed;
    double buffer_last_upload_time;
};

int ngli_node_block_ref(struct ngl_node *node);
void ngli_node_block_extend_usage(struct ngl_node *node, int usage);
int ngli_node_block_init(struct ngl_node *node);
void ngli_node_block_unref(struct ngl_node *node);
int ngli_node_block_upload(struct ngl_node *node);
int ngli_node_block_get_cpu_size(struct ngl_node *node);
int ngli_node_block_get_gpu_size(struct ngl_node *node);

struct program_opts {
    const char *vertex;
    const char *fragment;
    const char *compute;
    int workgroup_size[3];
    struct hmap *properties;
    struct hmap *vert_out_vars;
    int nb_frag_output;
};

struct program_priv {
    struct darray vert_out_vars_array; // pgcraft_iovar
};

extern const struct param_choices ngli_mipmap_filter_choices;
extern const struct param_choices ngli_filter_choices;

struct texture_opts {
    int requested_format;
    struct texture_params params;
    struct ngl_node *data_src;
    int direct_rendering;
    int clamp_video;
};

struct texture_priv {
    struct texture_params params;
    uint32_t supported_image_layouts;
    struct texture *texture;
    struct image image;
    struct hwmap hwmap;
};

struct media_priv {
    struct sxplayer_ctx *player;
    struct sxplayer_frame *frame;
    int nb_parents;

#if defined(TARGET_ANDROID)
    struct android_surface *android_surface;
    struct android_handlerthread *android_handlerthread;
    struct android_imagereader *android_imagereader;
#endif
};

struct timerangemode_opts {
    double start_time;
    double render_time;
};

struct timerangemode_priv {
    int updated;
};

struct transform {
    struct ngl_node *child;
    NGLI_ALIGNED_MAT(matrix);
};

struct io_opts {
    int precision_out;
    int precision_in;
};

struct io_priv {
    int type;
};

struct resourceprops_opts {
    int precision;
    int as_image;
    int writable;
    int variadic;
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

struct animkeyframe_opts {
    double time;
    float value[4];
    double scalar;
    uint8_t *data;
    int data_size;
    int easing;
    double *args;
    int nb_args;
    double offsets[2];
};

struct animkeyframe_priv {
    easing_function function;
    easing_function derivative;
    easing_function resolution;
    int scale_boundaries;
    double boundaries[2];
    double derivative_scale;
};

struct pathkey_move_opts {
    float to[3];
};

struct pathkey_line_opts {
    float to[3];
};

struct pathkey_bezier2_opts {
    float control[3];
    float to[3];
};

struct pathkey_bezier3_opts {
    float control1[3];
    float control2[3];
    float to[3];
};

struct textureview_opts {
    struct ngl_node *texture;
    int layer;
};

struct textureview_priv {
    struct textureview_opts opts;
};

enum {
    NGLI_NODE_CATEGORY_NONE,
    NGLI_NODE_CATEGORY_VARIABLE,
    NGLI_NODE_CATEGORY_TEXTURE,
    NGLI_NODE_CATEGORY_BUFFER,
    NGLI_NODE_CATEGORY_BLOCK,
    NGLI_NODE_CATEGORY_IO,
    NGLI_NODE_CATEGORY_RENDER, /* node executes a graphics pipeline */
};

/*
 * Node is an exposed live control.
 *
 * A few important notes when setting this flag:
 *
 * - the private node context must contain a livectl struct, and
 *   node_class.livectl_offset must point to it (we can not have any static
 *   check for this because 0 is a valid offset)
 * - an option named "live_id" must be exposed in the parameters (and
 *   associated with `livectl.id`)
 * - the value parameter can have any arbitrary name but must be present before
 *   "live_id", point to `livectl.val`, and has to be the first parameter
 *   flagged with NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE
 */
#define NGLI_NODE_FLAG_LIVECTL (1 << 0)

/*
 * Specifications of a node.
 *
 * Description of the callbacks attributes:
 *
 * - reentrant:
 *    + yes: callback will be called multiple time in diamond shaped tree
 *    + no: callback will be called only once in a diamond shaped tree
 * - execution order:
 *    + leaf/children first: callbacks are called in ascent order
 *    + root/parents first: callbacks are called in descent order
 *    + loose: each node decides (implies a manual dispatch)
 * - dispatch:
 *    + manual: the callback takes over / decides the dispatch to the children
 *    + managed: internals (nodes.c) are responsible for running the descent
 *               into children (meaning it controls ascent/descent order)
 *    + delegated: alias for "manual + managed", meaning managed by default
 *                 unless the callback is defined which take over the default
 *                 behavior
 */
struct node_class {
    int id;
    int category;
    const char *name;


    /************************
     * Init stage callbacks *
     ************************/

    /*
     * Initialize the node private context.
     *
     * reentrant: no (comparing state against STATE_INITIALIZED)
     * execution-order: leaf first
     * dispatch: managed
     * when: called during set_scene() / internal node_set_ctx()
     */
    int (*init)(struct ngl_node *node);

    /*
     * Handle render paths (for diamond shape in particular)
     *
     * If the node splits the tree in branches (such as Group) that can end up
     * with a render-based node in the leaves, it must create a new
     * rnode per branch and forward the call in each branch.
     *
     * If the node is a pipeline based node, it has to configure in the
     * callback each pipeline using ctx->rnode_pos.
     *
     * reentrant: yes (there is a different rnode per path)
     * execution-order: loose
     * dispatch: delegated
     * when: called during set_scene() / internal node_set_ctx() (after init)
     */
    int (*prepare)(struct ngl_node *node);


    /*******************************
     * Draw/update stage callbacks *
     *******************************/

    /*
     * Allow a node to stop the descent into its children by optionally
     * changing is_active and forwarding the call to the children.
     *
     * The callback MUST forward the call, even if the purpose is to disable
     * the branch.
     *
     * reentrant: yes (potentially with a different is_active flag)
     * execution-order: root first
     * dispatch: delegated
     * when: first step during an api draw call
     */
    int (*visit)(struct ngl_node *node, int is_active, double t);

    /*
     * Pre-allocate resources or start background processing so that they are
     * ready at update time (typically sxplayer). Contrary to allocations done
     * in the init, the prefetched resources lifetime is reduced to active
     * timeranges.
     *
     * The symmetrical callback for prefetch is the release callback.
     *
     * reentrant: no (comparing state against STATE_READY)
     * execution-order: leaf first
     * dispatch: managed
     * when: follows the visit phase, as part of
     *       ngli_node_honor_release_prefetch() (comes after the release)
     */
    int (*prefetch)(struct ngl_node *node);

    /*
     * Reset node update time (and other potential state used in the update) to
     * force an update during the next api draw call.
     *
     * reentrant: yes
     * execution-order: leaf first
     * dispatch: managed
     * when: any time a parameter is live-changed
     */
    int (*invalidate)(struct ngl_node *node);

    /*
     * Update CPU/GPU resources according to the time.
     *
     * reentrant: no (based on node last_update_time)
     * execution-order: loose
     * dispatch: manual
     * when: straight after ngli_node_honor_release_prefetch()
     */
    int (*update)(struct ngl_node *node, double t);

    /*
     * Apply transforms and execute graphics and compute pipelines.
     *
     * reentrant: yes (because the leaf of a diamond tree must be drawn for
     *            each path)
     * execution-order: loose
     * dispatch: manual
     * when: after scene has been update for a given time (which can be a no-op
     *       since it's non reentrant)
     */
    void (*draw)(struct ngl_node *node);

    /*
     * Must release resources (allocated during the prefetch phase) that will
     * not be used any time soon, or query a stop to potential background
     * processing (typically sxplayer).
     *
     * The symmetrical callback for release is the prefetch callback.
     *
     * reentrant: no (comparing state against STATE_READY)
     * execution-order: root first
     * dispatch: managed
     * when: follows the visit phase, as part of ngli_node_honor_release_prefetch()
     */
    void (*release)(struct ngl_node *node);


    /************************
     * Exit stage callbacks *
     ************************/

    /*
     * Must delete everything not released by the release callback. If
     * implemented, the release callback will always be called before uninit.
     *
     * reentrant: no (comparing state against STATE_READY)
     * execution-order: root first
     * dispatch: managed
     * when: called during set_scene() / internal node_set_ctx()
     */
    void (*uninit)(struct ngl_node *node);

    char *(*info_str)(const struct ngl_node *node);
    size_t opts_size;
    size_t priv_size;
    const struct node_param *params;
    const char *params_id;
    size_t livectl_offset;
    uint32_t flags;
    const char *file;
};

void ngli_node_print_specs(void);

int ngli_node_prepare(struct ngl_node *node);
int ngli_node_prepare_children(struct ngl_node *node);
int ngli_node_visit(struct ngl_node *node, int is_active, double t);
int ngli_node_honor_release_prefetch(struct ngl_node *scene, double t);
int ngli_node_update(struct ngl_node *node, double t);
int ngli_node_update_children(struct ngl_node *node, double t);
void *ngli_node_get_data_ptr(struct ngl_node *var_node, void *data_fallback);
int ngli_prepare_draw(struct ngl_ctx *s, double t);
void ngli_node_draw(struct ngl_node *node);

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);
void ngli_node_detach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);

int ngli_node_livectls_get(const struct ngl_node *scene, int *nb_livectlsp, struct ngl_livectl **livectlsp);
void ngli_node_livectls_freep(struct ngl_livectl **livectlsp);

char *ngli_node_default_label(const char *class_name);
int ngli_is_default_label(const char *class_name, const char *str);
const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp);

#endif
