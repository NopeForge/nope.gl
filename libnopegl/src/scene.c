/*
 * Copyright 2023 Nope Forge
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

#include "internal.h"
#include "log.h"
#include "memory.h"
#include "utils.h"

NGLI_RC_CHECK_STRUCT(ngl_scene);

static int reset_nodes(void *user_arg, struct ngl_node *parent, struct ngl_node *node)
{
    struct ngl_scene *s = user_arg;

    if (!node->scene)
        return 0;

    /*
     * This can happen if a failure happened during nodes association, for
     * example if part of the graph was associated with another scene. We make
     * sure to reset only the nodes we actually own.
     */
    if (node->scene != s)
        return 0;

    ngli_assert(!node->ctx);

    int ret = ngli_node_children_apply_func(reset_nodes, s, node);
    ngli_assert(ret == 0);

    ngli_darray_reset(&node->children);
    ngli_darray_reset(&node->parents);

    node->scene = NULL;
    return 0;
}

static void detach_root(struct ngl_scene *s)
{
    if (!s->params.root)
        return;

    int ret = reset_nodes(s, NULL, s->params.root);
    ngli_assert(ret == 0);

    ngl_node_unrefp(&s->params.root);
}

static int setup_nodes(void *user_arg, struct ngl_node *parent, struct ngl_node *node)
{
    struct ngl_scene *s = user_arg;

    if (node->scene) {
        if (node->scene != s) {
            LOG(ERROR, "one or more nodes of the graph are associated with another scene already");
            return NGL_ERROR_INVALID_USAGE;
        }
    } else {
        node->scene = s;

        ngli_darray_init(&node->children, sizeof(struct ngl_node *), 0);
        ngli_darray_init(&node->parents, sizeof(struct ngl_node *), 0);

        int ret = ngli_node_children_apply_func(setup_nodes, s, node);
        if (ret < 0)
            return ret;
    }

    if (parent) {
        if (!ngli_darray_push(&parent->children, &node))
            return NGL_ERROR_MEMORY;
        if (!ngli_darray_push(&node->parents, &parent))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int attach_root(struct ngl_scene *s, struct ngl_node *node)
{
    s->params.root = ngl_node_ref(node);

    int ret = setup_nodes(s, NULL, s->params.root);
    if (ret < 0)
        return ret;

    return 0;
}

static void scene_freep(struct ngl_scene **sp)
{
    struct ngl_scene *s = *sp;
    if (!s)
        return;
    detach_root(s);
    ngli_freep(sp);
}

struct ngl_scene *ngl_scene_create(void)
{
    struct ngl_scene *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->rc = NGLI_RC_CREATE(scene_freep);
    return s;
}

struct ngl_scene_params ngl_scene_default_params(struct ngl_node *root)
{
    const struct ngl_scene_params params = {
        .root         = root,
        .duration     = 30.0,
        .framerate    = {60, 1},
        .aspect_ratio = {1, 1},
    };
    return params;
}

struct ngl_scene *ngl_scene_ref(struct ngl_scene *s)
{
    return NGLI_RC_REF(s);
}

int ngl_scene_init(struct ngl_scene *s, const struct ngl_scene_params *params)
{
    if (!params->root) {
        LOG(ERROR, "cannot initialize a scene without root node");
        return NGL_ERROR_INVALID_ARG;
    }
    if (params->duration < 0.0) {
        LOG(ERROR, "invalid scene duration %g", params->duration);
        return NGL_ERROR_INVALID_ARG;
    }
    if (params->framerate[0] <= 0 || params->framerate[1] <= 0) {
        LOG(ERROR, "invalid framerate %d/%d", NGLI_ARG_VEC2(params->framerate));
        return NGL_ERROR_INVALID_ARG;
    }
    if (params->aspect_ratio[0] <= 0 || params->aspect_ratio[1] <= 0) {
        LOG(ERROR, "invalid aspect ratio %d:%d", NGLI_ARG_VEC2(params->aspect_ratio));
        return NGL_ERROR_INVALID_ARG;
    }

    if (s->params.root && s->params.root->ctx) {
        LOG(ERROR, "the node graph currently held within the scene is associated with a rendering context");
        return NGL_ERROR_INVALID_USAGE;
    }

    detach_root(s);
    s->params = *params;
    return attach_root(s, s->params.root);
}

int ngl_scene_init_from_str(struct ngl_scene *s, const char *str)
{
    return ngli_scene_deserialize(s, str);
}

const struct ngl_scene_params *ngl_scene_get_params(const struct ngl_scene *s)
{
    return &s->params;
}

char *ngl_scene_serialize(const struct ngl_scene *s)
{
    return ngli_scene_serialize(s);
}

char *ngl_scene_dot(const struct ngl_scene *s)
{
    return ngli_scene_dot(s);
}

void ngl_scene_unrefp(struct ngl_scene **sp)
{
    struct ngl_scene *s = *sp;
    if (!s)
        return;
    NGLI_RC_UNREFP(sp);
}
