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

#include <stdint.h>
#include <string.h>

#include "internal.h"
#include "nopegl.h"
#include "utils/bstr.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "utils/crc32.h"
#include "utils/string.h"

#define LB "<br align=\"left\"/>"
#define HSLFMT "\"0.%u 0.6 0.9\""
#define INACTIVE_COLOR "\"#333333\""

extern const struct node_param ngli_base_node_params[];

static int visited(struct hmap *ptr_set, const void *id)
{
    const uint64_t key = (uint64_t)(uintptr_t)id;
    if (ngli_hmap_get_u64(ptr_set, key))
        return 1;
    return ngli_hmap_set_u64(ptr_set, key, "");
}

static unsigned get_hue(const char *name)
{
    const uint32_t hash = ngli_crc32(name);
    const double hue = (double)hash / (double)0xffffffff;
    return (unsigned)(hue * 1000);
}

static int vec_is_set(const uint8_t *srcp, const struct node_param *par)
{
    const uint32_t n = par->type - NGLI_PARAM_TYPE_VEC2 + 2;
    const float *v = (const float *)srcp;
    return memcmp(v, par->def_value.vec, n * sizeof(*v));
}

static int ivec_is_set(const uint8_t *srcp, const struct node_param *par)
{
    const uint32_t n = par->type - NGLI_PARAM_TYPE_IVEC2 + 2;
    const int32_t *v = (const int32_t *)srcp;
    return memcmp(v, par->def_value.ivec, n * sizeof(*v));
}

static int uvec_is_set(const uint8_t *srcp, const struct node_param *par)
{
    const uint32_t n = par->type - NGLI_PARAM_TYPE_UVEC2 + 2;
    const uint32_t *v = (const uint32_t *)srcp;
    return memcmp(v, par->def_value.uvec, n * sizeof(*v));
}

static int mat_is_set(const uint8_t *srcp, const struct node_param *par)
{
    const float *v = (const float *)srcp;
    return memcmp(v, par->def_value.mat, 16 * sizeof(*v));
}

static int should_print_par(const uint8_t *srcp, const struct node_param *par)
{
    switch (par->type) {
        case NGLI_PARAM_TYPE_F32: {
            const float v = *(float *)srcp;
            return v != par->def_value.f32;
        }
        case NGLI_PARAM_TYPE_F64: {
            const double v = *(double *)srcp;
            return v != par->def_value.f64;
        }
        case NGLI_PARAM_TYPE_BOOL:
        case NGLI_PARAM_TYPE_FLAGS:
        case NGLI_PARAM_TYPE_SELECT:
        case NGLI_PARAM_TYPE_I32: {
            const int v = *(int *)srcp;
            return v != par->def_value.i32;
        }
        case NGLI_PARAM_TYPE_U32: {
            const uint32_t v = *(uint32_t *)srcp;
            return v != par->def_value.u32;
        }
        case NGLI_PARAM_TYPE_RATIONAL: {
            const int32_t *r = (int32_t *)srcp;
            return memcmp(r, par->def_value.r, sizeof(par->def_value.r));
        }
        case NGLI_PARAM_TYPE_STR: {
            const char *s = *(const char **)srcp;
            return s && (!par->def_value.str || strcmp(s, par->def_value.str));
        }
        case NGLI_PARAM_TYPE_VEC2:
        case NGLI_PARAM_TYPE_VEC3:
        case NGLI_PARAM_TYPE_VEC4:
            return vec_is_set(srcp, par);
        case NGLI_PARAM_TYPE_IVEC2:
        case NGLI_PARAM_TYPE_IVEC3:
        case NGLI_PARAM_TYPE_IVEC4:
            return ivec_is_set(srcp, par);
        case NGLI_PARAM_TYPE_UVEC2:
        case NGLI_PARAM_TYPE_UVEC3:
        case NGLI_PARAM_TYPE_UVEC4:
            return uvec_is_set(srcp, par);
        case NGLI_PARAM_TYPE_MAT4:
            return mat_is_set(srcp, par);
        default:
            break;
    }
    return 0;
}

static void print_custom_priv_options(struct bstr *b, const struct ngl_node *node)
{
    const struct node_param *par = node->cls->params;
    uint8_t *priv = node->opts;

    if (!par)
        return;

    while (par->key) {
        const uint8_t *srcp = priv + par->offset;
        if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE) {
            struct ngl_node *pnode = *(struct ngl_node **)srcp;
            if (pnode) {
                par++;
                continue;
            }
            srcp += sizeof(struct ngl_node *);
        }
        if (should_print_par(srcp, par)) {
            ngli_bstr_printf(b, "%s: ", par->key);
            ngli_params_bstr_print_val(b, priv, par);
            ngli_bstr_print(b, LB);
        }
        par++;
    }
}

static void print_decls(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *decls);

static void print_all_decls(struct bstr *b, const struct ngl_node *node, struct hmap *decls)
{
    if (visited(decls, node))
        return;

    ngli_bstr_printf(b, "    %s_%p[label=<<b>%s</b><br/>",
                    node->cls->name, node, node->cls->name);
    if (!ngli_is_default_label(node->cls->name, node->label) && *node->label)
        ngli_bstr_printf(b, "<i>%s</i><br/>", node->label);
    print_custom_priv_options(b, node);
    if (!node->ctx || node->is_active)
        ngli_bstr_printf(b, ">,color="HSLFMT"]\n", get_hue(node->cls->name));
    else
        ngli_bstr_print(b, ">,color="INACTIVE_COLOR"]\n");

    print_decls(b, node, ngli_base_node_params, (uint8_t *)node, decls);
    print_decls(b, node, node->cls->params, node->opts, decls);
}

static void table_header(struct bstr *b, const char *label, int is_active, size_t colspan)
{
    ngli_bstr_print(b, "[shape=none,label=<<table border=\"0\" cellspacing=\"0\" cellborder=\"1\" bgcolor=");
    const unsigned hue = get_hue(label);
    if (is_active)
        ngli_bstr_printf(b, "\"0.%u 0.2 0.8\"", hue); /* color of all the entries, more pale than HSLFMT */
    else
        ngli_bstr_print(b, INACTIVE_COLOR);
    ngli_bstr_printf(b, "><tr><td colspan=\"%zu\" bgcolor=", colspan);
    if (is_active)
        ngli_bstr_printf(b, HSLFMT, hue);
    else
        ngli_bstr_print(b, INACTIVE_COLOR);
    ngli_bstr_printf(b, "><b>%s</b></td></tr>", label);
}

static void table_footer(struct bstr *b)
{
    ngli_bstr_print(b, "</table>>,color=\"#222222\"]\n");
}

static void print_list_packed_decls(struct bstr *b, const char *key,
                                    struct ngl_node **children, size_t nb_children,
                                    int is_active)
{
    ngli_bstr_printf(b, "    %s_%p", key, children);
    table_header(b, key, is_active, 2);
    for (size_t i = 0; i < nb_children; i++) {
        const struct ngl_node *node = children[i];
        char *info_str = node->cls->info_str ? node->cls->info_str(node) : NULL;
        ngli_bstr_printf(b, "<tr><td>#%zu</td><td align=\"left\">%s</td></tr>",
                         i, info_str ? info_str : "?");
        ngli_free(info_str);
    }
    table_footer(b);
}

static void print_dict_packed_decls(struct bstr *b, const char *key,
                                    struct hmap *hmap, int is_active)
{
    ngli_bstr_printf(b, "    %s_%p", key, hmap);
    table_header(b, key, is_active, 2);
    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(hmap, entry))) {
        const struct ngl_node *node = entry->data;
        char *info_str = node->cls->info_str ? node->cls->info_str(node) : NULL;
        ngli_bstr_printf(b, "<tr><td align=\"left\">%s</td><td align=\"left\">%s</td></tr>",
                         entry->key.str, info_str ? info_str : "?");
        ngli_free(info_str);
    }
    table_footer(b);
}

static void print_decls(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *decls)
{
    if (!p)
        return;

    while (p->key) {
        const uint8_t *srcp = priv + p->offset;

        if (p->flags & NGLI_PARAM_FLAG_ALLOW_NODE) {
            const struct ngl_node *child = *(struct ngl_node **)srcp;
            if (child)
                print_all_decls(b, child, decls);
            p++;
            continue;
        }

        switch (p->type) {
            case NGLI_PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)srcp;
                if (child)
                    print_all_decls(b, child, decls);
                break;
            }
            case NGLI_PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)srcp;
                const size_t nb_children = *(size_t *)(srcp + sizeof(struct ngl_node **));

                if (nb_children && (p->flags & NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED)) {
                    print_list_packed_decls(b, p->key, children, nb_children, !node->ctx || node->is_active);
                    break;
                }

                for (size_t i = 0; i < nb_children; i++)
                    print_all_decls(b, children[i], decls);
                break;
            }
            case NGLI_PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)srcp;
                if (!hmap)
                    break;

                if (ngli_hmap_count(hmap) && (p->flags & NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED)) {
                    print_dict_packed_decls(b, p->key, hmap, !node->ctx || node->is_active);
                    break;
                }

                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry)))
                    print_all_decls(b, entry->data, decls);
                break;
            }
            default:
                break;
        }
        p++;
    }
}

static void print_link(struct bstr *b,
                       const struct ngl_node *x, const struct ngl_node *y,
                       const char *edge_attrs)
{
    ngli_bstr_printf(b, "    %s_%p -> %s_%p%s\n",
                    x->cls->name, x, y->cls->name, y, edge_attrs);
}

static void print_links(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *links);

static void print_all_links(struct bstr *b, const struct ngl_node *node, struct hmap *links)
{
    if (visited(links, node))
        return;

    print_links(b, node, ngli_base_node_params, (uint8_t *)node, links);
    print_links(b, node, node->cls->params, node->opts, links);
}

static void print_node_links(struct bstr *b, const struct ngl_node *node,
                             const struct node_param *p, const uint8_t *srcp,
                             struct hmap *links, const char *edge_attrs)
{
    const struct ngl_node *child = *(struct ngl_node **)srcp;
    if (child) {
        print_link(b, node, child, edge_attrs);
        print_all_links(b, child, links);
    }
}

static void print_nodelist_links(struct bstr *b, const struct ngl_node *node,
                                 const struct node_param *p, const uint8_t *srcp,
                                 struct hmap *links, const char *edge_attrs)
{
    struct ngl_node **children = *(struct ngl_node ***)srcp;
    const size_t nb_children = *(size_t *)(srcp + sizeof(struct ngl_node **));

    if (!nb_children)
        return;

    if (p->flags & NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED) {
        ngli_bstr_printf(b, "    %s_%p -> %s_%p%s\n", node->cls->name, node, p->key, children, edge_attrs);
        return;
    }

    /* Declare the list as a dedicated table */
    ngli_bstr_printf(b, "    %s_%p_%s", node->cls->name, node, p->key);
    table_header(b, p->key, !node->ctx || node->is_active, nb_children);
    ngli_bstr_print(b, "<tr>");
    for (size_t i = 0; i < nb_children; i++)
        ngli_bstr_printf(b, "<td port=\"e%zu\">#%zu</td>", i, i);
    ngli_bstr_print(b, "</tr>");
    table_footer(b);

    /* Link node to the list table */
    ngli_bstr_printf(b, "    %s_%p -> %s_%p_%s\n",
                     node->cls->name, node, node->cls->name, node, p->key);

    /* Link individual table cell to their dedicated nodes */
    for (size_t i = 0; i < nb_children; i++) {
        const struct ngl_node *child = children[i];
        ngli_bstr_printf(b, "    %s_%p_%s:e%zu -> %s_%p\n", node->cls->name, node, p->key, i, child->cls->name, child);
        print_all_links(b, child, links);
    }
}

static void print_nodedict_links(struct bstr *b, const struct ngl_node *node,
                                 const struct node_param *p, const uint8_t *srcp,
                                 struct hmap *links, const char *edge_attrs)
{
    struct hmap *hmap = *(struct hmap **)srcp;
    if (!hmap || !ngli_hmap_count(hmap))
        return;

    if (p->flags & NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED) {
        ngli_bstr_printf(b, "    %s_%p -> %s_%p%s\n", node->cls->name, node, p->key, hmap, edge_attrs);
        return;
    }

    /* Declare the dictionary as a dedicated table */
    ngli_bstr_printf(b, "    %s_%p_%s", node->cls->name, node, p->key);
    table_header(b, p->key, !node->ctx || node->is_active, ngli_hmap_count(hmap));
    ngli_bstr_print(b, "<tr>");
    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(hmap, entry)))
        ngli_bstr_printf(b, "<td port=\"%s\">%s</td>", entry->key.str, entry->key.str);
    ngli_bstr_print(b, "</tr>");
    table_footer(b);

    /* Link node to the dict table */
    ngli_bstr_printf(b, "    %s_%p -> %s_%p_%s\n",
                     node->cls->name, node, node->cls->name, node, p->key);

    /* Link individual table cell to their dedicated nodes */
    entry = NULL;
    while ((entry = ngli_hmap_next(hmap, entry))) {
        const struct ngl_node *child = entry->data;
        ngli_bstr_printf(b, "    %s_%p_%s:%s -> %s_%p\n",
                         node->cls->name, node, p->key, entry->key.str, child->cls->name, child);
        print_all_links(b, child, links);
    }
}

static void print_links(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *links)
{
    if (!p)
        return;

    while (p->key) {
        const int print_label = !!(p->flags & (NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME | NGLI_PARAM_FLAG_ALLOW_NODE));
        char *edge_attrs = "";
        if (print_label) {
            edge_attrs = ngli_asprintf("[label=\"%s\"]", p->key);
            if (!edge_attrs)
                return;
        }
        const uint8_t *srcp = priv + p->offset;
        switch (p->type) {
        case NGLI_PARAM_TYPE_NODE:
            print_node_links(b, node, p, srcp, links, edge_attrs);
            break;
        case NGLI_PARAM_TYPE_NODELIST:
            print_nodelist_links(b, node, p, srcp, links, edge_attrs);
            break;
        case NGLI_PARAM_TYPE_NODEDICT:
            print_nodedict_links(b, node, p, srcp, links, edge_attrs);
            break;
        default:
            if (p->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
                print_node_links(b, node, p, srcp, links, edge_attrs);
            break;
        }
        if (print_label)
            ngli_free(edge_attrs);
        p++;
    }
}

char *ngli_scene_dot(const struct ngl_scene *s)
{
    if (!s || !s->params.root)
        return NULL;

    char *graph = NULL;
    struct hmap *decls = ngli_hmap_create(NGLI_HMAP_TYPE_U64);
    struct hmap *links = ngli_hmap_create(NGLI_HMAP_TYPE_U64);
    struct bstr *b = ngli_bstr_create();
    if (!decls || !links || !b)
        goto end;

    const char *font_settings="fontsize=9,fontname=Arial";

    ngli_bstr_printf(b, "digraph G {\n"
                    "    bgcolor=\"#222222\";\n"
                    "    edge [%s,color=\"#dddddd\",fontcolor=\"#dddddd\",arrowsize=0.7];\n"
                    "    node [style=filled,%s];\n",
                    font_settings, font_settings);

    const struct ngl_node *node = s->params.root;
    print_all_decls(b, node, decls);
    print_all_links(b, node, links);

    ngli_bstr_print(b, "}\n");

    graph = ngli_bstr_strdup(b);

end:
    ngli_bstr_freep(&b);
    ngli_hmap_freep(&decls);
    ngli_hmap_freep(&links);
    return graph;
}

char *ngl_dot(struct ngl_ctx *s, double t)
{
    int ret = ngli_prepare_draw(s, t);
    if (ret < 0)
        return NULL;
    return ngli_scene_dot(s->scene);
}
