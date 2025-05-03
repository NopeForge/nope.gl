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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "node_uniform.h"
#include "nodes_register.h"
#include "nopegl.h"
#include "params.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "utils/utils.h"

/* We depend on the monotonically incrementing by 1 property of these fields */
NGLI_STATIC_ASSERT(node_uniform_vec_flt, NGL_NODE_UNIFORMVEC4      - NGL_NODE_UNIFORMFLOAT       == 3);
NGLI_STATIC_ASSERT(node_animkf_vec_flt,  NGL_NODE_ANIMKEYFRAMEVEC4 - NGL_NODE_ANIMKEYFRAMEFLOAT  == 3);
NGLI_STATIC_ASSERT(node_anim_vec_flt,    NGL_NODE_ANIMATEDVEC4     - NGL_NODE_ANIMATEDFLOAT      == 3);

extern const struct param_specs ngli_params_specs[];

/* Warning: the common node parameters *must* not include any node-based parameter */
#define OFFSET(x) offsetof(struct ngl_node, x)
const struct node_param ngli_base_node_params[] = {
    {"label",     NGLI_PARAM_TYPE_STR,      OFFSET(label)},
    {NULL}
};

static void *aligned_allocz(size_t size)
{
    void *ptr = ngli_malloc_aligned(size);
    if (!ptr)
        return NULL;
    memset(ptr, 0, size);
    return ptr;
}

static struct ngl_node *node_create(const struct node_class *cls)
{
    struct ngl_node *node;
    const size_t node_size = NGLI_ALIGN(sizeof(*node), NGLI_ALIGN_VAL);
    const size_t opts_size = NGLI_ALIGN(cls->opts_size, NGLI_ALIGN_VAL);
    const size_t priv_size = NGLI_ALIGN(cls->priv_size, NGLI_ALIGN_VAL);

    node = aligned_allocz(node_size + opts_size + priv_size);
    if (!node)
        return NULL;
    node->opts = ((uint8_t *)node) + node_size;
    node->priv_data = ((uint8_t *)node->opts) + opts_size;

    /* Make sure the node, opts, and its private data are properly aligned */
    ngli_assert(NGLI_IS_ALIGNED((uintptr_t)node, NGLI_ALIGN_VAL));
    ngli_assert(NGLI_IS_ALIGNED((uintptr_t)node->opts, NGLI_ALIGN_VAL));
    ngli_assert(NGLI_IS_ALIGNED((uintptr_t)node->priv_data, NGLI_ALIGN_VAL));

    node->cls = cls;
    node->last_update_time = -1.;
    node->visit_time = -1.;

    node->refcount = 1;

    node->state = NODE_STATE_UNINITIALIZED;

    return node;
}

#define DEF_NAME_CHR(c) (((c) >= 'A' && (c) <= 'Z') ? (c) ^ 0x20 : (c))

static char *node_default_label(const char *class_name)
{
    char *label = ngli_strdup(class_name);
    if (!label)
        return NULL;
    for (size_t i = 0; label[i]; i++)
        label[i] = DEF_NAME_CHR(label[i]);
    return label;
}

int ngli_is_default_label(const char *class_name, const char *str)
{
    const size_t len = strlen(class_name);
    if (len != strlen(str))
        return 0;
    for (size_t i = 0; i < len; i++)
        if (DEF_NAME_CHR(class_name[i]) != str[i])
            return 0;
    return 1;
}

#define REGISTER_NODE(type_name, cls)           \
    case type_name: {                           \
        extern const struct node_class cls;     \
        ngli_assert(cls.id == type_name);       \
        return &cls;                            \
    }                                           \

static const struct node_class *get_node_class(uint32_t type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(REGISTER_NODE)
        default:
            return NULL;
    }
}

struct ngl_node *ngl_node_create(uint32_t type)
{
    const struct node_class *cls = get_node_class(type);
    if (!cls) {
        LOG(ERROR, "unknown node type 0x%x", type);
        return NULL;
    }

    struct ngl_node *node = node_create(cls);
    if (!node)
        return NULL;

    if (ngli_params_set_defaults((uint8_t *)node, ngli_base_node_params) < 0 ||
        ngli_params_set_defaults(node->opts, node->cls->params) < 0) {
        ngl_node_unrefp(&node);
        return NULL;
    }

    node->label = node_default_label(node->cls->name);
    if (!node->label) {
        ngl_node_unrefp(&node);
        return NULL;
    }

    LOG(VERBOSE, "CREATED %s @ %p", node->label, node);

    return node;
}

static void node_release(struct ngl_node *node)
{
    if (node->state != NODE_STATE_READY)
        return;

    ngli_assert(node->ctx);
    if (node->cls->release) {
        TRACE("RELEASE %s @ %p", node->label, node);
        node->cls->release(node);
    }
    node->state = NODE_STATE_INITIALIZED;
    node->last_update_time = -1.;
}

static void node_uninit(struct ngl_node *node)
{
    if (node->state == NODE_STATE_UNINITIALIZED)
        return;

    ngli_assert(node->ctx);
    node_release(node);

    if (node->cls->uninit) {
        LOG(VERBOSE, "UNINIT %s @ %p", node->label, node);
        node->cls->uninit(node);
    }
    memset(node->priv_data, 0, node->cls->priv_size);
    node->state = NODE_STATE_UNINITIALIZED;
    node->visit_time = -1.;
}

static int node_init(struct ngl_node *node)
{
    if (node->state != NODE_STATE_UNINITIALIZED)
        return 0;

    ngli_assert(node->ctx);
    if (node->cls->init) {
        LOG(VERBOSE, "INIT %s @ %p", node->label, node);
        int ret = node->cls->init(node);
        if (ret < 0) {
            LOG(ERROR, "initializing node %s failed: %s", node->label, NGLI_RET_STR(ret));
            node->state = NODE_STATE_INIT_FAILED;
            node_uninit(node);
            return ret;
        }
    }

    if (node->cls->prefetch)
        node->state = NODE_STATE_INITIALIZED;
    else
        node->state = NODE_STATE_READY;

    return 0;
}

static int node_set_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    int ret;

    struct ngl_node **children = ngli_darray_data(&node->children);
    for (size_t i = 0; i < ngli_darray_count(&node->children); i++) {
        struct ngl_node *child = children[i];
        ret = node_set_ctx(child, ctx);
        if (ret < 0)
            return ret;
    }

    node->ctx = ctx;
    ret = node_init(node);
    if (ret < 0) {
        node->ctx = NULL;
        return ret;
    }
    node->ctx_refcount++;

    return 0;
}

static void node_reset_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    if (node->state > NODE_STATE_UNINITIALIZED) {
        if (node->ctx != ctx)
            return;
        if (node->ctx_refcount-- == 1) {
            node_uninit(node);
            node->ctx = NULL;
        }
    }
    ngli_assert(node->ctx_refcount >= 0);

    struct ngl_node **children = ngli_darray_data(&node->children);
    for (size_t i = 0; i < ngli_darray_count(&node->children); i++) {
        struct ngl_node *child = children[i];
        node_reset_ctx(child, ctx);
    }
}

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    int ret = node_set_ctx(node, ctx);
    if (ret < 0)
        return ret;

    ret = ngli_node_prepare(node);
    if (ret < 0)
        return ret;

    return ret;
}

void ngli_node_detach_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    node_reset_ctx(node, ctx);
}

int ngli_node_prepare_children(struct ngl_node *node)
{
    struct ngl_node **children = ngli_darray_data(&node->children);
    for (size_t i = 0; i < ngli_darray_count(&node->children); i++) {
        int ret = ngli_node_prepare(children[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int ngli_node_prepare(struct ngl_node *node)
{
    if (node->cls->prepare) {
        TRACE("PREPARE %s @ %p", node->label, node);
        int ret = node->cls->prepare(node);
        if (ret < 0) {
            LOG(ERROR, "preparing node %s failed: %s", node->label, NGLI_RET_STR(ret));
            return ret;
        }
        return 0;
    }
    return ngli_node_prepare_children(node);
}

int ngli_node_visit(struct ngl_node *node, bool is_active, double t)
{
    /*
     * If a node is inactive and meant to be, there is no need
     * to check for resources below as we can assume they were already released
     * as well (unless they're shared with another branch) by
     * honor_release_prefetch().
     *
     * On the other hand, we cannot do the same if the node is active, because
     * we have to mark every node below for activity to prevent an early
     * release from another branch.
     */
    if (!is_active && !node->is_active)
        return 0;

    const int queue_node = node->visit_time != t;

    if (queue_node) {
        /*
         * If a node is active or is going to be activated but has already been
         * updated for that time previously, we need to force its update. This
         * scenario is rare but can happen with specific time sequences on time
         * filtered diamond-tree graphs.
         */
        if (is_active && node->last_update_time == t)
            node->last_update_time = -1.;
        /*
         * If we never passed through this node for that given time, the new
         * active state takes over to replace the one from a previous update.
         */
        node->is_active = is_active;
        node->visit_time = t;
    } else {
        /*
         * This is not the first time we come across that node, so if it's
         * needed in that part of the branch we mark it as active so it doesn't
         * get released.
         */
        node->is_active |= is_active;
    }

    if (node->cls->visit) {
        int ret = node->cls->visit(node, is_active, t);
        if (ret < 0)
            return ret;
    } else {
        struct darray *children_array = &node->children;
        struct ngl_node **children = ngli_darray_data(children_array);
        for (size_t i = 0; i < ngli_darray_count(children_array); i++) {
            struct ngl_node *child = children[i];
            int ret = ngli_node_visit(child, is_active, t);
            if (ret < 0)
                return ret;
        }
    }

    /* Insert children (leaves) first */
    if (queue_node &&
        (node->cls->prefetch || node->cls->release) &&
        !ngli_darray_push(&node->ctx->activitycheck_nodes, &node))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int node_prefetch(struct ngl_node *node)
{
    if (node->state == NODE_STATE_READY)
        return 0;

    if (node->cls->prefetch) {
        TRACE("PREFETCH %s @ %p", node->label, node);
        int ret = node->cls->prefetch(node);
        if (ret < 0) {
            LOG(ERROR, "prefetching node %s failed: %s", node->label, NGLI_RET_STR(ret));
            node->visit_time = -1.;
            if (node->cls->release) {
                LOG(VERBOSE, "RELEASE %s @ %p", node->label, node);
                node->cls->release(node);
            }
            return ret;
        }
    }
    node->state = NODE_STATE_READY;

    return 0;
}

int ngli_node_honor_release_prefetch(struct ngl_node *scene, double t)
{
    /* Build a new list of activity checks nodes */
    struct darray *nodes_array = &scene->ctx->activitycheck_nodes;
    ngli_darray_clear(nodes_array);
    int ret = ngli_node_visit(scene, true, t);
    if (ret < 0)
        return ret;

    struct ngl_node **nodes = ngli_darray_data(nodes_array);

    /* Release nodes starting from the parents (root) down to the children (leaves) */
    for (size_t i = 0; i < ngli_darray_count(nodes_array); i++) {
        struct ngl_node *node = nodes[ngli_darray_count(nodes_array) - i - 1];
        if (!node->is_active)
            node_release(node);
    }

    /* Prefetch nodes starting from the children (leaves) up to the parents (root) */
    for (size_t i = 0; i < ngli_darray_count(nodes_array); i++) {
        struct ngl_node *node = nodes[i];
        if (node->is_active) {
            ret = node_prefetch(node);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

int ngli_node_update(struct ngl_node *node, double t)
{
    ngli_assert(node->state == NODE_STATE_READY);
    if (node->cls->update) {
        if (node->last_update_time != t) {
            TRACE("UPDATE %s @ %p with t=%g", node->label, node, t);
            int ret = node->cls->update(node, t);
            if (ret < 0) {
                LOG(ERROR, "updating node %s failed: %s", node->label, NGLI_RET_STR(ret));
                return ret;
            }
            node->last_update_time = t;
            node->draw_count = 0;
        } else {
            TRACE("%s already updated for t=%g, skip it", node->label, t);
        }
    }

    return 0;
}

int ngli_node_update_children(struct ngl_node *node, double t)
{
    struct ngl_node **children = ngli_darray_data(&node->children);
    for (size_t i = 0; i < ngli_darray_count(&node->children); i++) {
        struct ngl_node *child = children[i];
        int ret = ngli_node_update(child, t);
        if (ret < 0)
            return ret;
    }
    return 0;
}

void *ngli_node_get_data_ptr(const struct ngl_node *var_node, void *data_fallback)
{
    if (!var_node)
        return data_fallback;
    ngli_assert(var_node->cls->category == NGLI_NODE_CATEGORY_VARIABLE);
    struct variable_info *var = var_node->priv_data;
    return var->data;
}

void ngli_node_draw_children(struct ngl_node *node)
{
    struct ngl_node **children = ngli_darray_data(&node->children);
    for (size_t i = 0; i < ngli_darray_count(&node->children); i++) {
        struct ngl_node *child = children[i];
        ngli_node_draw(child);
    }
}

void ngli_node_draw(struct ngl_node *node)
{
    if (node->cls->draw) {
        TRACE("DRAW %s @ %p", node->label, node);
        node->cls->draw(node);
        node->draw_count++;
    }
}

const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp)
{
    const struct node_param *par = ngli_params_find(ngli_base_node_params, key);
    *base_ptrp = (uint8_t *)node;

    if (!par) {
        par = ngli_params_find(node->cls->params, key);
        *base_ptrp = (uint8_t *)node->opts;
    }
    if (!par)
        LOG(ERROR, "parameter %s not found in %s", key, node->cls->name);
    return par;
}

static int param_add(struct ngl_node *node, const char *key, size_t nb_elems, void *elems)
{
    int ret = 0;

    uint8_t *base_ptr;
    const struct node_param *par = ngli_node_param_find(node, key, &base_ptr);
    if (!par)
        return NGL_ERROR_NOT_FOUND;

    if (node->ctx && !(par->flags & NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE)) {
        LOG(ERROR, "%s.%s can not be live extended", node->label, key);
        return NGL_ERROR_INVALID_USAGE;
    }

    ret = ngli_params_add(base_ptr, par, nb_elems, elems);
    if (ret < 0) {
        LOG(ERROR, "unable to add elements to %s.%s", node->label, key);
        return ret;
    }

    if (node->ctx && par->update_func)
        ret = par->update_func(node);

    return ret;
}

int ngl_node_param_add_nodes(struct ngl_node *node, const char *key,
                             size_t nb_nodes, struct ngl_node **nodes)
{
    if (node->scene) {
        LOG(ERROR, "the nodes graph cannot be extended after being associated with a scene");
        return NGL_ERROR_INVALID_USAGE;
    }
    return param_add(node, key, nb_nodes, nodes);
}

int ngl_node_param_add_f64s(struct ngl_node *node, const char *key,
                            size_t nb_f64s, double *f64s)
{
    return param_add(node, key, nb_f64s, f64s);
}

static int node_invalidate_branch(struct ngl_node *node)
{
    node->last_update_time = -1;
    if (node->cls->invalidate) {
        int ret = node->cls->invalidate(node);
        if (ret < 0)
            return ret;
    }
    struct ngl_node **parents = ngli_darray_data(&node->parents);
    for (size_t i = 0; i < ngli_darray_count(&node->parents); i++) {
        int ret = node_invalidate_branch(parents[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int node_param_is_value_allowed(struct ngl_node *node, const char *key,
                                       const uint8_t *ptr, const struct node_param *par)
{
    if (!node->ctx)
        return 0;

    if (!(par->flags & NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE)) {
        LOG(ERROR, "%s.%s can not be live changed", node->label, key);
        return NGL_ERROR_INVALID_USAGE;
    }

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE) {
        const struct ngl_node *pnode = *(struct ngl_node **)ptr;
        if (pnode) {
            LOG(ERROR, "%s.%s can not be live changed because it is associated with a node", node->label, key);
            return NGL_ERROR_INVALID_USAGE;
        }
    }

    return 0;
}

static int node_param_update(struct ngl_node *node, const struct node_param *par)
{
    if (node->scene && par->flags & NGLI_PARAM_FLAG_FILEPATH)
        ngli_scene_update_filepath_ref(node, par);

    if (!node->ctx)
        return 0;

    if (par->update_func) {
        int ret = par->update_func(node);
        if (ret < 0)
            return ret;
    }

    return node_invalidate_branch(node);
}

#define FORWARD_TO_PARAM(type, ...)                                     \
    int ret;                                                            \
    uint8_t *base_ptr;                                                  \
    const struct node_param *par =                                      \
        ngli_node_param_find(node, key, &base_ptr);                     \
    if (!par)                                                           \
        return NGL_ERROR_NOT_FOUND;                                     \
    uint8_t *dst = base_ptr + par->offset;                              \
    if ((ret = node_param_is_value_allowed(node, key, dst, par)) < 0 || \
        (ret = ngli_params_set_##type(dst, par, __VA_ARGS__)) < 0 ||    \
        (ret = node_param_update(node, par)) < 0)                       \
        return ret;                                                     \
    return 0

int ngl_node_param_set_bool(struct ngl_node *node, const char *key, int value)
{
    FORWARD_TO_PARAM(bool, value);
}

int ngl_node_param_set_data(struct ngl_node *node, const char *key, size_t size, const void *data)
{
    FORWARD_TO_PARAM(data, size, data);
}

int ngl_node_param_set_f32(struct ngl_node *node, const char *key, float value)
{
    FORWARD_TO_PARAM(f32, value);
}

int ngl_node_param_set_f64(struct ngl_node *node, const char *key, double value)
{
    FORWARD_TO_PARAM(f64, value);
}

int ngl_node_param_set_flags(struct ngl_node *node, const char *key, const char *value)
{
    FORWARD_TO_PARAM(flags, value);
}

int ngl_node_param_set_i32(struct ngl_node *node, const char *key, int32_t value)
{
    FORWARD_TO_PARAM(i32, value);
}

int ngl_node_param_set_ivec2(struct ngl_node *node, const char *key, const int32_t *value)
{
    FORWARD_TO_PARAM(ivec2, value);
}

int ngl_node_param_set_ivec3(struct ngl_node *node, const char *key, const int32_t *value)
{
    FORWARD_TO_PARAM(ivec3, value);
}

int ngl_node_param_set_ivec4(struct ngl_node *node, const char *key, const int32_t *value)
{
    FORWARD_TO_PARAM(ivec4, value);
}

int ngl_node_param_set_mat4(struct ngl_node *node, const char *key, const float *value)
{
    FORWARD_TO_PARAM(mat4, value);
}

int ngl_node_param_set_node(struct ngl_node *node, const char *key, struct ngl_node *value)
{
    if (node->ctx) {
        LOG(ERROR, "%s.%s node can not be live changed", node->label, key);
        return NGL_ERROR_INVALID_USAGE;
    }
    if (node->scene) {
        LOG(ERROR, "the structure of the graph cannot be changed after being associated with a scene");
        return NGL_ERROR_INVALID_USAGE;
    }
    FORWARD_TO_PARAM(node, value);
}

int ngl_node_param_set_rational(struct ngl_node *node, const char *key, int32_t num, int32_t den)
{
    FORWARD_TO_PARAM(rational, num, den);
}

int ngl_node_param_set_select(struct ngl_node *node, const char *key, const char *value)
{
    FORWARD_TO_PARAM(select, value);
}

int ngl_node_param_set_str(struct ngl_node *node, const char *key, const char *value)
{
    FORWARD_TO_PARAM(str, value);
}

int ngl_node_param_set_u32(struct ngl_node *node, const char *key, const uint32_t value)
{
    FORWARD_TO_PARAM(u32, value);
}

int ngl_node_param_set_uvec2(struct ngl_node *node, const char *key, const uint32_t *value)
{
    FORWARD_TO_PARAM(uvec2, value);
}

int ngl_node_param_set_uvec3(struct ngl_node *node, const char *key, const uint32_t *value)
{
    FORWARD_TO_PARAM(uvec3, value);
}

int ngl_node_param_set_uvec4(struct ngl_node *node, const char *key, const uint32_t *value)
{
    FORWARD_TO_PARAM(uvec4, value);
}

int ngl_node_param_set_vec2(struct ngl_node *node, const char *key, const float *value)
{
    FORWARD_TO_PARAM(vec2, value);
}

int ngl_node_param_set_vec3(struct ngl_node *node, const char *key, const float *value)
{
    FORWARD_TO_PARAM(vec3, value);
}

int ngl_node_param_set_vec4(struct ngl_node *node, const char *key, const float *value)
{
    FORWARD_TO_PARAM(vec4, value);
}

int ngl_node_param_set_dict(struct ngl_node *node, const char *key, const char *name, struct ngl_node *value)
{
    if (node->scene) {
        LOG(ERROR, "the nodes graph cannot be extended after being associated with a scene");
        return NGL_ERROR_INVALID_USAGE;
    }
    FORWARD_TO_PARAM(dict, name, value);
}

struct ngl_node *ngl_node_ref(struct ngl_node *node)
{
    node->refcount++;
    return node;
}

void ngl_node_unrefp(struct ngl_node **nodep)
{
    struct ngl_node *node = *nodep;

    if (!node)
        return;
    const int delete = node->refcount-- == 1;
    if (delete) {
        LOG(VERBOSE, "DELETE %s @ %p", node->label, node);
        ngli_assert(!node->ctx);
        ngli_params_free((uint8_t *)node, ngli_base_node_params);
        ngli_params_free(node->opts, node->cls->params);
        ngli_free_aligned(node);
    }
    *nodep = NULL;
}
