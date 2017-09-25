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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmap.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"
#include "utils.h"
#include "nodes_register.h"

#define OFFSET(x) offsetof(struct ngl_node, x)
const struct node_param ngli_base_node_params[] = {
    {"glstates", PARAM_TYPE_NODELIST, OFFSET(glstates), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED},
    {"ranges",   PARAM_TYPE_NODELIST, OFFSET(ranges),   .flags=PARAM_FLAG_DOT_DISPLAY_PACKED},
    {"name",     PARAM_TYPE_STR,      OFFSET(name)},
    {NULL}
};

static void *aligned_allocz(size_t size)
{
    void *ptr = NULL;
    if (posix_memalign(&ptr, NGLI_ALIGN, size))
        return NULL;
    memset(ptr, 0, size);
    return ptr;
}

#define ALIGN(v, a) ((v) + (((v) + (a) - 1) & ~((v) - 1)))

static struct ngl_node *node_create(const struct node_class *class)
{
    struct ngl_node *node;
    const size_t node_size = ALIGN(sizeof(*node), NGLI_ALIGN);

    node = aligned_allocz(node_size + class->priv_size);
    if (!node)
        return NULL;
    node->priv_data = ((uint8_t *)node) + node_size;

    /* Make sure the node and its private data are properly aligned */
    ngli_assert((((uintptr_t)node)            & ~(NGLI_ALIGN - 1)) == (uintptr_t)node);
    ngli_assert((((uintptr_t)node->priv_data) & ~(NGLI_ALIGN - 1)) == (uintptr_t)node->priv_data);

    /* We depend on the monotically incrementing by 1 property of these fields */
    ngli_assert(NGL_NODE_UNIFORMVEC4      - NGL_NODE_UNIFORMSCALAR      == 3);
    ngli_assert(NGL_NODE_ANIMKEYFRAMEVEC4 - NGL_NODE_ANIMKEYFRAMESCALAR == 3);
    ngli_assert(NGL_NODE_ANIMATIONVEC4    - NGL_NODE_ANIMATIONSCALAR    == 3);

    node->class = class;
    node->last_update_time = -1.;
    node->active_time = -1.;

    node->refcount = 1;

    node->state = STATE_UNINITIALIZED;

    node->modelview_matrix[ 0] =
    node->modelview_matrix[ 5] =
    node->modelview_matrix[10] =
    node->modelview_matrix[15] = 1.f;

    node->projection_matrix[ 0] =
    node->projection_matrix[ 5] =
    node->projection_matrix[10] =
    node->projection_matrix[15] = 1.f;

    return node;
}

#define DEF_NAME_CHR(c) (((c) >= 'A' && (c) <= 'Z') ? (c) ^ 0x20 : (c))

char *ngli_node_default_name(const char *class_name)
{
    char *name = ngli_strdup(class_name);
    if (!name)
        return NULL;
    for (int i = 0; name[i]; i++)
        name[i] = DEF_NAME_CHR(name[i]);
    return name;
}

#define REGISTER_NODE(type_name, class)         \
    case type_name: {                           \
        extern const struct node_class class;   \
        ngli_assert(class.id == type_name);     \
        return &class;                          \
    }                                           \

static const struct node_class *get_node_class(int type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(REGISTER_NODE)
    }
    return NULL;
}

struct ngl_node *ngli_node_create_noconstructor(int type)
{
    const struct node_class *class = get_node_class(type);
    if (!class)
        return NULL;

    struct ngl_node *node = node_create(class);
    if (!node)
        return NULL;

    ngli_params_set_defaults((uint8_t *)node, ngli_base_node_params);
    ngli_params_set_defaults(node->priv_data, node->class->params);

    node->name = ngli_node_default_name(node->class->name);
    if (!node->name) {
        ngl_node_unrefp(&node);
        return NULL;
    }

    return node;
}

struct ngl_node *ngl_node_create(int type, ...)
{
    struct ngl_node *node = ngli_node_create_noconstructor(type);
    if (!node)
        return NULL;

    va_list ap;
    va_start(ap, type);
    ngli_params_set_constructors(node->priv_data, node->class->params, &ap);
    va_end(ap);

    LOG(VERBOSE, "CREATED %s @ %p", node->name, node);

    return node;
}

static int compare_range(const void *p1, const void *p2)
{
    const struct ngl_node *n1 = *(const struct ngl_node **)p1;
    const struct ngl_node *n2 = *(const struct ngl_node **)p2;
    const struct renderrange *r1 = n1->priv_data;
    const struct renderrange *r2 = n2->priv_data;
    return r1->start_time > r2->start_time;
}

void ngli_node_release(struct ngl_node *node)
{
    if (node->state == STATE_IDLE)
        return;

    if (node->state != STATE_READY)
        return;

    ngli_assert(node->ctx);
    if (node->class->release) {
        LOG(DEBUG, "RELEASE %s @ %p", node->name, node);
        node->class->release(node);
    }
    node->state = STATE_IDLE;
}

static const size_t opt_sizes[] = {
    [PARAM_TYPE_INT]      = sizeof(int),
    [PARAM_TYPE_I64]      = sizeof(int64_t),
    [PARAM_TYPE_DBL]      = sizeof(double),
    [PARAM_TYPE_STR]      = sizeof(char *),
    [PARAM_TYPE_VEC2]     = sizeof(float[2]),
    [PARAM_TYPE_VEC3]     = sizeof(float[3]),
    [PARAM_TYPE_VEC4]     = sizeof(float[4]),
    [PARAM_TYPE_NODE]     = sizeof(struct ngl_node *),
    [PARAM_TYPE_NODELIST] = sizeof(struct ngl_node **) + sizeof(int),
    [PARAM_TYPE_DBLLIST]  = sizeof(double *)           + sizeof(int),
    [PARAM_TYPE_NODEDICT] = sizeof(struct hmap *),
};

/*
 * Reset every field of the private data which is not a parameter. This allows
 * the init() to always be called in a clean state.
 */
static void reset_non_params(struct ngl_node *node)
{
    size_t cur_offset = 0;
    const struct node_param *par = node->class->params;
    uint8_t *base_ptr = node->priv_data;

    while (par && par->key) {
        size_t offset = par->offset;
        if (offset != cur_offset)
            memset(base_ptr + cur_offset, 0, offset - cur_offset);
        cur_offset = offset + opt_sizes[par->type];
        par++;
    }
    memset(base_ptr + cur_offset, 0, node->class->priv_size - cur_offset);
}

static void node_uninit(struct ngl_node *node)
{
    if (node->state == STATE_UNINITIALIZED)
        return;

    ngli_assert(node->ctx);
    ngli_node_release(node);

    if (node->class->uninit) {
        LOG(VERBOSE, "UNINIT %s @ %p", node->name, node);
        node->class->uninit(node);
    }
    reset_non_params(node);
    node->state = STATE_UNINITIALIZED;
}

static int node_set_children_ctx(uint8_t *base_ptr, const struct node_param *params,
                                 struct ngl_ctx *ctx)
{
    if (!params)
        return 0;
    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        if (par->type == PARAM_TYPE_NODE) {
            uint8_t *node_p = base_ptr + par->offset;
            struct ngl_node *node = *(struct ngl_node **)node_p;
            if (node) {
                int ret = ngli_node_attach_ctx(node, ctx);
                if (ret < 0)
                    return ret;
            }
        } else if (par->type == PARAM_TYPE_NODELIST) {
            uint8_t *elems_p = base_ptr + par->offset;
            uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
            struct ngl_node **elems = *(struct ngl_node ***)elems_p;
            const int nb_elems = *(int *)nb_elems_p;
            for (int j = 0; j < nb_elems; j++) {
                int ret = ngli_node_attach_ctx(elems[j], ctx);
                if (ret < 0)
                    return ret;
            }
        } else if (par->type == PARAM_TYPE_NODEDICT) {
            struct hmap *hmap = *(struct hmap **)(base_ptr + par->offset);
            if (!hmap)
                continue;
            const struct hmap_entry *entry = NULL;
            while ((entry = ngli_hmap_next(hmap, entry))) {
                struct ngl_node *node = entry->data;
                int ret = ngli_node_attach_ctx(node, ctx);
                if (ret < 0)
                    return ret;
            }
        }
    }
    return 0;
}

static int node_set_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    int ret;

    if (ctx) {
        if (node->ctx) {
            if (node->ctx != ctx) {
                LOG(ERROR, "\"%s\" is associated with another rendering context", node->name);
                return -1;
            }
        } else {
            node->ctx = ctx;
        }
    } else {
        node_uninit(node);
        node->ctx = NULL;
    }

    if ((ret = node_set_children_ctx(node->priv_data, node->class->params, ctx)) < 0 ||
        (ret = node_set_children_ctx((uint8_t *)node, ngli_base_node_params, ctx)) < 0)
        return ret;
    return 0;
}

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    return node_set_ctx(node, ctx);
}

void ngli_node_detach_ctx(struct ngl_node *node)
{
    int ret = node_set_ctx(node, NULL);
    ngli_assert(ret == 0);
}

int ngli_node_init(struct ngl_node *node)
{
    if (node->state == STATE_INITIALIZED)
        return 0;

    if (node->state != STATE_UNINITIALIZED)
        return 0;

    ngli_assert(node->ctx);
    if (node->class->init) {
        LOG(VERBOSE, "INIT %s @ %p", node->name, node);
        int ret = node->class->init(node);
        if (ret < 0)
            return ret;
    }
    // Sort the ranges by start time. We also skip the ngli_node_init as, for
    // now, render ranges don't have any
    // TODO: merge successive continuous and norender ones?
    qsort(node->ranges, node->nb_ranges, sizeof(*node->ranges), compare_range);

    for (int i = 0; i < node->nb_glstates; i++) {
        int ret = ngli_node_init(node->glstates[i]);
        if (ret < 0)
            return ret;
    }

    node->state = STATE_INITIALIZED;

    return 0;
}

static int get_rr_id(const struct ngl_node *node, int start, double t)
{
    int ret = -1;

    for (int i = start; i < node->nb_ranges; i++) {
        const struct renderrange *rr = node->ranges[i]->priv_data;
        if (rr->start_time > t)
            break;
        ret = i;
    }
    return ret;
}

static int update_rr_state(struct ngl_node *node, double t)
{
    if (!node->nb_ranges)
        return -1;

    int rr_id = get_rr_id(node, node->current_range, t);
    if (rr_id < 0) {
        // range not found, probably backward in time so look from the
        // start
        // TODO: look for rr using bsearch?
        rr_id = get_rr_id(node, 0, t);
    }

    if (rr_id >= 0) {
        if (node->current_range != rr_id) {
            // We leave our current render range, so we reset the "Once" flag
            // for next time we may come in again (seek back)
            struct ngl_node *cur_rr = node->ranges[node->current_range];
            if (cur_rr->class->id == NGL_NODE_RENDERRANGEONCE) {
                struct renderrange *rro = cur_rr->priv_data;
                rro->updated = 0;
            }
        }

        node->current_range = rr_id;
    }

    return rr_id;
}

static void node_update(struct ngl_node *node, double t)
{
    node->drawme = 0;

    const int rr_id = update_rr_state(node, t);
    if (rr_id >= 0) {
        struct ngl_node *rr = node->ranges[rr_id];

        if (rr->class->id == NGL_NODE_RENDERRANGENORENDER)
            return;

        if (rr->class->id == NGL_NODE_RENDERRANGEONCE) {
            struct renderrange *rro = rr->priv_data;
            if (rro->updated)
                return;
            t = rro->render_time;
            rro->updated = 1;
        }
    }

    if (node->last_update_time != t) {
        // Sometimes the node might not be prefetched by the node_check_prefetch()
        // crawling: this could happen when the node was for instance instantiated
        // internally and not through the options. So just to be safe, we
        // "prefetch" it now (a bit late for sure).
        ngli_node_prefetch(node);

        LOG(VERBOSE, "UPDATE %s @ %p with t=%g", node->name, node, t);
        node->class->update(node, t);
    } else {
        LOG(VERBOSE, "%s already updated for t=%g, skip it", node->name, t);
    }
    node->last_update_time = t;
    node->drawme = 1;
}

#define PREFETCH_TIME 1.0
#define MAX_IDLE_TIME (PREFETCH_TIME + 3.0)

// TODO: render once
static void check_activity(struct ngl_node *node, double t, int parent_is_active)
{
    int is_active = parent_is_active;

    int ret = ngli_node_init(node);
    if (ret < 0)
        return;

    /*
     * The life of the parent takes over the life of its children: if the
     * parent is dead, the children are likely dead as well. However, a living
     * children from a dead parent can be revealed by another living branch.
     */
    if (parent_is_active) {
        const int rr_id = update_rr_state(node, t);

        if (rr_id >= 0) {
            struct ngl_node *rr = node->ranges[rr_id];

            node->current_range = rr_id;

            if (rr->class->id == NGL_NODE_RENDERRANGENORENDER) {
                is_active = 0;

                if (rr_id < node->nb_ranges - 1) {
                    // We assume here the next range requires the node started
                    // as the current one doesn't.
                    const struct renderrange *next = node->ranges[rr_id + 1]->priv_data;
                    const double next_use_in = next->start_time - t;

                    if (next_use_in < PREFETCH_TIME) {
                        // The node will actually be needed soon, so we need to
                        // start it if necessary.
                        is_active = 1;
                    } else if (next_use_in < MAX_IDLE_TIME && node->state == STATE_READY) {
                        // The node will be needed in a slight amount of time;
                        // a bit longer than a prefetch period so we don't need
                        // to start it, but in the case where it's actually
                        // already active it's not worth releasing it to start
                        // it again soon after, so we keep it active.
                        is_active = 1;
                    }
                }
            }
        }
    }

    /*
     * If a node is inactive and is already in a dead state, there is no need
     * to check for resources below as we can assume they were already released
     * as well (unless they're shared with another branch) by
     * honor_release_prefetch().
     *
     * On the other hand, we cannot do the same if the node is active, because
     * we have to mark every node below for activity to prevent an early
     * release from another branch.
     */
    if (!is_active && node->state == STATE_IDLE)
        return;

    if (node->active_time != t) {
        // If we never passed through this node for that given time, the new
        // active state takes over to replace the one from a previous update.
        node->is_active = is_active;
        node->active_time = t;
    } else {
        // This is not the first time we come across that node, so if it's
        // needed in that part of the branch we mark it as active so it doesn't
        // get released.
        node->is_active |= is_active;
    }

    /*
     * Now we check activity for all the children
     *
     * Note: this is currently only needed for private options as the base node
     * doesn't contain anything worth prefetching or releasing.
     */
    uint8_t *base_ptr = node->priv_data;
    const struct node_param *par = node->class->params;

    while (par && par->key) {
        switch (par->type) {
            case PARAM_TYPE_NODE: {
                uint8_t *child_p = base_ptr + par->offset;
                struct ngl_node *child = *(struct ngl_node **)child_p;
                if (child)
                    check_activity(child, t, node->is_active);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                uint8_t *elems_p = base_ptr + par->offset;
                uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
                struct ngl_node **elems = *(struct ngl_node ***)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                for (int i = 0; i < nb_elems; i++)
                    check_activity(elems[i], t, node->is_active);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(base_ptr + par->offset);
                if (!hmap)
                    break;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry)))
                    check_activity(entry->data, t, node->is_active);
                break;
            }
        }
        par++;
    }
}

static void honor_release_prefetch(struct ngl_node *node, double t)
{
    uint8_t *base_ptr = node->priv_data;
    const struct node_param *par = node->class->params;

    if (node->active_time != t)
        return;

    while (par && par->key) {
        switch (par->type) {
            case PARAM_TYPE_NODE: {
                uint8_t *child_p = base_ptr + par->offset;
                struct ngl_node *child = *(struct ngl_node **)child_p;
                if (child)
                    honor_release_prefetch(child, t);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                uint8_t *elems_p = base_ptr + par->offset;
                uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
                struct ngl_node **elems = *(struct ngl_node ***)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                for (int i = 0; i < nb_elems; i++)
                    honor_release_prefetch(elems[i], t);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(base_ptr + par->offset);
                if (!hmap)
                    break;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry)))
                    honor_release_prefetch(entry->data, t);
                break;
            }
        }
        par++;
    }

    if (node->is_active)
        ngli_node_prefetch(node);
    else
        ngli_node_release(node);
}

void ngli_node_check_resources(struct ngl_node *node, double t)
{
    check_activity(node, t, 1);
    honor_release_prefetch(node, t);
}

void ngli_node_prefetch(struct ngl_node *node)
{
    if (node->state == STATE_READY)
        return;

    int ret = ngli_node_init(node);
    if (ret < 0)
        return;

    if (node->class->prefetch) {
        LOG(DEBUG, "PREFETCH %s @ %p", node->name, node);
        node->class->prefetch(node);
    }
    node->state = STATE_READY;
}

void ngli_node_update(struct ngl_node *node, double t)
{
    int ret = ngli_node_init(node);
    if (ret < 0)
        return;
    if (node->class->update)
        node_update(node, t);
}

void ngli_honor_glstates(struct ngl_ctx *ctx, int nb_glstates, struct ngl_node **glstates)
{
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    for (int i = 0; i < nb_glstates; i++) {
        struct ngl_node *glstate_node = glstates[i];
        struct glstate *glstate = glstate_node->priv_data;

        if (glstate_node->class->id == NGL_NODE_GLBLENDSTATE) {
            ngli_glGetIntegerv(gl, glstate->capability, (GLint *)&glstate->enabled[1]);
            if (glstate->enabled[0]) {
                ngli_glGetIntegerv(gl, GL_BLEND_SRC_RGB, (GLint *)&glstate->src_rgb[1]);
                ngli_glGetIntegerv(gl, GL_BLEND_DST_RGB, (GLint *)&glstate->dst_rgb[1]);
                ngli_glGetIntegerv(gl, GL_BLEND_SRC_ALPHA, (GLint *)&glstate->src_alpha[1]);
                ngli_glGetIntegerv(gl, GL_BLEND_DST_ALPHA, (GLint *)&glstate->dst_alpha[1]);
                ngli_glGetIntegerv(gl, GL_BLEND_EQUATION_RGB, (GLint *)&glstate->mode_rgb[1]);
                ngli_glGetIntegerv(gl, GL_BLEND_EQUATION_ALPHA, (GLint *)&glstate->mode_alpha[1]);

                ngli_glEnable(gl, glstate->capability);
                ngli_glBlendFuncSeparate(gl, glstate->src_rgb[0], glstate->dst_rgb[0],
                                    glstate->src_alpha[0], glstate->dst_alpha[0]);
                ngli_glBlendEquationSeparate(gl, glstate->mode_rgb[0], glstate->mode_alpha[0]);
            } else {
                ngli_glDisable(gl, glstate->capability);
            }
        } else if (glstate_node->class->id == NGL_NODE_GLCOLORSTATE) {
            GLboolean rgba[4];

            ngli_glGetBooleanv(gl, GL_COLOR_WRITEMASK, rgba);

            glstate->rgba[1][0] = rgba[0];
            glstate->rgba[1][1] = rgba[1];
            glstate->rgba[1][2] = rgba[2];
            glstate->rgba[1][3] = rgba[3];

            ngli_glColorMask(gl, glstate->rgba[0][0],
                          glstate->rgba[0][1],
                          glstate->rgba[0][2],
                          glstate->rgba[0][3]);
        } else if (glstate_node->class->id == NGL_NODE_GLSTENCILSTATE) {
            ngli_glGetIntegerv(gl, glstate->capability, (GLint *)&glstate->enabled[1]);
            if (glstate->enabled[0]) {
                ngli_glGetIntegerv(gl, GL_STENCIL_WRITEMASK, (GLint *)&glstate->writemask[1]);
                ngli_glGetIntegerv(gl, GL_STENCIL_FUNC, (GLint *)&glstate->func[1]);
                ngli_glGetIntegerv(gl, GL_STENCIL_REF, (GLint *)&glstate->func_ref[1]);
                ngli_glGetIntegerv(gl, GL_STENCIL_VALUE_MASK, (GLint *)&glstate->func_mask[1]);
                ngli_glGetIntegerv(gl, GL_STENCIL_FAIL, (GLint *)&glstate->op_sfail[1]);
                ngli_glGetIntegerv(gl, GL_STENCIL_PASS_DEPTH_FAIL, (GLint *)&glstate->op_dpfail[1]);
                ngli_glGetIntegerv(gl, GL_STENCIL_PASS_DEPTH_PASS, (GLint *)&glstate->op_dppass[1]);

                ngli_glEnable(gl, glstate->capability);
                ngli_glStencilMask(gl, glstate->writemask[0]);
                ngli_glStencilFunc(gl, glstate->func[0], glstate->func_ref[0], glstate->func_mask[0]);
                ngli_glStencilOp(gl, glstate->op_sfail[0], glstate->op_dpfail[0], glstate->op_dppass[0]);
            } else {
                ngli_glDisable(gl, glstate->capability);
            }
        } else {
            ngli_glGetIntegerv(gl, glstate->capability, (GLint *)&glstate->enabled[1]);
            if (glstate->enabled[0] != glstate->enabled[1]) {
                if (glstate->enabled[0]) {
                    ngli_glEnable(gl, glstate->capability);
                } else {
                    ngli_glDisable(gl, glstate->capability);
                }
            }
        }
    }
}

void ngli_restore_glstates(struct ngl_ctx *ctx, int nb_glstates, struct ngl_node **glstates)
{
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    for (int i = 0; i < nb_glstates; i++) {
        struct ngl_node *glstate_node = glstates[i];
        struct glstate *glstate = glstates[i]->priv_data;
        if (glstate_node->class->id == NGL_NODE_GLBLENDSTATE) {
            if (glstate->enabled[1]) {
                ngli_glEnable(gl, glstate->capability);
                ngli_glBlendFuncSeparate(gl, glstate->src_rgb[1], glstate->dst_rgb[1],
                                    glstate->src_alpha[1], glstate->dst_alpha[1]);
                ngli_glBlendEquationSeparate(gl, glstate->mode_rgb[1], glstate->mode_alpha[1]);
            } else {
                ngli_glDisable(gl, glstate->capability);
            }
        } else if (glstate_node->class->id == NGL_NODE_GLCOLORSTATE) {
            ngli_glColorMask(gl, glstate->rgba[1][0],
                          glstate->rgba[1][1],
                          glstate->rgba[1][2],
                          glstate->rgba[1][3]);
        } else if (glstate_node->class->id == NGL_NODE_GLSTENCILSTATE) {
            if (glstate->enabled[1]) {
                ngli_glEnable(gl, glstate->capability);
                ngli_glStencilMask(gl, glstate->writemask[1]);
                ngli_glStencilFunc(gl, glstate->func[1], glstate->func_ref[1], glstate->func_mask[1]);
                ngli_glStencilOp(gl, glstate->op_sfail[1], glstate->op_dpfail[1], glstate->op_dppass[1]);
            } else {
                ngli_glDisable(gl, glstate->capability);
            }
        } else {
            if (glstate->enabled[0] != glstate->enabled[1]) {
                if (glstate->enabled[1]) {
                    ngli_glEnable(gl, glstate->capability);
                } else {
                    ngli_glDisable(gl, glstate->capability);
                }
            }
        }
    }
}

void ngli_node_draw(struct ngl_node *node)
{
    if (!node->drawme) {
        LOG(VERBOSE, "%s @ %p not marked for drawing, skip it", node->name, node);
        return;
    }

    if (node->class->draw) {
        LOG(VERBOSE, "DRAW %s @ %p", node->name, node);

        ngli_honor_glstates(node->ctx, node->nb_glstates, node->glstates);
        node->class->draw(node);
        ngli_restore_glstates(node->ctx, node->nb_glstates, node->glstates);
    }
}

const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp)
{
    const struct node_param *par = ngli_params_find(ngli_base_node_params, key);
    *base_ptrp = (uint8_t *)node;

    if (!par) {
        par = ngli_params_find(node->class->params, key);
        *base_ptrp = (uint8_t *)node->priv_data;
    }
    if (!par)
        LOG(ERROR, "parameter %s not found in %s", key, node->class->name);
    return par;
}

int ngl_node_param_add(struct ngl_node *node, const char *key,
                       int nb_elems, void *elems)
{
    int ret = 0;

    uint8_t *base_ptr;
    const struct node_param *par = ngli_node_param_find(node, key, &base_ptr);
    if (!par)
        return -1;

    ret = ngli_params_add(base_ptr, par, nb_elems, elems);
    if (ret < 0)
        LOG(ERROR, "unable to add elements to %s.%s", node->name, key);
    node_uninit(node); // need a reinit after changing options
    return ret;
}

int ngl_node_param_set(struct ngl_node *node, const char *key, ...)
{
    int ret = 0;
    va_list ap;

    uint8_t *base_ptr;
    const struct node_param *par = ngli_node_param_find(node, key, &base_ptr);
    if (!par)
        return -1;

    va_start(ap, key);
    ret = ngli_params_set(base_ptr, par, &ap);
    if (ret < 0)
        LOG(ERROR, "unable to set %s.%s", node->name, key);
    va_end(ap);
    node_uninit(node); // need a reinit after changing options
    return ret;
}

struct ngl_node *ngl_node_ref(struct ngl_node *node)
{
    node->refcount++;
    return node;
}

void ngl_node_unrefp(struct ngl_node **nodep)
{
    int delete = 0;
    struct ngl_node *node = *nodep;

    if (!node)
        return;
    delete = node->refcount-- == 1;
    if (delete) {
        LOG(VERBOSE, "DELETE %s @ %p", node->name, node);
        ngli_assert(!node->ctx);
        ngli_params_free((uint8_t *)node, ngli_base_node_params);
        ngli_params_free(node->priv_data, node->class->params);
        free(node);
    }
    *nodep = NULL;
}
