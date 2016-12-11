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

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"
#include "utils.h"

extern const struct node_class ngli_attributevec2_class;
extern const struct node_class ngli_attributevec3_class;
extern const struct node_class ngli_attributevec4_class;
extern const struct node_class ngli_camera_class;
extern const struct node_class ngli_texture_class;
extern const struct node_class ngli_glstate_class;
extern const struct node_class ngli_glblendstate_class;
extern const struct node_class ngli_group_class;
extern const struct node_class ngli_identity_class;
extern const struct node_class ngli_media_class;
extern const struct node_class ngli_texturedshape_class;
extern const struct node_class ngli_quad_class;
extern const struct node_class ngli_triangle_class;
extern const struct node_class ngli_shapeprimitive_class;
extern const struct node_class ngli_shape_class;
extern const struct node_class ngli_shader_class;
extern const struct node_class ngli_renderrangecontinuous_class;
extern const struct node_class ngli_renderrangenorender_class;
extern const struct node_class ngli_renderrangeonce_class;
extern const struct node_class ngli_rotate_class;
extern const struct node_class ngli_rtt_class;
extern const struct node_class ngli_translate_class;
extern const struct node_class ngli_scale_class;
extern const struct node_class ngli_animkeyframescalar_class;
extern const struct node_class ngli_animkeyframevec2_class;
extern const struct node_class ngli_animkeyframevec3_class;
extern const struct node_class ngli_animkeyframevec4_class;
extern const struct node_class ngli_uniformscalar_class;
extern const struct node_class ngli_uniformvec2_class;
extern const struct node_class ngli_uniformvec3_class;
extern const struct node_class ngli_uniformvec4_class;
extern const struct node_class ngli_uniformint_class;
extern const struct node_class ngli_uniformmat4_class;
extern const struct node_class ngli_uniformsampler_class;
extern const struct node_class ngli_fps_class;

static const struct node_class *node_class_map[] = {
    [NGL_NODE_ATTRIBUTEVEC2]         = &ngli_attributevec2_class,
    [NGL_NODE_ATTRIBUTEVEC3]         = &ngli_attributevec3_class,
    [NGL_NODE_ATTRIBUTEVEC4]         = &ngli_attributevec4_class,
    [NGL_NODE_CAMERA]                = &ngli_camera_class,
    [NGL_NODE_TEXTURE]               = &ngli_texture_class,
    [NGL_NODE_MEDIA]                 = &ngli_media_class,
    [NGL_NODE_GLSTATE]               = &ngli_glstate_class,
    [NGL_NODE_GLBLENDSTATE]          = &ngli_glblendstate_class,
    [NGL_NODE_GROUP]                 = &ngli_group_class,
    [NGL_NODE_IDENTITY]              = &ngli_identity_class,
    [NGL_NODE_TEXTUREDSHAPE]         = &ngli_texturedshape_class,
    [NGL_NODE_QUAD]                  = &ngli_quad_class,
    [NGL_NODE_TRIANGLE]              = &ngli_triangle_class,
    [NGL_NODE_SHAPEPRIMITIVE]        = &ngli_shapeprimitive_class,
    [NGL_NODE_SHAPE]                 = &ngli_shape_class,
    [NGL_NODE_SHADER]                = &ngli_shader_class,
    [NGL_NODE_RENDERRANGECONTINUOUS] = &ngli_renderrangecontinuous_class,
    [NGL_NODE_RENDERRANGENORENDER]   = &ngli_renderrangenorender_class,
    [NGL_NODE_RENDERRANGEONCE]       = &ngli_renderrangeonce_class,
    [NGL_NODE_RTT]                   = &ngli_rtt_class,
    [NGL_NODE_ROTATE]                = &ngli_rotate_class,
    [NGL_NODE_TRANSLATE]             = &ngli_translate_class,
    [NGL_NODE_SCALE]                 = &ngli_scale_class,
    [NGL_NODE_ANIMKEYFRAMESCALAR]    = &ngli_animkeyframescalar_class,
    [NGL_NODE_ANIMKEYFRAMEVEC2]      = &ngli_animkeyframevec2_class,
    [NGL_NODE_ANIMKEYFRAMEVEC3]      = &ngli_animkeyframevec3_class,
    [NGL_NODE_ANIMKEYFRAMEVEC4]      = &ngli_animkeyframevec4_class,
    [NGL_NODE_UNIFORMSCALAR]         = &ngli_uniformscalar_class,
    [NGL_NODE_UNIFORMVEC2]           = &ngli_uniformvec2_class,
    [NGL_NODE_UNIFORMVEC3]           = &ngli_uniformvec3_class,
    [NGL_NODE_UNIFORMVEC4]           = &ngli_uniformvec4_class,
    [NGL_NODE_UNIFORMINT]            = &ngli_uniformint_class,
    [NGL_NODE_UNIFORMMAT4]           = &ngli_uniformmat4_class,
    [NGL_NODE_UNIFORMSAMPLER]        = &ngli_uniformsampler_class,
    [NGL_NODE_FPS]                   = &ngli_fps_class,
};

static const char *param_type_strings[] = {
    [PARAM_TYPE_INT]      = "int",
    [PARAM_TYPE_I64]      = "i64",
    [PARAM_TYPE_DBL]      = "double",
    [PARAM_TYPE_STR]      = "string",
    [PARAM_TYPE_VEC2]     = "vec2",
    [PARAM_TYPE_VEC3]     = "vec3",
    [PARAM_TYPE_VEC4]     = "vec4",
    [PARAM_TYPE_NODE]     = "Node",
    [PARAM_TYPE_NODELIST] = "NodeList",
    [PARAM_TYPE_DBLLIST]  = "doubleList",
};

#define OFFSET(x) offsetof(struct ngl_node, x)
const struct node_param ngli_base_node_params[] = {
    {"glstates", PARAM_TYPE_NODELIST, OFFSET(glstates), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED},
    {"ranges",   PARAM_TYPE_NODELIST, OFFSET(ranges),   .flags=PARAM_FLAG_DOT_DISPLAY_PACKED},
    {"name",     PARAM_TYPE_STR,      OFFSET(name)},
    {NULL}
};

static void print_param(const struct node_param *p)
{
    printf("        - [%s, %s]\n",
           p->key, param_type_strings[p->type]);
}

static void print_node_params(const char *name, const struct node_param *p)
{
    printf("- %s:\n", name);
    if (p) {
        if (p->key && (p->flags & PARAM_FLAG_CONSTRUCTOR)) {
            printf("    constructors:\n");
            while (p->key && (p->flags & PARAM_FLAG_CONSTRUCTOR)) {
                print_param(p);
                p++;
            }
        }
        if (p->key && !(p->flags & PARAM_FLAG_CONSTRUCTOR)) {
            printf("    optional:\n");
            while (p->key && !(p->flags & PARAM_FLAG_CONSTRUCTOR)) {
                print_param(p);
                p++;
            }
        }
    }
    printf("\n");
}

void ngli_node_print_specs(void)
{
    printf("#\n# Nodes specifications for node.gl v%d.%d.%d\n#\n\n",
           NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    print_node_params("_Node", ngli_base_node_params);
    for (int i = 0; i < NGLI_ARRAY_NB(node_class_map); i++) {
        const struct node_class *c = node_class_map[i];
        if (c) {
            const struct node_param *p = &c->params[0];
            print_node_params(c->name, p);
        }
    }
}

static struct ngl_node *node_create(const struct node_class *class)
{
    struct ngl_node *node = calloc(1, sizeof(*node));
    if (!node)
        return NULL;
    if (class->priv_size) {
        node->priv_data = calloc(1, class->priv_size);
        if (!node->priv_data) {
            free(node);
            return NULL;
        }
    }

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

struct ngl_node *ngl_node_create(int type, ...)
{
    va_list ap;
    struct ngl_node *node = NULL;

    va_start(ap, type);
    if (type < 0 || type >= NGLI_ARRAY_NB(node_class_map))
        goto end;
    node = node_create(node_class_map[type]);
    if (!node)
        goto end;

    ngli_params_set_defaults((uint8_t *)node, ngli_base_node_params);
    ngli_params_set_defaults(node->priv_data, node->class->params);

    /* set default name to lower(class->name) */
    node->name = ngli_strdup(node->class->name);
    if (!node->name) {
        free(node);
        return NULL;
    }
    for (int i = 0; node->name[i]; i++)
        if (node->name[i] >= 'A' && node->name[i] <= 'Z')
            node->name[i] ^= 0x20;

    ngli_params_set_constructors(node->priv_data, node->class->params, &ap);

    LOG(VERBOSE, "CREATED %s @ %p", node->name, node);

end:
    va_end(ap);
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

    ngli_node_init(node);

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

    ngli_node_init(node);

    if (node->class->prefetch) {
        LOG(DEBUG, "PREFETCH %s @ %p", node->name, node);
        node->class->prefetch(node);
    }
    node->state = STATE_READY;
}

void ngli_node_update(struct ngl_node *node, double t)
{
    ngli_node_init(node);
    if (node->class->update)
        node_update(node, t);
}

void ngli_node_draw(struct ngl_node *node)
{
    if (!node->drawme) {
        LOG(VERBOSE, "%s @ %p not marked for drawing, skip it", node->name, node);
        return;
    }

    if (node->class->draw) {
        LOG(VERBOSE, "DRAW %s @ %p", node->name, node);

        for (int i = 0; i < node->nb_glstates; i++) {
            struct ngl_node *glstate_node = node->glstates[i];
            struct glstate *glstate = glstate_node->priv_data;

            if (glstate_node->class->id == NGL_NODE_GLBLENDSTATE) {
                glGetIntegerv(glstate->capability, (GLint *)&glstate->enabled[1]);
                if (glstate->enabled[0]) {
                    glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&glstate->src_rgb[1]);
                    glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&glstate->dst_rgb[1]);
                    glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&glstate->src_alpha[1]);
                    glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&glstate->dst_alpha[1]);
                    glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&glstate->mode_rgb[1]);
                    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&glstate->mode_alpha[1]);

                    glEnable(glstate->capability);
                    glBlendFuncSeparate(glstate->src_rgb[0], glstate->dst_rgb[0],
                                        glstate->src_alpha[0], glstate->dst_alpha[0]);
                    glBlendEquationSeparate(glstate->mode_rgb[0], glstate->mode_alpha[0]);
                } else {
                    glDisable(glstate->capability);
                }
            } else {
                glGetIntegerv(glstate->capability, (GLint *)&glstate->enabled[1]);
                if (glstate->enabled[0] != glstate->enabled[1]) {
                    if (glstate->enabled[0]) {
                        glEnable(glstate->capability);
                    } else {
                        glDisable(glstate->capability);
                    }
                }
            }
        }

        node->class->draw(node);

        for (int i = 0; i < node->nb_glstates; i++) {
            struct ngl_node *glstate_node = node->glstates[i];
            struct glstate *glstate = node->glstates[i]->priv_data;
            if (glstate_node->class->id == NGL_NODE_GLBLENDSTATE) {
                if (glstate->enabled[1]) {
                    glEnable(glstate->capability);
                    glBlendFuncSeparate(glstate->src_rgb[1], glstate->dst_rgb[1],
                                        glstate->src_alpha[1], glstate->dst_alpha[1]);
                    glBlendEquationSeparate(glstate->mode_rgb[1], glstate->mode_alpha[1]);
                } else {
                    glDisable(glstate->capability);
                }
            } else {
                if (glstate->enabled[0] != glstate->enabled[1]) {
                    if (glstate->enabled[1]) {
                        glEnable(glstate->capability);
                    } else {
                        glDisable(glstate->capability);
                    }
                }
            }
        }
    }
}

static const struct node_param *node_param_find(const struct ngl_node *node, const char *key,
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
    const struct node_param *par = node_param_find(node, key, &base_ptr);
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
    const struct node_param *par = node_param_find(node, key, &base_ptr);
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

void ngl_node_ref(struct ngl_node *node)
{
    node->refcount++;
}

void ngl_node_unrefp(struct ngl_node **op)
{
    int delete = 0;
    struct ngl_node *node = *op;

    if (!node)
        return;
    delete = node->refcount-- == 1;
    if (delete) {
        LOG(VERBOSE, "DELETE %s @ %p", node->name, node);
        ngli_assert(!node->ctx);
        ngli_params_free((uint8_t *)node, ngli_base_node_params);
        ngli_params_free(node->priv_data, node->class->params);
        free(node->priv_data);
        free(node);
    }
    *op = NULL;
}
