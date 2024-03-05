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

#include <string.h>

#include "darray.h"
#include "hmap.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "params.h"
#include "utils.h"

NGLI_RC_CHECK_STRUCT(ngl_scene);

typedef int (*children_func_type)(void *user_arg, struct ngl_node *parent, struct ngl_node *node);

/*
 * Apply a function on all children by walking through them. This is useful when
 * node->children is not yet initialized (or confirmed to be complete yet).
 */
static int children_apply_func(children_func_type func, void *user_arg, struct ngl_node *node)
{
    uint8_t *base_ptr = node->opts;
    const struct node_param *par = node->cls->params;

    if (!par)
        return 0;

    while (par->key) {
        uint8_t *parp = base_ptr + par->offset;

        if (par->type == NGLI_PARAM_TYPE_NODE || (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)) {
            struct ngl_node *child = *(struct ngl_node **)parp;
            if (child) {
                int ret = func(user_arg, node, child);
                if (ret < 0)
                    return ret;
            }
        } else if (par->type == NGLI_PARAM_TYPE_NODELIST) {
            uint8_t *elems_p = parp;
            uint8_t *nb_elems_p = parp + sizeof(struct ngl_node **);
            struct ngl_node **elems = *(struct ngl_node ***)elems_p;
            const size_t nb_elems = *(size_t *)nb_elems_p;
            for (size_t i = 0; i < nb_elems; i++) {
                struct ngl_node *child = elems[i];
                int ret = func(user_arg, node, child);
                if (ret < 0)
                    return ret;
            }
        } else if (par->type == NGLI_PARAM_TYPE_NODEDICT) {
            struct hmap *hmap = *(struct hmap **)parp;
            if (hmap) {
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry))) {
                    struct ngl_node *child = entry->data;
                    int ret = func(user_arg, node, child);
                    if (ret < 0)
                        return ret;
                }
            }
        }
        par++;
    }

    return 0;
}

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

    int ret = children_apply_func(reset_nodes, s, node);
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

    ngli_darray_reset(&s->nodes);

    ngli_darray_reset(&s->files);
    ngli_darray_reset(&s->files_par);

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

        int ret = children_apply_func(setup_nodes, s, node);
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

static int track_nodes(struct hmap *nodes_set, struct ngl_node *node)
{
    const uint64_t key = (uint64_t)(uintptr_t)node;
    if (!ngli_hmap_get_u64(nodes_set, key)) {
        int ret = ngli_hmap_set_u64(nodes_set, key, node);
        if (ret < 0)
            return ret;
    }

    struct darray *children_array = &node->children;
    struct ngl_node **children = ngli_darray_data(children_array);
    for (size_t i = 0; i < ngli_darray_count(children_array); i++) {
        int ret = track_nodes(nodes_set, children[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int build_nodes_set(struct ngl_scene *s)
{
    struct hmap *nodes_set = ngli_hmap_create(NGLI_HMAP_TYPE_U64);
    if (!nodes_set)
        return NGL_ERROR_MEMORY;

    int ret = track_nodes(nodes_set, s->params.root);
    if (ret < 0)
        goto end;

    // Transfer the nodes set to a flat darray set of nodes
    ngli_darray_init(&s->nodes, sizeof(struct ngl_node *), 0);
    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(nodes_set, entry))) {
        const struct ngl_node *node = entry->data;
        if (!ngli_darray_push(&s->nodes, &node)) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }
    }

end:
    ngli_hmap_freep(&nodes_set);
    return ret;
}

static int track_files(struct ngl_scene *s)
{
    ngli_darray_init(&s->files, sizeof(char *), 0);
    ngli_darray_init(&s->files_par, sizeof(uint8_t *), 0);

    const struct ngl_node **nodes = ngli_darray_data(&s->nodes);
    for (size_t i = 0; i < ngli_darray_count(&s->nodes); i++) {
        const struct ngl_node *node = nodes[i];
        const uint8_t *base_ptr = node->opts;

        const struct node_param *params = node->cls->params;
        if (!params)
            continue;

        for (size_t j = 0; params[j].key; j++) {
            const struct node_param *par = &params[j];
            const uint8_t *parp = base_ptr + par->offset;
            if (par->flags & NGLI_PARAM_FLAG_FILEPATH) {
                const char *str = *(char **)parp;
                if (!str)
                    continue;
                if (!ngli_darray_push(&s->files, &str))
                    return NGL_ERROR_MEMORY;
                if (!ngli_darray_push(&s->files_par, &parp))
                    return NGL_ERROR_MEMORY;
            }
        }
    }

    return 0;
}

static int check_nodes_params_sanity(const struct darray *nodes_array)
{
    const struct ngl_node **nodes = ngli_darray_data(nodes_array);
    for (size_t i = 0; i < ngli_darray_count(nodes_array); i++) {
        const struct ngl_node *node = nodes[i];

        const uint8_t *base_ptr = node->opts;
        const struct node_param *par = node->cls->params;

        if (!par)
            return 0;

        while (par->key) {
            const void *p = base_ptr + par->offset;
            if ((par->flags & NGLI_PARAM_FLAG_NON_NULL) && !*(uint8_t **)p) {
                LOG(ERROR, "%s: %s parameter can not be null", node->label, par->key);
                return NGL_ERROR_INVALID_ARG;
            }
            par++;
        }
    }

    return 0;
}

static int attach_root(struct ngl_scene *s, struct ngl_node *node)
{
    s->params.root = ngl_node_ref(node);

    int ret = setup_nodes(s, NULL, s->params.root);
    if (ret < 0)
        return ret;

    ret = build_nodes_set(s);
    if (ret < 0)
        return ret;

    ret = check_nodes_params_sanity(&s->nodes);
    if (ret < 0)
        return ret;

    ret = track_files(s);
    if (ret < 0)
        return ret;

    return 0;
}

int ngl_scene_get_filepaths(struct ngl_scene *s, char ***filepathsp, size_t *nb_filepathsp)
{
    *filepathsp = NULL;
    *nb_filepathsp = 0;

    if (!s->params.root)
        return NGL_ERROR_INVALID_USAGE;

    *filepathsp = ngli_darray_data(&s->files);
    *nb_filepathsp = ngli_darray_count(&s->files);
    return 0;
}

static void update_filepath_ref(struct ngl_scene *s, size_t index, const char *str)
{
    const char **filep = ngli_darray_get(&s->files, index);
    *filep = str;
}

void ngli_scene_update_filepath_ref(struct ngl_node *node, const struct node_param *par)
{
    struct ngl_scene *s = node->scene;
    const uint8_t **pars = ngli_darray_data(&s->files_par);
    for (size_t i = 0; i < ngli_darray_count(&s->files_par); i++) {
        const uint8_t *base_ptr = node->opts;
        const uint8_t *parp = base_ptr + par->offset;
        if (pars[i] == parp) {
            char *str = *(char **)parp;
            update_filepath_ref(s, i, str);
            return;
        }
    }
    ngli_assert(0);
}

int ngl_scene_update_filepath(struct ngl_scene *s, size_t index, const char *filepath)
{
    if (s->params.root->ctx) {
        LOG(ERROR, "the file paths cannot be updated when a rendering context is associated with the scene");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (index >= ngli_darray_count(&s->files))
        return NGL_ERROR_INVALID_ARG;

    /* Update the node parameter with the new value */
    char *new_str = ngli_strdup(filepath);
    if (!new_str)
        return NGL_ERROR_MEMORY;
    uint8_t *parp = ngli_darray_get(&s->files_par, index);
    char **dstp = *(char ***)parp;
    ngli_freep(dstp);
    *dstp = new_str;

    update_filepath_ref(s, index, new_str);

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

static const struct livectl *get_internal_livectl(const struct ngl_node *node)
{
    const uint8_t *base_ptr = node->opts;
    const struct livectl *ctl = (struct livectl *)(base_ptr + node->cls->livectl_offset);
    return ctl;
}

static int find_livectls(struct ngl_scene *scene, struct hmap *hm)
{
    const struct ngl_node **nodes = ngli_darray_data(&scene->nodes);
    for (size_t i = 0; i < ngli_darray_count(&scene->nodes); i++) {
        const struct ngl_node *node = nodes[i];
        if (!(node->cls->flags & NGLI_NODE_FLAG_LIVECTL))
            continue;

        const struct livectl *ref_ctl = get_internal_livectl(node);
        if (!ref_ctl->id)
            continue;

        const struct ngl_node *ctl_node = ngli_hmap_get_str(hm, ref_ctl->id);
        if (ctl_node) {
            if (ctl_node != node) {
                LOG(ERROR, "duplicated live control with name \"%s\"", ref_ctl->id);
                return NGL_ERROR_INVALID_USAGE;
            }
            ngli_assert(0); // scene->nodes is a set so each node is supposed to be present only once
        }

        int ret = ngli_hmap_set_str(hm, ref_ctl->id, (void *)node);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ngl_livectls_get(struct ngl_scene *scene, size_t *nb_livectlsp, struct ngl_livectl **livectlsp)
{
    struct ngl_livectl *ctls = NULL;
    *livectlsp = NULL;
    *nb_livectlsp = 0;

    if (!scene->params.root)
        return 0;

    struct hmap *livectls_index = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!livectls_index)
        return NGL_ERROR_MEMORY;

    int ret = find_livectls(scene, livectls_index);
    if (ret < 0)
        goto end;

    const size_t nb = ngli_hmap_count(livectls_index);
    if (!nb)
        goto end;

    /* +1 so that we know when to stop in ngli_node_livectls_freep() */
    ctls = ngli_calloc(nb + 1, sizeof(*ctls));
    if (!ctls) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    /*
     * Transfer internal live controls (struct livectl) to public live controls
     * (struct ngl_livectl), with independant ownership (ref counting the node
     * and duplicating memory when needed).
     */
    size_t i = 0;
    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(livectls_index, entry))) {
        struct ngl_node *node = entry->data;

        struct ngl_livectl *ctl = &ctls[i++];
        ctl->node_type = node->cls->id;
        ctl->node = ngl_node_ref(node);

        const struct livectl *ref_ctl = get_internal_livectl(node);
        ctl->id = ngli_strdup(ref_ctl->id);
        if (!ctl->id) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }
        memcpy(&ctl->val, &ref_ctl->val, sizeof(ctl->val));
        memcpy(&ctl->min, &ref_ctl->min, sizeof(ctl->min));
        memcpy(&ctl->max, &ref_ctl->max, sizeof(ctl->max));

        if (node->cls->id == NGL_NODE_TEXT) {
            ngli_assert(ctl->val.s);
            ngli_assert(!ctl->min.s && !ctl->max.s);
            ctl->val.s = ngli_strdup(ctl->val.s);
            if (!ctl->val.s) {
                ret = NGL_ERROR_MEMORY;
                goto end;
            }
        }
    }

    *livectlsp = ctls;
    *nb_livectlsp = nb;

end:
    if (ret < 0)
        ngl_livectls_freep(&ctls);
    ngli_hmap_freep(&livectls_index);
    return ret;
}

void ngl_livectls_freep(struct ngl_livectl **livectlsp)
{
    struct ngl_livectl *livectls = *livectlsp;
    if (!livectls)
        return;
    for (size_t i = 0; livectls[i].node; i++) {
        struct ngl_livectl *ctl = &livectls[i];
        if (livectls[i].node->cls->id == NGL_NODE_TEXT)
            ngli_freep(&livectls[i].val.s);
        ngl_node_unrefp(&ctl->node);
        ngli_freep(&ctl->id);
    }
    ngli_freep(livectlsp);
}

void ngl_scene_unrefp(struct ngl_scene **sp)
{
    struct ngl_scene *s = *sp;
    if (!s)
        return;
    NGLI_RC_UNREFP(sp);
}
