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

#include <stdint.h>
#include <string.h>

#include "bstr.h"
#include "ndict.h"
#include "nodegl.h"
#include "nodes.h"

#define LB "<br align=\"left\"/>"
#define HSLFMT "\"%f 0.6 0.9\""

extern const struct node_param ngli_base_node_params[];

struct decl {
    const void *id;
    struct decl *next;
};

static int list_check(struct decl **idxdecls, const void *id)
{
    for (struct decl *decl = *idxdecls; decl; decl = decl->next)
        if (id == decl->id)
            return 1;

    struct decl *cur = malloc(sizeof(*cur));
    cur->id = id;
    cur->next = *idxdecls;
    *idxdecls = cur;

    return 0;
}

static void free_list_decls(struct decl **idxdecls)
{
    struct decl *decl = *idxdecls;
    while (decl) {
        struct decl *next = decl->next;
        free(decl);
        decl = next;
    }
    *idxdecls = NULL;
}

static float get_hue(const char *name)
{
    uint32_t hash = 5381;
    for (int i = 0; name[i]; i++)
        hash = (hash<<5) + hash + name[i];
    return (hash & 0xffffff) / (float)(0xffffff);
}

static int should_print_par(uint8_t *priv, const struct node_param *par)
{
    switch (par->type) {
        case PARAM_TYPE_DBL: {
            const double v = *(double *)(priv + par->offset);
            return v != par->def_value.dbl;
        }
        case PARAM_TYPE_INT: {
            const int v = *(int *)(priv + par->offset);
            return v != par->def_value.i64;
        }
        case PARAM_TYPE_I64: {
            const int64_t v = *(int64_t *)(priv + par->offset);
            return v != par->def_value.i64;
        }
        case PARAM_TYPE_VEC2: {
            const float *v = (const float *)(priv + par->offset);
            return v[0] || v[1];
        }
        case PARAM_TYPE_VEC3: {
            const float *v = (const float *)(priv + par->offset);
            return v[0] || v[1] || v[2];
        }
        case PARAM_TYPE_VEC4: {
            const float *v = (const float *)(priv + par->offset);
            return v[0] || v[1] || v[2] || v[4];
        }
        case PARAM_TYPE_STR: {
            const char *s = *(const char **)(priv + par->offset);
            return (s && !strchr(s, '\n') /* prevent shaders from being printed */ &&
                    (!par->def_value.str || strcmp(s, par->def_value.str)));
        }
    }
    return 0;
}

static void print_custom_priv_options(struct bstr *b, const struct ngl_node *node)
{
    const struct node_param *par = node->class->params;
    uint8_t *priv = node->priv_data;

    if (!par)
        return;

    while (par->key) {
        if (should_print_par(priv, par)) {
            ngli_bstr_print(b, "%s: ", par->key);
            ngli_params_bstr_print_val(b, priv, par);
            ngli_bstr_print(b, LB);
        }
        par++;
    }
}

static void print_decls(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct decl **idxdecls);

static void print_all_decls(struct bstr *b, const struct ngl_node *node, struct decl **idxdecls)
{
    if (list_check(idxdecls, node))
        return;

    ngli_bstr_print(b, "    %s_%p[label=<<b>%s</b><br/><i>%s</i><br/>",
                    node->class->name, node, node->class->name, node->name);
    print_custom_priv_options(b, node);
    ngli_bstr_print(b, ">,color="HSLFMT"]\n", get_hue(node->class->name));

    print_decls(b, node, ngli_base_node_params, (uint8_t *)node, idxdecls);
    print_decls(b, node, node->class->params, node->priv_data, idxdecls);
}

static void print_packed_decls(struct bstr *b, const char *name,
                               struct ngl_node **children, int nb_children)
{
    ngli_bstr_print(b, "    %s_%p[label=<<b>%s</b> (x%d)", name, children, name, nb_children);
    for (int i = 0; i < nb_children; i++) {
        const struct ngl_node *node = children[i];
        char *info_str = node->class->info_str ? node->class->info_str(node) : NULL;
        ngli_bstr_print(b, LB "- %s", info_str ? info_str : "?");
        free(info_str);
    }
    ngli_bstr_print(b, LB ">,shape=box,color="HSLFMT"]\n", get_hue(name));
}

static void print_decls(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct decl **idxdecls)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child)
                    print_all_decls(b, child, idxdecls);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                if (nb_children && (p->flags & PARAM_FLAG_DOT_DISPLAY_PACKED)) {
                    if (list_check(idxdecls, children))
                        break;
                    print_packed_decls(b, p->key, children, nb_children);
                    break;
                }

                for (int i = 0; i < nb_children; i++)
                    print_all_decls(b, children[i], idxdecls);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct ndict *ndict = *(struct ndict **)(priv + p->offset);
                struct ndict_entry *entry = NULL;
                while ((entry = ngli_ndict_get(ndict, NULL, entry)))
                    print_all_decls(b, entry->node, idxdecls);
                break;
            }
        }
        p++;
    }
}

struct link {
    const void *a;
    const void *b;
    struct link *next;
};

static int list_check_links(struct link **links, const void *a, const void *b)
{
    for (struct link *link = *links; link; link = link->next)
        if ((link->a == a && link->b == b) ||
            (link->a == b && link->b == a))
            return 1;

    struct link *cur = malloc(sizeof(*cur));
    cur->a = a;
    cur->b = b;
    cur->next = *links;
    *links = cur;

    return 0;
}

static void free_list_links(struct link **links)
{
    struct link *link = *links;
    while (link) {
        struct link *next = link->next;
        free(link);
        link = next;
    }
    *links = NULL;
}

static void print_link(struct bstr *b,
                       const struct ngl_node *x, const struct ngl_node *y,
                       const char *label)
{
    ngli_bstr_print(b, "    %s_%p -> %s_%p%s\n",
                    x->class->name, x, y->class->name, y, label);
}

static void print_links(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct link **idxlinks);

static void print_all_links(struct bstr *b, const struct ngl_node *node, struct link **idxlinks)
{
    print_links(b, node, ngli_base_node_params, (uint8_t *)node, idxlinks);
    print_links(b, node, node->class->params, node->priv_data, idxlinks);
}

static void print_links(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct link **idxlinks)
{
    while (p && p->key) {
        char *label = ngli_asprintf("[label=\"%s\"]",
                                    (p->flags & PARAM_FLAG_DOT_DISPLAY_FIELDNAME) ? p->key : "");
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child) {
                    if (list_check_links(idxlinks, node, child))
                        break;
                    print_link(b, node, child, label);
                    print_all_links(b, child, idxlinks);
                }
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                if (nb_children && (p->flags & PARAM_FLAG_DOT_DISPLAY_PACKED)) {
                    if (list_check_links(idxlinks, node, children))
                        break;
                    ngli_bstr_print(b, "    %s_%p -> %s_%p%s\n",
                                    node->class->name, node, p->key, children, label);
                    break;
                }

                for (int i = 0; i < nb_children; i++) {
                    const struct ngl_node *child = children[i];

                    if (list_check_links(idxlinks, node, child))
                        continue;
                    print_link(b, node, child, label);
                    print_all_links(b, child, idxlinks);
                }
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct ndict *ndict = *(struct ndict **)(priv + p->offset);
                struct ndict_entry *entry = NULL;

                while ((entry = ngli_ndict_get(ndict, NULL, entry))) {
                    char *key;
                    const struct ngl_node *child = entry->node;

                    if (list_check_links(idxlinks, node, child))
                        continue;

                    if (p->flags & PARAM_FLAG_DOT_DISPLAY_FIELDNAME)
                        key = ngli_asprintf("[label=\"%s:%s\"]", p->key, entry->name);
                    else
                        key = ngli_asprintf("[label=\"%s\"]", entry->name);

                    if (!key) {
                        free(label);
                        return;
                    }

                    print_link(b, node, child, key);
                    print_all_links(b, child, idxlinks);

                    free(key);
                }
                break;
            }
        }
        free(label);
        p++;
    }
}

char *ngl_node_dot(const struct ngl_node *node)
{
    struct decl *idxdecls = NULL;
    struct link *idxlinks = NULL;

    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    const char *font_settings="fontsize=9,fontname=Arial";

    ngli_bstr_print(b, "digraph G {\n"
                    "    bgcolor=\"#222222\";\n"
                    "    edge [%s,color=\"#dddddd\",fontcolor=\"#dddddd\",arrowsize=0.7];\n"
                    "    node [style=filled,%s];\n",
                    font_settings, font_settings);

    print_all_decls(b, node, &idxdecls);
    print_all_links(b, node, &idxlinks);

    ngli_bstr_print(b, "}\n");

    free_list_decls(&idxdecls);
    free_list_links(&idxlinks);

    char *graph = ngli_bstr_strdup(b);

    ngli_bstr_freep(&b);

    return graph;
}
