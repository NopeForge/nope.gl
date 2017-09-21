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

#include <string.h>
#include <inttypes.h>

#include "log.h"
#include "hmap.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"
#include "utils.h"

const struct node_param *ngli_params_find(const struct node_param *params, const char *key)
{
    if (!params)
        return NULL;
    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];
        if (!strcmp(par->key, key))
            return par;
    }
    return NULL;
}

void ngli_params_bstr_print_val(struct bstr *b, uint8_t *base_ptr, const struct node_param *par)
{
    switch (par->type) {
        case PARAM_TYPE_DBL: {
            const double v = *(double *)(base_ptr + par->offset);
            ngli_bstr_print(b, "%g", v);
            break;
        }
        case PARAM_TYPE_INT: {
            const int v = *(int *)(base_ptr + par->offset);
            ngli_bstr_print(b, "%d", v);
            break;
        }
        case PARAM_TYPE_I64: {
            const int64_t v = *(int64_t *)(base_ptr + par->offset);
            ngli_bstr_print(b, "%" PRId64, v);
            break;
        }
        case PARAM_TYPE_VEC2: {
            const float *v = (const float *)(base_ptr + par->offset);
            ngli_bstr_print(b, "(%g,%g)", v[0], v[1]);
            break;
        }
        case PARAM_TYPE_VEC3: {
            const float *v = (const float *)(base_ptr + par->offset);
            ngli_bstr_print(b, "(%g,%g,%g)", v[0], v[1], v[2]);
            break;
        }
        case PARAM_TYPE_VEC4: {
            const float *v = (const float *)(base_ptr + par->offset);
            ngli_bstr_print(b, "(%g,%g,%g,%g)", v[0], v[1], v[2], v[3]);
            break;
        }
        case PARAM_TYPE_STR: {
            const char *s = *(const char **)(base_ptr + par->offset);
            if (!s)
                ngli_bstr_print(b, "\"\"");
            else if (strchr(s, '/')) // assume a file and display only basename
                ngli_bstr_print(b, "\"%s\"", strrchr(s, '/') + 1);
            else
                ngli_bstr_print(b, "\"%s\"", s);
            break;
        }
        case PARAM_TYPE_DBLLIST: {
            uint8_t *elems_p = base_ptr + par->offset;
            uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(double *);
            const double *elems = *(double **)elems_p;
            const int nb_elems = *(int *)nb_elems_p;
            for (int i = 0; i < nb_elems; i++)
                ngli_bstr_print(b, "%s%g", i ? "," : "", elems[i]);
            break;
        }
    }
}

static int allowed_node(const struct ngl_node *node, const int *allowed_ids)
{
    if (!allowed_ids)
        return 1;
    const int id = node->class->id;
    for (int i = 0; allowed_ids[i] != -1; i++)
        if (id == allowed_ids[i])
            return 1;
    return 0;
}

static void node_hmap_free(void *user_arg, void *data)
{
    struct ngl_node *node = data;
    ngl_node_unrefp(&node);
}

int ngli_params_set(uint8_t *base_ptr, const struct node_param *par, va_list *ap)
{
    uint8_t *dstp = base_ptr + par->offset;

    switch (par->type) {
        case PARAM_TYPE_INT: {
            int v = va_arg(*ap, int);
            LOG(VERBOSE, "set %s to %d", par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_I64: {
            int64_t v = va_arg(*ap, int64_t);
            LOG(VERBOSE, "set %s to %"PRId64, par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_DBL: {
            double v = va_arg(*ap, double);
            LOG(VERBOSE, "set %s to %g", par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_STR: {
            const char *s = ngli_strdup(va_arg(*ap, const char *));
            if (!s)
                return -1;
            free(*(char **)dstp);
            LOG(VERBOSE, "set %s to \"%s\"", par->key, s);
            memcpy(dstp, &s, sizeof(s));
            break;
        }
        case PARAM_TYPE_VEC2: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE, "set %s to (%g,%g)", par->key, v[0], v[1]);
            memcpy(dstp, v, 2 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_VEC3: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE, "set %s to (%g,%g,%g)", par->key, v[0], v[1], v[2]);
            memcpy(dstp, v, 3 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_VEC4: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE, "set %s to (%g,%g,%g,%g)", par->key, v[0], v[1], v[2], v[3]);
            memcpy(dstp, v, 4 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_NODE: {
            struct ngl_node *node = va_arg(*ap, struct ngl_node *);
            if (!allowed_node(node, par->node_types)) {
                LOG(ERROR, "%s (%s) is not an allowed type for %s",
                    node->name, node->class->name, par->key);
                return -1;
            }
            ngl_node_ref(node);
            LOG(VERBOSE, "set %s to %s", par->key, node->name);
            memcpy(dstp, &node, sizeof(node));
            break;
        }
        case PARAM_TYPE_NODEDICT: {
            int ret;
            const char *name = va_arg(*ap, const char *);
            struct ngl_node *node = va_arg(*ap, struct ngl_node *);
            if (node && !allowed_node(node, par->node_types)) {
                LOG(ERROR, "%s (%s) is not an allowed type for %s",
                    node->name, node->class->name, par->key);
                return -1;
            }
            LOG(VERBOSE, "set %s to (%s,%p)", par->key, name, node);
            struct hmap **hmapp = (struct hmap **)dstp;
            if (!*hmapp) {
                *hmapp = ngli_hmap_create();
                if (!*hmapp)
                    return -1;
                ngli_hmap_set_free(*hmapp, node_hmap_free, NULL);
            }

            ret = ngli_hmap_set(*hmapp, name, node ? ngl_node_ref(node) : NULL);
            if (ret < 0)
                return ret;
            break;
        }

    }
    return 0;
}

int ngli_params_vset(uint8_t *base_ptr, const struct node_param *par, ...)
{
    va_list ap;
    va_start(ap, par);
    int ret = ngli_params_set(base_ptr, par, &ap);
    va_end(ap);
    return ret;
}

int ngli_params_set_constructors(uint8_t *base_ptr, const struct node_param *params, va_list *ap)
{
    if (!params)
        return 0;

    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        if (par->flags & PARAM_FLAG_CONSTRUCTOR) {
            int ret = ngli_params_set(base_ptr, par, ap);
            if (ret < 0)
                return ret;
        } else {
            break; /* assume all constructors are at the start */
        }
    }
    return 0;
}

int ngli_params_set_defaults(uint8_t *base_ptr, const struct node_param *params)
{
    int last_offset = 0;

    if (!params)
        return 0;

    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        // The offset must be monotically incrementing to make the reset of the
        // non-params much simpler in the node uninit.
        if (par->offset < last_offset) {
            LOG(ERROR, "offset inconsistency detected around %s", par->key);
            ngli_assert(0);
        }
        last_offset = par->offset;

        if (!(par->flags & PARAM_FLAG_CONSTRUCTOR)) {
            switch (par->type) {
                case PARAM_TYPE_INT:
                case PARAM_TYPE_I64:
                    ngli_params_vset(base_ptr, par, par->def_value.i64);
                    break;
                case PARAM_TYPE_DBL:
                    ngli_params_vset(base_ptr, par, par->def_value.dbl);
                    break;
                case PARAM_TYPE_STR:
                    ngli_params_vset(base_ptr, par, par->def_value.str);
                    break;
                case PARAM_TYPE_VEC2:
                case PARAM_TYPE_VEC3:
                case PARAM_TYPE_VEC4:
                    ngli_params_vset(base_ptr, par, par->def_value.vec);
                    break;
            }
        }
    }
    return 0;
}

int ngli_params_add(uint8_t *base_ptr, const struct node_param *par,
                    int nb_elems, void *elems)
{
    LOG(VERBOSE, "add %d elems to %s", nb_elems, par->key);
    switch (par->type) {
        case PARAM_TYPE_NODELIST: {
            uint8_t *cur_elems_p = base_ptr + par->offset;
            uint8_t *nb_cur_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
            struct ngl_node **cur_elems = *(struct ngl_node ***)cur_elems_p;
            const int nb_cur_elems = *(int *)nb_cur_elems_p;
            const int nb_new_elems = nb_cur_elems + nb_elems;
            struct ngl_node **new_elems = realloc(cur_elems, nb_new_elems * sizeof(*new_elems));
            struct ngl_node **new_elems_addp = new_elems + nb_cur_elems;
            struct ngl_node **add_elems = elems;

            if (!new_elems)
                return -1;

            *(struct ngl_node ***)cur_elems_p = new_elems;
            for (int i = 0; i < nb_elems; i++) {
                const struct ngl_node *e = add_elems[i];
                if (!allowed_node(e, par->node_types)) {
                    LOG(ERROR, "%s (%s) is not an allowed type for %s list",
                        e->name, e->class->name, par->key);
                    return -1;
                }
            }
            for (int i = 0; i < nb_elems; i++) {
                struct ngl_node *e = add_elems[i];
                new_elems_addp[i] = ngl_node_ref(e);
            }
            *(int *)nb_cur_elems_p = nb_new_elems;
            break;
        }
        case PARAM_TYPE_DBLLIST: {
            uint8_t *cur_elems_p = base_ptr + par->offset;
            uint8_t *nb_cur_elems_p = base_ptr + par->offset + sizeof(double *);
            double *cur_elems = *(double **)cur_elems_p;
            const int nb_cur_elems = *(int *)nb_cur_elems_p;
            const int nb_new_elems = nb_cur_elems + nb_elems;
            double *new_elems = realloc(cur_elems, nb_new_elems * sizeof(*new_elems));
            double *new_elems_addp = new_elems + nb_cur_elems;
            double *add_elems = elems;

            if (!new_elems)
                return -1;
            for (int i = 0; i < nb_elems; i++)
                new_elems_addp[i] = add_elems[i];
            *(double **)cur_elems_p = new_elems;
            *(int *)nb_cur_elems_p = nb_new_elems;
            break;
        }
        default:
            LOG(ERROR, "parameter %s is not a list", par->key);
            return -1;
    }
    return 0;
}

void ngli_params_free(uint8_t *base_ptr, const struct node_param *params)
{
    if (!params)
        return;

    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];
        uint8_t *parp = base_ptr + par->offset;

        switch (par->type) {
            case PARAM_TYPE_STR: {
                char *s = *(char **)parp;
                free(s);
                break;
            }
            case PARAM_TYPE_NODE: {
                uint8_t *node_p = base_ptr + par->offset;
                struct ngl_node *node = *(struct ngl_node **)node_p;
                ngl_node_unrefp(&node);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                int j;
                uint8_t *elems_p = base_ptr + par->offset;
                uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
                struct ngl_node **elems = *(struct ngl_node ***)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                for (j = 0; j < nb_elems; j++)
                    ngl_node_unrefp(&elems[j]);
                free(elems);
                break;
            }
            case PARAM_TYPE_DBLLIST: {
                uint8_t *elems_p = base_ptr + par->offset;
                double *elems = *(double **)elems_p;
                free(elems);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap **hmapp = (struct hmap **)(base_ptr + par->offset);
                ngli_hmap_freep(hmapp);
                break;
            }
        }
    }
}
