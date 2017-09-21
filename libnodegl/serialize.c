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
#include "ndict.h"
#include "nodes.h"
#include "nodegl.h"
#include "utils.h"

extern const struct node_param ngli_base_node_params[];

static void free_func(void *arg, void *data)
{
    free(data);
}

static void register_node(struct hmap *nlist,
                          const struct ngl_node *node)
{
    char key[32];
    (void)snprintf(key, sizeof(key), "%p", node);
    char *val = ngli_asprintf("%x", ngli_hmap_count(nlist));
    (void)ngli_hmap_set(nlist, key, val);
}

static const char *get_node_id(const struct hmap *nlist,
                               const struct ngl_node *node)
{
    char key[32];
    (void)snprintf(key, sizeof(key), "%p", node);
    const char *val = ngli_hmap_get(nlist, key);
    return val;
}

static const float zvec[4] = {0};

static void serialize_options(struct hmap *sctx,
                              struct bstr *b,
                              const struct ngl_node *node,
                              uint8_t *priv,
                              const struct node_param *p)
{
    while (p && p->key) {
        const int constructor = p->flags & PARAM_FLAG_CONSTRUCTOR;
        switch (p->type) {
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
                if (constructor)
                    ngli_bstr_print(b, " %a", v);
                else if (v != p->def_value.dbl)
                    ngli_bstr_print(b, " %s:%a", p->key, v);
                break;
            }
            case PARAM_TYPE_STR: {
                const char *s = *(char **)(priv + p->offset);
                if (!s || (p->def_value.str && !strcmp(s, p->def_value.str)))
                    break;
                if (!strcmp(p->key, "name")) {
                    char *auto_name = ngli_node_default_name(node->class->name);
                    const int is_default = !strcmp(s, auto_name);
                    free(auto_name);
                    if (is_default)
                        break;
                }
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
            case PARAM_TYPE_VEC2: {
                const float *v = (float *)(priv + p->offset);
                if (constructor)
                    ngli_bstr_print(b, " %a,%a", v[0], v[1]);
                else if (memcmp(v, zvec, 2 * sizeof(*v)))
                    ngli_bstr_print(b, " %s:%a,%a", p->key, v[0], v[1]);
                break;
            }
            case PARAM_TYPE_VEC3: {
                const float *v = (float *)(priv + p->offset);
                if (constructor)
                    ngli_bstr_print(b, " %a,%a,%a", v[0], v[1], v[2]);
                else if (memcmp(v, zvec, 3 * sizeof(*v)))
                    ngli_bstr_print(b, " %s:%a,%a,%a", p->key, v[0], v[1], v[2]);
                break;
            }
            case PARAM_TYPE_VEC4: {
                const float *v = (float *)(priv + p->offset);
                if (constructor)
                    ngli_bstr_print(b, " %a,%a,%a,%a", v[0], v[1], v[2], v[3]);
                else if (memcmp(v, zvec, 4 * sizeof(*v)))
                    ngli_bstr_print(b, " %s:%a,%a,%a,%a", p->key, v[0], v[1], v[2], v[3]);
                break;
            }
            case PARAM_TYPE_NODE: {
                const struct ngl_node *node = *(struct ngl_node **)(priv + p->offset);
                if (!node)
                    break;
                const char *node_id = get_node_id(sctx, node);
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
                    const char *node_id = get_node_id(sctx, nodes[i]);
                    ngli_bstr_print(b, "%s%s", i ? "," : "", node_id);
                }
                break;
            }
            case PARAM_TYPE_DBLLIST: {
                uint8_t *elems_p = priv + p->offset;
                uint8_t *nb_elems_p = priv + p->offset + sizeof(double *);
                const double *elems = *(double **)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                if (nb_elems) {
                    ngli_bstr_print(b, " ");
                    for (int i = 0; i < nb_elems; i++)
                        ngli_bstr_print(b, "%s%a", i ? "," : "", elems[i]);
                }
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct ndict *ndict = *(struct ndict **)(priv + p->offset);
                struct ndict_entry *entry = NULL;
                const int nb_nodes = ngli_ndict_count(ndict);
                if (!nb_nodes)
                    break;
                if (constructor)
                    ngli_bstr_print(b, " ");
                else
                    ngli_bstr_print(b, " %s:", p->key);
                int i = 0;
                while ((entry = ngli_ndict_get(ndict, NULL, entry))) {
                    const char *node_id = get_node_id(sctx, entry->node);
                    ngli_bstr_print(b, "%s%s=%s", i ? "," : "", entry->name, node_id);
                    i++;
                }
                break;
            }
            default:
                LOG(ERROR, "Cannot serialize %s: unsupported parameter type", p->key);
        }
        p++;
    }
}

static void serialize(struct hmap *sctx,
                      struct bstr *b,
                      const struct ngl_node *node);

static void serialize_children(struct hmap *sctx,
                               struct bstr *b,
                               const struct ngl_node *node,
                               uint8_t *priv,
                               const struct node_param *p)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child)
                    serialize(sctx, b, child);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                for (int i = 0; i < nb_children; i++)
                    serialize(sctx, b, children[i]);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct ndict *ndict = *(struct ndict **)(priv + p->offset);
                struct ndict_entry *entry = NULL;
                while ((entry = ngli_ndict_get(ndict, NULL, entry)))
                    serialize(sctx, b, entry->node);
                break;
            }
        }
        p++;
    }
}

static void serialize(struct hmap *sctx,
                      struct bstr *b,
                      const struct ngl_node *node)
{
    if (get_node_id(sctx, node))
        return;

    serialize_children(sctx, b, node, (uint8_t *)node, ngli_base_node_params);
    serialize_children(sctx, b, node, node->priv_data, node->class->params);

    ngli_bstr_print(b, "%x", node->class->id);
    serialize_options(sctx, b, node, node->priv_data, node->class->params);
    serialize_options(sctx, b, node, (uint8_t *)node, ngli_base_node_params);
    ngli_bstr_print(b, "\n");

    register_node(sctx, node);
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
    serialize(nlist, b, node);
    s = ngli_bstr_strdup(b);

end:
    ngli_hmap_freep(&nlist);
    ngli_bstr_freep(&b);
    return s;
}
