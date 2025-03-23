/*
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

#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdlib.h>

#include "config.h"

#if defined(HAVE_VAAPI)
#include "vaapi_ctx.h"
#endif

#if defined(TARGET_ANDROID)
#include "android_ctx.h"
#endif

#if HAVE_TEXT_LIBRARIES
#include <ft2build.h>
#include FT_OUTLINE_H
#endif

#include "hud.h"
#include "ngpu/ctx.h"
#include "ngpu/rendertarget.h"
#include "nopegl.h"
#include "params.h"
#include "rnode.h"
#include "utils/darray.h"
#include "utils/hmap.h"
#include "utils/pthread_compat.h"
#include "utils/utils.h"

struct node_class;

typedef int (*cmd_func_type)(struct ngl_ctx *s, void *arg);

struct api_impl {
    int (*configure)(struct ngl_ctx *s, const struct ngl_config *config);
    int (*resize)(struct ngl_ctx *s, int32_t width, int32_t height);
    int (*get_viewport)(struct ngl_ctx *s, int32_t *viewport);
    int (*set_capture_buffer)(struct ngl_ctx *s, void *capture_buffer);
    int (*set_scene)(struct ngl_ctx *s, struct ngl_scene *scene);
    int (*prepare_draw)(struct ngl_ctx *s, double t);
    int (*draw)(struct ngl_ctx *s, double t);
    void (*reset)(struct ngl_ctx *s, int action);

    /* OpenGL */
    int (*gl_wrap_framebuffer)(struct ngl_ctx *s, uint32_t framebuffer);
};

void ngli_free_text_builtin_atlas(void *user_arg, void *data);

struct text_builtin_atlas {
    struct distmap *distmap;
    int32_t char_map[256];
};

struct ngl_ctx {
    /* Controller-only fields */
    int configured;
    pthread_t worker_tid;
    const struct api_impl *api_impl;

    /* Worker-only fields */
    struct ngpu_ctx *gpu_ctx;
    struct rnode rnode;
    struct rnode *rnode_pos;
    struct ngl_scene *scene;
    struct ngl_config config;
    struct ngl_backend backend;
    struct ngpu_viewport viewport;
    struct ngpu_scissor scissor;
    struct ngpu_rendertarget *available_rendertargets[2];
    struct ngpu_rendertarget *current_rendertarget;
    int render_pass_started;
    float default_modelview_matrix[16];
    float default_projection_matrix[16];
    struct darray modelview_matrix_stack;
    struct darray projection_matrix_stack;

    /*
     * Array of nodes that are candidate to either prefetch (active) or release
     * (non-active). Nodes are inserted from bottom (leaves) up to the top
     * (root).
     */
    struct darray activitycheck_nodes;

    struct hmap *text_builtin_atlasses; // struct text_builtin_atlas
#if HAVE_TEXT_LIBRARIES
    FT_Library ft_library;
#endif

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
int ngli_ctx_resize(struct ngl_ctx *s, int32_t width, int32_t height);
int ngli_ctx_get_viewport(struct ngl_ctx *s, int32_t *viewport);
int ngli_ctx_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer);
int ngli_ctx_set_scene(struct ngl_ctx *s, struct ngl_scene *scene);
int ngli_ctx_prepare_draw(struct ngl_ctx *s, double t);
int ngli_ctx_draw(struct ngl_ctx *s, double t);
void ngli_ctx_reset(struct ngl_ctx *s, int action);

struct livectl {
    union ngl_livectl_data val;
    char *id;
    union ngl_livectl_data min;
    union ngl_livectl_data max;
};

#define NGLI_NODE_NONE 0xffffffff

struct ngl_node {
    const struct node_class *cls;
    struct ngl_ctx *ctx;
    struct ngl_scene *scene;

    void *opts;

    int state;
    int is_active;

    double visit_time;
    double last_update_time;

    int draw_count;

    int refcount;
    int ctx_refcount;

    struct darray children;
    struct darray draw_children; // children with a draw callback
    struct darray parents;

    char *label;

    void *priv_data;
};

struct ngl_scene {
    struct ngli_rc rc;
    struct ngl_scene_params params;
    struct darray nodes; // set of all the nodes in the graph
    struct darray files; // files path strings (array of char *)
    struct darray files_par; // file based parameters pointers (array of uint8_t *)
};

enum node_category {
    NGLI_NODE_CATEGORY_NONE,
    NGLI_NODE_CATEGORY_VARIABLE,
    NGLI_NODE_CATEGORY_TEXTURE,
    NGLI_NODE_CATEGORY_BUFFER,
    NGLI_NODE_CATEGORY_BLOCK,
    NGLI_NODE_CATEGORY_IO,
    NGLI_NODE_CATEGORY_DRAW, /* node executes a graphics ngpu_pipeline */
    NGLI_NODE_CATEGORY_TRANSFORM,
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
    uint32_t id;
    enum node_category category;
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
     * ready at update time (typically nope.media). Contrary to allocations
     * done in the init, the prefetched resources lifetime is reduced to active
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
     * processing (typically nope.media).
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

/* Internal scene API */
int ngli_scene_deserialize(struct ngl_scene *s, const char *str);
char *ngli_scene_serialize(const struct ngl_scene *s);
char *ngli_scene_dot(const struct ngl_scene *s);
void ngli_scene_update_filepath_ref(struct ngl_node *node, const struct node_param *par);

int ngli_node_prepare(struct ngl_node *node);
int ngli_node_prepare_children(struct ngl_node *node);
int ngli_node_visit(struct ngl_node *node, int is_active, double t);
int ngli_node_honor_release_prefetch(struct ngl_node *scene, double t);
int ngli_node_update(struct ngl_node *node, double t);
int ngli_node_update_children(struct ngl_node *node, double t);
int ngli_prepare_draw(struct ngl_ctx *s, double t);
void ngli_node_draw(struct ngl_node *node);
void ngli_node_draw_children(struct ngl_node *node);

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);
void ngli_node_detach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);

int ngli_is_default_label(const char *class_name, const char *str);
const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp);

#endif
