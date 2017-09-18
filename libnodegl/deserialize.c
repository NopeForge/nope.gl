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
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"

struct serial_ctx {
    struct ngl_node **nodes;
    int nb_nodes;
};

static int register_node(struct serial_ctx *sctx,
                         const struct ngl_node *node)
{
    struct ngl_node **new_nodes = realloc(sctx->nodes, (sctx->nb_nodes + 1) * sizeof(*new_nodes));
    if (!new_nodes)
        return -1;
    new_nodes[sctx->nb_nodes++] = (struct ngl_node *)node;
    sctx->nodes = new_nodes;
    return 0;
}

#define CASE_LITERAL(param_type, type, fmt) \
case param_type: {                          \
    type v;                                 \
    n = sscanf(str, fmt "%n", &v, &len);    \
    if (n != 1)                             \
        return -1;                          \
    memcpy(dstp, &v, sizeof(v));            \
    break;                                  \
}

#define DECLARE_PARSE_LIST_FUNC(type, fmt)                                  \
static int parse_##type##s(const char *s, type **valsp, int *nb_valsp)      \
{                                                                           \
    type *vals = NULL;                                                      \
    int nb_vals = 0, consumed = 0, len;                                     \
                                                                            \
    for (;;) {                                                              \
        type v;                                                             \
        int n = sscanf(s, fmt "%n", &v, &len);                              \
        if (n != 1) {                                                       \
            consumed = -1;                                                  \
            break;                                                          \
        }                                                                   \
        type *new_vals = realloc(vals, (nb_vals + 1) * sizeof(*new_vals));  \
        if (!new_vals) {                                                    \
            consumed = -1;                                                  \
            break;                                                          \
        }                                                                   \
        s += len;                                                           \
        consumed += len;                                                    \
        new_vals[nb_vals++] = v;                                            \
        vals = new_vals;                                                    \
        if (*s != ',')                                                      \
            break;                                                          \
        s++;                                                                \
        consumed++;                                                         \
    }                                                                       \
    if (consumed < 0) {                                                     \
        free(vals);                                                         \
        vals = NULL;                                                        \
        nb_vals = 0;                                                        \
    }                                                                       \
    *valsp = vals;                                                          \
    *nb_valsp = nb_vals;                                                    \
    return consumed;                                                        \
}

DECLARE_PARSE_LIST_FUNC(double, "%la")
DECLARE_PARSE_LIST_FUNC(int,    "%x")

#define FREE_KVS(count, keys, vals) do {                                    \
    for (int k = 0; k < (count); k++)                                       \
        free((keys)[k]);                                                    \
    free(keys);                                                             \
    free(vals);                                                             \
    keys = NULL;                                                            \
    vals = NULL;                                                            \
} while (0)

static int parse_kvs(const char *s, int *nb_kvsp, char ***keysp, int **valsp)
{
    char **keys = NULL;
    int *vals = NULL;
    int nb_vals = 0, consumed = 0, len;

    for (;;) {
        char key[63 + 1];
        int val;
        int n = sscanf(s, "%63[^=]=%x" "%n", key, &val, &len);
        if (n != 2) {
            consumed = -1;
            break;
        }

        char **new_keys = realloc(keys, (nb_vals +1) * sizeof(*new_keys));
        if (!new_keys) {
            consumed = -1;
            break;
        }
        keys = new_keys;

        int *new_vals = realloc(vals, (nb_vals + 1) * sizeof(*new_vals));
        if (!new_vals) {
            consumed = -1;
            break;
        }
        vals = new_vals;

        s += len;
        consumed += len;
        keys[nb_vals] = ngli_strdup(key);
        vals[nb_vals] = val;
        nb_vals++;
        if (*s != ',')
            break;
        s++;
        consumed++;
    }
    if (consumed < 0) {
        FREE_KVS(nb_vals, keys, vals);
        nb_vals = 0;
    }
    *keysp = keys;
    *valsp = vals;
    *nb_kvsp = nb_vals;
    return consumed;
}

static inline int hexv(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - ('a' - 10);
    if (c >= '0' && c <= '9')
        return c - '0';
    return 0;
}

static int parse_param(struct serial_ctx *sctx, uint8_t *base_ptr,
                       const struct node_param *par, const char *str)
{
    int n, len = -1;
    uint8_t *dstp = base_ptr + par->offset;

    switch (par->type) {
        CASE_LITERAL(PARAM_TYPE_INT, int,     "%d")
        CASE_LITERAL(PARAM_TYPE_I64, int64_t, "%"SCNd64)
        CASE_LITERAL(PARAM_TYPE_DBL, double,  "%la")

        case PARAM_TYPE_STR: {
            len = strcspn(str, " \n");
            char *s = malloc(len + 1);
            char *sstart = s;
            for (int i = 0; i < len; i++) {
                if (str[i] == '%' && i + 2 < len) {
                    int chr = hexv(str[i+1])<<4 | hexv(str[i+2]);
                    *s++ = chr;
                    i += 2;
                } else {
                    *s++ = str[i];
                }
            }
            *s = 0;
            ngli_params_vset(base_ptr, par, sstart);
            free(sstart);
            break;
        }
        case PARAM_TYPE_VEC2: {
            float v[2];
            n = sscanf(str, "%a,%a%n", v, v+1, &len);
            if (n != 2)
                return -1;
            ngli_params_vset(base_ptr, par, v);
            break;
        }
        case PARAM_TYPE_VEC3: {
            float v[3];
            n = sscanf(str, "%a,%a,%a%n", v, v+1, v+2, &len);
            if (n != 3)
                return -1;
            ngli_params_vset(base_ptr, par, v);
            break;
        }
        case PARAM_TYPE_VEC4: {
            float v[4];
            n = sscanf(str, "%a,%a,%a,%a%n", v, v+1, v+2, v+3, &len);
            if (n != 4)
                return -1;
            ngli_params_vset(base_ptr, par, v);
            break;
        }
        case PARAM_TYPE_NODE: {
            int node_id;
            n = sscanf(str, "%x%n", &node_id, &len);
            if (n != 1 || node_id < 0 || node_id >= sctx->nb_nodes)
                return -1;
            struct ngl_node *node = sctx->nodes[node_id];
            n = ngli_params_vset(base_ptr, par, node);
            if (n < 0)
                return n;
            break;
        }
        case PARAM_TYPE_NODELIST: {
            int *node_ids, nb_node_ids;
            len = parse_ints(str, &node_ids, &nb_node_ids);
            if (len < 0)
                return -1;
            for (int i = 0; i < nb_node_ids; i++) {
                const int node_id = node_ids[i];
                if (node_id < 0 || node_id >= sctx->nb_nodes) {
                    free(node_ids);
                    return -1;
                }
                struct ngl_node *node = sctx->nodes[node_id];
                ngli_params_add(base_ptr, par, 1, &node);
            }
            free(node_ids);
            break;
        }
        case PARAM_TYPE_DBLLIST: {
            double *dbls;
            int nb_dbls;
            len = parse_doubles(str, &dbls, &nb_dbls);
            if (len < 0)
                return -1;
            ngli_params_add(base_ptr, par, nb_dbls, dbls);
            break;
        }
        case PARAM_TYPE_NODEDICT: {
            char **node_keys;
            int *node_ids, nb_nodes;
            len = parse_kvs(str, &nb_nodes, &node_keys, &node_ids);
            if (len < 0)
                return -1;
            for (int i = 0; i < nb_nodes; i++) {
                const char *key = node_keys[i];
                const int node_id = node_ids[i];
                if (node_id < 0 || node_id >= sctx->nb_nodes) {
                    FREE_KVS(nb_nodes, node_keys, node_ids);
                    return -1;
                }
                struct ngl_node *node = sctx->nodes[node_id];
                ngli_params_vset(base_ptr, par, key, node);
            }
            FREE_KVS(nb_nodes, node_keys, node_ids);
            break;
        }
        default:
            LOG(ERROR, "Cannot deserialize %s: "
                "unsupported parameter type", par->key);
    }
    return len;
}

static int set_node_params(struct serial_ctx *sctx, char *str,
                           const struct ngl_node *node)
{
    uint8_t *base_ptr = node->priv_data;
    const struct node_param *params = node->class->params;

    if (!params)
        return 0;

    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        if (par->flags & PARAM_FLAG_CONSTRUCTOR) {
            int ret = parse_param(sctx, base_ptr, par, str);
            if (ret < 0)
                break;

            str += ret;
            if (*str != ' ')
                break;
            str++;
        } else {
            break; /* assume all constructors are at the start */
        }
    }

    for (;;) {
        char *eok = strchr(str, ':');
        if (!eok)
            break;
        *eok = 0;

        const struct node_param *par = ngli_node_param_find(node, str, &base_ptr);
        if (!par)
            break;

        str = eok + 1;
        int ret = parse_param(sctx, base_ptr, par, str);
        if (ret < 0)
            break;

        str += ret;
        if (*str != ' ')
            break;
        str++;
    }

    return 0;
}

struct ngl_node *ngl_node_deserialize(const char *str)
{
    struct ngl_node *node = NULL;
    struct serial_ctx sctx = {0};

    char *s = ngli_strdup(str);
    if (!s)
        return NULL;

    char *sstart = s;
    size_t len = strlen(str);
    char *end = s + len;

    int major, minor, micro;
    int n = sscanf(s, "# Node.GL v%d.%d.%d", &major, &minor, &micro);
    if (n != 3) {
        LOG(ERROR, "Invalid serialized scene");
        goto end;
    }
    if (NODEGL_VERSION_INT != NODEGL_GET_VERSION(major, minor, micro)) {
        LOG(ERROR, "Mismatching version: %d.%d.%d != %d.%d.%d",
            major, minor, micro,
            NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
        goto end;
    }
    s += strcspn(s, "\n");

    while (s < end) {
        const int type = strtol(s, &s, 16);
        if (*s == ' ')
            s++;

        node = ngli_node_create_noconstructor(type);
        if (!node)
            break;

        int ret = register_node(&sctx, node);
        if (ret < 0) {
            ngl_node_unrefp(&node);
            break;
        }

        size_t eol = strcspn(s, "\n");
        s[eol] = 0;

        set_node_params(&sctx, s, node);

        s += eol + 1;
    }

    if (node)
        ngl_node_ref(node);

    for (int i = 0; i < sctx.nb_nodes; i++)
        ngl_node_unrefp(&sctx.nodes[i]);
    free(sctx.nodes);

end:
    free(sstart);
    return node;
}
