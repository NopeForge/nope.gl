/*
 * Copyright 2017 GoPro Inc.
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

#include <inttypes.h>
#include <string.h>

#include "bstr.h"
#include "hmap.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "nodegl.h"
#include "utils.h"

extern const struct node_param ngli_base_node_params[];

static void free_func(void *arg, void *data)
{
    ngli_free(data);
}

static int register_node(struct hmap *nlist,
                          const struct ngl_node *node)
{
    char key[32];
    int ret = snprintf(key, sizeof(key), "%p", node);
    if (ret < 0)
        return ret;
    char *val = ngli_asprintf("%x", ngli_hmap_count(nlist));
    if (!val)
        return -1;
    ret = ngli_hmap_set(nlist, key, val);
    if (ret < 0)
        ngli_free(val);
    return ret;
}

static const char *get_node_id(const struct hmap *nlist,
                               const struct ngl_node *node)
{
    char key[32];
    (void)snprintf(key, sizeof(key), "%p", node);
    const char *val = ngli_hmap_get(nlist, key);
    return val;
}

#define DECLARE_FLT_PRINT_FUNCS(type, nbit, shift_exp, z)               \
static void print_##type(struct bstr *b, type f)                        \
{                                                                       \
    const union { uint##nbit##_t i; type f; } u = {.f = f};             \
    const uint##nbit##_t v = u.i;                                       \
    const int sign = v >> (nbit - 1);                                   \
    const uint##nbit##_t exp_mask = (1 << (nbit - shift_exp - 1)) - 1;  \
    const uint##nbit##_t exp  = v >> shift_exp & exp_mask;              \
    const uint##nbit##_t mant = v & ((1ULL << shift_exp) - 1);          \
    ngli_bstr_print(b, "%s%" PRIX##nbit "%c%" PRIX##nbit,               \
                    sign ? "-" : "", exp, z, mant);                     \
}                                                                       \
                                                                        \
static void print_##type##s(struct bstr *b, int n, const type *f)       \
{                                                                       \
    for (int i = 0; i < n; i++) {                                       \
        ngli_bstr_print(b, "%s", i ? "," : "");                         \
        print_##type(b, f[i]);                                          \
    }                                                                   \
}

DECLARE_FLT_PRINT_FUNCS(float,  32, 23, 'z')
DECLARE_FLT_PRINT_FUNCS(double, 64, 52, 'Z')

static void serialize_options(struct hmap *nlist,
                              struct bstr *b,
                              const struct ngl_node *node,
                              uint8_t *priv,
                              const struct node_param *p)
{
    while (p && p->key) {
        const int constructor = p->flags & PARAM_FLAG_CONSTRUCTOR;
        switch (p->type) {
            case PARAM_TYPE_SELECT: {
                const int v = *(int *)(priv + p->offset);
                const char *s = ngli_params_get_select_str(p->choices->consts, v);
                ngli_assert(s);
                if (constructor)
                    ngli_bstr_print(b, " %s", s);
                else if (v != p->def_value.i64)
                    ngli_bstr_print(b, " %s:%s", p->key, s);
                break;
            }
            case PARAM_TYPE_FLAGS: {
                const int v = *(int *)(priv + p->offset);
                char *s = ngli_params_get_flags_str(p->choices->consts, v);
                if (!s) {
                    LOG(ERROR, "unable to allocate param flags string");
                    return; // XXX: code return
                }
                ngli_assert(*s);
                if (constructor)
                    ngli_bstr_print(b, " %s", s);
                else if (v != p->def_value.i64)
                    ngli_bstr_print(b, " %s:%s", p->key, s);
                ngli_free(s);
                break;
            }
            case PARAM_TYPE_BOOL:
            case PARAM_TYPE_INT: {
                const int v = *(int *)(priv + p->offset);
                if (constructor)
                    ngli_bstr_print(b, " %d", v);
                else if (v != p->def_value.i64)
                    ngli_bstr_print(b, " %s:%d", p->key, v);
                break;
            }
            case PARAM_TYPE_I64: {
                const int64_t v = *(int64_t *)(priv + p->offset);
                if (constructor)
                    ngli_bstr_print(b, " %"PRId64, v);
                else if (v != p->def_value.i64)
                    ngli_bstr_print(b, " %s:%"PRId64, p->key, v);
                break;
            }
            case PARAM_TYPE_DBL: {
                const double v = *(double *)(priv + p->offset);
                if (constructor) {
                    ngli_bstr_print(b, " ");
                    print_double(b, v);
                } else if (v != p->def_value.dbl) {
                    ngli_bstr_print(b, " %s:", p->key);
                    print_double(b, v);
                }
                break;
            }
            case PARAM_TYPE_RATIONAL: {
                const int *r = (int *)(priv + p->offset);
                if (constructor)
                    ngli_bstr_print(b, " %d/%d", r[0], r[1]);
                else if (memcmp(r, p->def_value.r, sizeof(p->def_value.r)))
                    ngli_bstr_print(b, " %s:%d/%d", p->key, r[0], r[1]);
                break;
            }
            case PARAM_TYPE_STR: {
                const char *s = *(char **)(priv + p->offset);
                if (!s || (p->def_value.str && !strcmp(s, p->def_value.str)))
                    break;
                if (!strcmp(p->key, "label") &&
                    ngli_is_default_label(node->class->name, s))
                    break;
                if (!constructor)
                    ngli_bstr_print(b, " %s:", p->key);
                else
                    ngli_bstr_print(b, " ");
                for (int i = 0; s[i]; i++)
                    if (s[i] >= '!' && s[i] <= '~' && s[i] != '%')
                        ngli_bstr_print(b, "%c", s[i]);
                    else
                        ngli_bstr_print(b, "%%%02x", s[i] & 0xff);
                break;
            }
            case PARAM_TYPE_DATA: {
                const uint8_t *data = *(uint8_t **)(priv + p->offset);
                const int size = *(int *)(priv + p->offset + sizeof(uint8_t *));
                if (!data || !size)
                    break;
                if (!constructor)
                    ngli_bstr_print(b, " %s:%d,", p->key, size);
                else
                    ngli_bstr_print(b, " %d,", size);
                for (int i = 0; i < size; i++) {
                    ngli_bstr_print(b, "%02x", data[i]);
                }
                break;
            }
            case PARAM_TYPE_VEC2:
            case PARAM_TYPE_VEC3:
            case PARAM_TYPE_VEC4: {
                const float *v = (float *)(priv + p->offset);
                const int n = p->type - PARAM_TYPE_VEC2 + 2;
                if (constructor) {
                    ngli_bstr_print(b, " ");
                    print_floats(b, n, v);
                } else if (memcmp(v, p->def_value.vec, n * sizeof(*v))) {
                    ngli_bstr_print(b, " %s:", p->key);
                    print_floats(b, n, v);
                }
                break;
            }
            case PARAM_TYPE_MAT4: {
                const float *m = (float *)(priv + p->offset);
                if (constructor) {
                    ngli_bstr_print(b, " ");
                    print_floats(b, 16, m);
                } else if (memcmp(m, p->def_value.mat, 16 * sizeof(*m))) {
                    ngli_bstr_print(b, " %s:", p->key);
                    print_floats(b, 16, m);
                }
                break;
            }
            case PARAM_TYPE_NODE: {
                const struct ngl_node *node = *(struct ngl_node **)(priv + p->offset);
                if (!node)
                    break;
                const char *node_id = get_node_id(nlist, node);
                if (constructor)
                    ngli_bstr_print(b, " %s", node_id);
                else if (node)
                    ngli_bstr_print(b, " %s:%s", p->key, node_id);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **nodes = *(struct ngl_node ***)(priv + p->offset);
                const int nb_nodes = *(int *)(priv + p->offset + sizeof(struct ngl_node **));
                if (!nb_nodes)
                    break;
                if (constructor)
                    ngli_bstr_print(b, " ");
                else
                    ngli_bstr_print(b, " %s:", p->key);
                for (int i = 0; i < nb_nodes; i++) {
                    const char *node_id = get_node_id(nlist, nodes[i]);
                    ngli_bstr_print(b, "%s%s", i ? "," : "", node_id);
                }
                break;
            }
            case PARAM_TYPE_DBLLIST: {
                uint8_t *elems_p = priv + p->offset;
                uint8_t *nb_elems_p = priv + p->offset + sizeof(double *);
                const double *elems = *(double **)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                if (!nb_elems)
                    break;
                if (constructor)
                    ngli_bstr_print(b, " ");
                else
                    ngli_bstr_print(b, " %s:", p->key);
                print_doubles(b, nb_elems, elems);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(priv + p->offset);
                const int nb_nodes = hmap ? ngli_hmap_count(hmap) : 0;
                if (!nb_nodes)
                    break;
                if (constructor)
                    ngli_bstr_print(b, " ");
                else
                    ngli_bstr_print(b, " %s:", p->key);
                int i = 0;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry))) {
                    const char *node_id = get_node_id(nlist, entry->data);
                    ngli_bstr_print(b, "%s%s=%s", i ? "," : "", entry->key, node_id);
                    i++;
                }
                break;
            }
            default:
                LOG(ERROR, "cannot serialize %s: unsupported parameter type", p->key);
        }
        p++;
    }
}

static int serialize(struct hmap *nlist,
                     struct bstr *b,
                     const struct ngl_node *node);

static int serialize_children(struct hmap *nlist,
                               struct bstr *b,
                               const struct ngl_node *node,
                               uint8_t *priv,
                               const struct node_param *p)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child) {
                    int ret = serialize(nlist, b, child);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                for (int i = 0; i < nb_children; i++) {
                    int ret = serialize(nlist, b, children[i]);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(priv + p->offset);
                if (!hmap)
                    break;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry))) {
                    int ret = serialize(nlist, b, entry->data);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
        }
        p++;
    }
    return 0;
}

static int serialize(struct hmap *nlist,
                     struct bstr *b,
                     const struct ngl_node *node)
{
    if (get_node_id(nlist, node))
        return 0;

    int ret;

    if ((ret = serialize_children(nlist, b, node, (uint8_t *)node, ngli_base_node_params)) < 0 ||
        (ret = serialize_children(nlist, b, node, node->priv_data, node->class->params)) < 0)
        return ret;

    const uint32_t tag = node->class->id;
    ngli_bstr_print(b, "%c%c%c%c",
                    tag >> 24 & 0xff,
                    tag >> 16 & 0xff,
                    tag >>  8 & 0xff,
                    tag       & 0xff);
    serialize_options(nlist, b, node, node->priv_data, node->class->params);
    serialize_options(nlist, b, node, (uint8_t *)node, ngli_base_node_params);
    ngli_bstr_print(b, "\n");

    return register_node(nlist, node);
}

char *ngl_node_serialize(const struct ngl_node *node)
{
    char *s = NULL;
    struct hmap *nlist = ngli_hmap_create();
    struct bstr *b = ngli_bstr_create();
    if (!nlist || !b)
        goto end;

    ngli_hmap_set_free(nlist, free_func, NULL);
    ngli_bstr_print(b, "# Node.GL v%d.%d.%d\n",
                    NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    if (serialize(nlist, b, node) < 0)
        goto end;
    s = ngli_bstr_strdup(b);

end:
    ngli_hmap_freep(&nlist);
    ngli_bstr_freep(&b);
    return s;
}
