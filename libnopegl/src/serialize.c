/*
 * Copyright 2017-2022 GoPro Inc.
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

#include "internal.h"
#include "log.h"
#include "nopegl.h"
#include "utils/bstr.h"
#include "utils/darray.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "utils/utils.h"

extern const struct node_param ngli_base_node_params[];

static void free_func(void *arg, void *data)
{
    ngli_free(data);
}

static int register_node(struct hmap *nlist,
                          const struct ngl_node *node)
{
    const uint64_t key = (uint64_t)(uintptr_t)node;
    char *val = ngli_asprintf("%zx", ngli_hmap_count(nlist));
    if (!val)
        return NGL_ERROR_MEMORY;
    int ret = ngli_hmap_set_u64(nlist, key, val);
    if (ret < 0)
        ngli_free(val);
    return ret;
}

static int get_node_id(const struct hmap *nlist, const struct ngl_node *node)
{
    const uint64_t key = (uint64_t)(uintptr_t)node;
    const char *val = ngli_hmap_get_u64(nlist, key);
    return val ? (int)strtol(val, NULL, 16) : -1;
}

static int get_rel_node_id(const struct hmap *nlist, const struct ngl_node *node)
{
    return (int)ngli_hmap_count(nlist) - get_node_id(nlist, node);
}

#define DECLARE_FLT_PRINT_FUNC(type, nbit, shift_exp, z)                \
static void print_f##nbit(struct bstr *b, type f)                       \
{                                                                       \
    const union { uint##nbit##_t i; type f; } u = {.f = f};             \
    const uint##nbit##_t v = u.i;                                       \
    const int sign = (int)(v >> (nbit - 1));                            \
    const uint##nbit##_t exp_mask = (1 << (nbit - shift_exp - 1)) - 1;  \
    const uint##nbit##_t exp  = v >> shift_exp & exp_mask;              \
    const uint##nbit##_t mant = v & ((1ULL << shift_exp) - 1);          \
    ngli_bstr_printf(b, "%s%" PRIX##nbit "%c%" PRIX##nbit,              \
                    sign ? "-" : "", exp, z, mant);                     \
}                                                                       \

DECLARE_FLT_PRINT_FUNC(float,  32, 23, 'z')
DECLARE_FLT_PRINT_FUNC(double, 64, 52, 'Z')

#define print_i32(b, v) ngli_bstr_printf(b, "%d", v)
#define print_u32(b, v) ngli_bstr_printf(b, "%u", v)

#define DECLARE_PRINT_FUNC(name, type)                                  \
static void print_##name##s(struct bstr *b, size_t n, const type *v)    \
{                                                                       \
    for (size_t i = 0; i < n; i++) {                                    \
        ngli_bstr_printf(b, "%s", i ? "," : "");                        \
        print_##name(b, v[i]);                                          \
    }                                                                   \
}

DECLARE_PRINT_FUNC(f32, float)
DECLARE_PRINT_FUNC(f64, double)
DECLARE_PRINT_FUNC(i32, int32_t)
DECLARE_PRINT_FUNC(u32, uint32_t)

struct item {
    const char *key;
    void *data;
};

static int cmp_item(const void *p1, const void *p2)
{
    const struct item *i1 = p1;
    const struct item *i2 = p2;
    return strcmp(i1->key, i2->key);
}

static int hmap_to_sorted_items(struct darray *items_array, struct hmap *hm)
{
    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(hm, entry))) {
        struct item item = {.key = entry->key.str, .data = entry->data};
        if (!ngli_darray_push(items_array, &item))
            return NGL_ERROR_MEMORY;
    }

    void *items = ngli_darray_data(items_array);
    const size_t nb_items = ngli_darray_count(items_array);
    qsort(items, nb_items, sizeof(struct item), cmp_item);

    return 0;
}

static void serialize_select(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const int v = *(int *)srcp;
    const char *s = ngli_params_get_select_str(par->choices->consts, v);
    ngli_assert(s);
    if (v != par->def_value.i32)
        ngli_bstr_printf(b, " %s:%s", par->key, s);
}

static int serialize_flags(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const int v = *(int *)srcp;
    char *s = ngli_params_get_flags_str(par->choices->consts, v);
    if (!s) {
        LOG(ERROR, "unable to allocate param flags string");
        return NGL_ERROR_MEMORY;
    }
    ngli_assert(*s);
    if (v != par->def_value.i32)
        ngli_bstr_printf(b, " %s:%s", par->key, s);
    ngli_free(s);
    return 0;
}

static void serialize_i32(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const int v = *(int *)srcp;
    if (v != par->def_value.i32)
        ngli_bstr_printf(b, " %s:%d", par->key, v);
}

static void serialize_u32(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const int v = *(int *)srcp;
    if (v != par->def_value.u32)
        ngli_bstr_printf(b, " %s:%u", par->key, v);
}

static void serialize_f32(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const float v = *(float *)srcp;
    if (v != par->def_value.f32) {
        ngli_bstr_printf(b, " %s:", par->key);
        print_f32(b, v);
    }
}

static void serialize_f64(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const double v = *(double *)srcp;
    if (v != par->def_value.f64) {
        ngli_bstr_printf(b, " %s:", par->key);
        print_f64(b, v);
    }
}

static void serialize_rational(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const int *r = (int *)srcp;
    if (memcmp(r, par->def_value.r, sizeof(par->def_value.r)))
        ngli_bstr_printf(b, " %s:%d/%d", par->key, r[0], r[1]);
}

static void serialize_str(struct bstr *b, const uint8_t *srcp,
                          const struct node_param *par, const char *label)
{
    const char *s = *(char **)srcp;
    if (!s || (par->def_value.str && !strcmp(s, par->def_value.str)))
        return;
    if (!strcmp(par->key, "label") && ngli_is_default_label(label, s))
        return;
    ngli_bstr_printf(b, " %s:", par->key);
    for (size_t i = 0; s[i]; i++)
        if (s[i] >= '!' && s[i] <= '~' && s[i] != '%')
            ngli_bstr_printf(b, "%c", s[i]);
        else
            ngli_bstr_printf(b, "%%%02x", s[i] & 0xff);
}

static void serialize_data(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const uint8_t *data = *(uint8_t **)srcp;
    const size_t size = *(size_t *)(srcp + sizeof(uint8_t *));
    if (!data || !size)
        return;
    ngli_bstr_printf(b, " %s:%zu,", par->key, size);
    for (size_t i = 0; i < size; i++)
        ngli_bstr_printf(b, "%02x", data[i]);
}

static void serialize_ivec(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const int32_t *iv = (const int *)srcp;
    const uint32_t n = par->type - NGLI_PARAM_TYPE_IVEC2 + 2;
    if (memcmp(iv, par->def_value.ivec, n * sizeof(*iv))) {
        ngli_bstr_printf(b, " %s:", par->key);
        print_i32s(b, n, iv);
    }
}

static void serialize_uvec(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const uint32_t *uv = (const uint32_t *)srcp;
    const uint32_t n = par->type - NGLI_PARAM_TYPE_UVEC2 + 2;
    if (memcmp(uv, par->def_value.uvec, n * sizeof(*uv))) {
        ngli_bstr_printf(b, " %s:", par->key);
        print_u32s(b, n, uv);
    }
}

static void serialize_vec(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const float *v = (float *)srcp;
    const uint32_t n = par->type - NGLI_PARAM_TYPE_VEC2 + 2;
    if (memcmp(v, par->def_value.vec, n * sizeof(*v))) {
        ngli_bstr_printf(b, " %s:", par->key);
        print_f32s(b, n, v);
    }
}

static void serialize_mat4(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const float *m = (float *)srcp;
    if (memcmp(m, par->def_value.mat, 16 * sizeof(*m))) {
        ngli_bstr_printf(b, " %s:", par->key);
        print_f32s(b, 16, m);
    }
}

static void serialize_node(struct bstr *b, const uint8_t *srcp,
                           const struct node_param *par, struct hmap *nlist)
{
    const struct ngl_node *node = *(struct ngl_node **)srcp;
    if (!node)
        return;
    const int node_id = get_rel_node_id(nlist, node);
    ngli_bstr_printf(b, " %s:%x", par->key, node_id);
}

static void serialize_nodelist(struct bstr *b, const uint8_t *srcp,
                               const struct node_param *par, struct hmap *nlist)
{
    struct ngl_node **nodes = *(struct ngl_node ***)srcp;
    const size_t nb_nodes = *(size_t *)(srcp + sizeof(struct ngl_node **));
    if (!nb_nodes)
        return;
    ngli_bstr_printf(b, " %s:", par->key);
    for (size_t i = 0; i < nb_nodes; i++) {
        const int node_id = get_rel_node_id(nlist, nodes[i]);
        ngli_bstr_printf(b, "%s%x", i ? "," : "", node_id);
    }
}

static void serialize_f64list(struct bstr *b, const uint8_t *srcp, const struct node_param *par)
{
    const uint8_t *elems_p = srcp;
    const uint8_t *nb_elems_p = srcp + sizeof(double *);
    const double *elems = *(double **)elems_p;
    const size_t nb_elems = *(size_t *)nb_elems_p;
    if (!nb_elems)
        return;
    ngli_bstr_printf(b, " %s:", par->key);
    print_f64s(b, nb_elems, elems);
}

static int serialize_nodedict(struct bstr *b, const uint8_t *srcp,
                              const struct node_param *par, struct hmap *nlist)
{
    struct hmap *hmap = *(struct hmap **)srcp;
    const size_t nb_nodes = hmap ? ngli_hmap_count(hmap) : 0;
    if (!nb_nodes)
        return 0;
    ngli_bstr_printf(b, " %s:", par->key);

    struct darray items_array;
    ngli_darray_init(&items_array, sizeof(struct item), 0);
    int ret = hmap_to_sorted_items(&items_array, hmap);
    if (ret < 0) {
        ngli_darray_reset(&items_array);
        return ret;
    }
    const struct item *items = ngli_darray_data(&items_array);
    for (size_t i = 0; i < ngli_darray_count(&items_array); i++) {
        const struct item *item = &items[i];
        const int node_id = get_rel_node_id(nlist, item->data);
        ngli_bstr_printf(b, "%s%s=%x", i ? "," : "", item->key, node_id);
    }
    ngli_darray_reset(&items_array);
    return 0;
}

static int serialize_options(struct hmap *nlist,
                             struct bstr *b,
                             const struct ngl_node *node,
                             uint8_t *priv,
                             const struct node_param *p)
{
    if (!p)
        return 0;

    const char *label = node->cls->name;
    while (p->key) {
        const uint8_t *srcp = priv + p->offset;

        if (p->flags & NGLI_PARAM_FLAG_ALLOW_NODE) {
            struct ngl_node *src_node = *(struct ngl_node **)srcp;
            if (src_node) {
                const int node_id = get_rel_node_id(nlist, src_node);
                ngli_bstr_printf(b, " %s:!%x", p->key, node_id);
                p++;
                continue;
            }
            srcp += sizeof(struct ngl_node *);
        }

        int ret = 0;
        switch (p->type) {
        case NGLI_PARAM_TYPE_SELECT:    serialize_select(b, srcp, p);                   break;
        case NGLI_PARAM_TYPE_FLAGS:     ret = serialize_flags(b, srcp, p);              break;
        case NGLI_PARAM_TYPE_BOOL:
        case NGLI_PARAM_TYPE_I32:       serialize_i32(b, srcp, p);                      break;
        case NGLI_PARAM_TYPE_U32:       serialize_u32(b, srcp, p);                      break;
        case NGLI_PARAM_TYPE_F32:       serialize_f32(b, srcp, p);                      break;
        case NGLI_PARAM_TYPE_F64:       serialize_f64(b, srcp, p);                      break;
        case NGLI_PARAM_TYPE_RATIONAL:  serialize_rational(b, srcp, p);                 break;
        case NGLI_PARAM_TYPE_STR:       serialize_str(b, srcp, p, label);               break;
        case NGLI_PARAM_TYPE_DATA:      serialize_data(b, srcp, p);                     break;
        case NGLI_PARAM_TYPE_IVEC2:
        case NGLI_PARAM_TYPE_IVEC3:
        case NGLI_PARAM_TYPE_IVEC4:     serialize_ivec(b, srcp, p);                     break;
        case NGLI_PARAM_TYPE_UVEC2:
        case NGLI_PARAM_TYPE_UVEC3:
        case NGLI_PARAM_TYPE_UVEC4:     serialize_uvec(b, srcp, p);                     break;
        case NGLI_PARAM_TYPE_VEC2:
        case NGLI_PARAM_TYPE_VEC3:
        case NGLI_PARAM_TYPE_VEC4:      serialize_vec(b, srcp, p);                      break;
        case NGLI_PARAM_TYPE_MAT4:      serialize_mat4(b, srcp, p);                     break;
        case NGLI_PARAM_TYPE_NODE:      serialize_node(b, srcp, p, nlist);              break;
        case NGLI_PARAM_TYPE_NODELIST:  serialize_nodelist(b, srcp, p, nlist);          break;
        case NGLI_PARAM_TYPE_F64LIST:   serialize_f64list(b, srcp, p);                  break;
        case NGLI_PARAM_TYPE_NODEDICT:  ret = serialize_nodedict(b, srcp, p, nlist);    break;
        default:
            LOG(ERROR, "cannot serialize %s: unsupported parameter type", p->key);
            return NGL_ERROR_BUG;
        }
        if (ret < 0)
            return ret;
        p++;
    }
    return 0;
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
    if (!p)
        return 0;

    while (p->key) {
        const uint8_t *srcp = priv + p->offset;

        switch (p->type) {
            case NGLI_PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)srcp;
                if (child) {
                    int ret = serialize(nlist, b, child);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
            case NGLI_PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)srcp;
                const size_t nb_children = *(size_t *)(srcp + sizeof(struct ngl_node **));

                for (size_t i = 0; i < nb_children; i++) {
                    int ret = serialize(nlist, b, children[i]);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
            case NGLI_PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)srcp;
                if (!hmap)
                    break;

                struct darray items_array;
                ngli_darray_init(&items_array, sizeof(struct item), 0);
                int ret = hmap_to_sorted_items(&items_array, hmap);
                if (ret < 0) {
                    ngli_darray_reset(&items_array);
                    return ret;
                }
                const struct item *items = ngli_darray_data(&items_array);
                for (size_t i = 0; i < ngli_darray_count(&items_array); i++) {
                    const struct item *item = &items[i];
                    ret = serialize(nlist, b, item->data);
                    if (ret < 0) {
                        ngli_darray_reset(&items_array);
                        return ret;
                    }
                }
                ngli_darray_reset(&items_array);
                break;
            }
            default: {
                if (!(p->flags & NGLI_PARAM_FLAG_ALLOW_NODE))
                    break;
                struct ngl_node *child = *(struct ngl_node **)srcp;
                if (child) {
                    int ret = serialize(nlist, b, child);
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
    if (get_node_id(nlist, node) >= 0)
        return 0;

    int ret;

    if ((ret = serialize_children(nlist, b, node, (uint8_t *)node, ngli_base_node_params)) < 0 ||
        (ret = serialize_children(nlist, b, node, node->opts, node->cls->params)) < 0)
        return ret;

    const uint32_t tag = node->cls->id;
    ngli_bstr_printf(b, "%c%c%c%c",
                    tag >> 24 & 0xff,
                    tag >> 16 & 0xff,
                    tag >>  8 & 0xff,
                    tag       & 0xff);
    if ((ret = serialize_options(nlist, b, node, node->opts, node->cls->params)) < 0 ||
        (ret = serialize_options(nlist, b, node, (uint8_t *)node, ngli_base_node_params)) < 0)
        return ret;

    ngli_bstr_print(b, "\n");

    return register_node(nlist, node);
}

char *ngli_scene_serialize(const struct ngl_scene *s)
{
    char *str = NULL;
    struct hmap *nlist = ngli_hmap_create(NGLI_HMAP_TYPE_U64);
    struct bstr *b = ngli_bstr_create();
    if (!nlist || !b)
        goto end;

    /* Write header */
    ngli_hmap_set_free_func(nlist, free_func, NULL);
    ngli_bstr_printf(b, "# Nope.GL v%d.%d.%d\n",
                    NGL_VERSION_MAJOR, NGL_VERSION_MINOR, NGL_VERSION_MICRO);

    /* Write metadata */
    ngli_bstr_printf(b, "# duration=");
    print_f64(b, s->params.duration);
    ngli_bstr_print(b, "\n");
    ngli_bstr_printf(b, "# aspect_ratio=%d/%d\n", s->params.aspect_ratio[0], s->params.aspect_ratio[1]);
    ngli_bstr_printf(b, "# framerate=%d/%d\n", s->params.framerate[0], s->params.framerate[1]);

    /* Write nodes (1 line = 1 node) */
    if (serialize(nlist, b, s->params.root) < 0)
        goto end;
    str = ngli_bstr_strdup(b);

end:
    ngli_hmap_freep(&nlist);
    ngli_bstr_freep(&b);
    return str;
}
