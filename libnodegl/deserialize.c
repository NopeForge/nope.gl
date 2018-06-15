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
    int nb_allocated_nodes;
};

static int register_node(struct serial_ctx *sctx,
                         const struct ngl_node *node)
{
    if (sctx->nb_nodes == sctx->nb_allocated_nodes) {
        if (!sctx->nb_allocated_nodes)
            sctx->nb_allocated_nodes = 16;
        else
            sctx->nb_allocated_nodes *= 2;
        struct ngl_node **new_nodes = realloc(sctx->nodes, sctx->nb_allocated_nodes * sizeof(*new_nodes));
        if (!new_nodes)
            return -1;
        sctx->nodes = new_nodes;
    }
    sctx->nodes[sctx->nb_nodes++] = (struct ngl_node *)node;
    return 0;
}

#define CASE_LITERAL(param_type, type, parse_func)      \
case param_type: {                                      \
    type v;                                             \
    len = parse_func(str, &v);                          \
    if (len < 0)                                        \
        return -1;                                      \
    int ret = ngli_params_vset(base_ptr, par, v);       \
    if (ret < 0)                                        \
        return ret;                                     \
    break;                                              \
}

static int parse_int(const char *s, int *valp)
{
    char *endptr = NULL;
    *valp = (int)strtol(s, &endptr, 10);
    return (int)(endptr - s);
}

static int parse_hexint(const char *s, int *valp)
{
    char *endptr = NULL;
    *valp = (int)strtol(s, &endptr, 16);
    return (int)(endptr - s);
}

static int64_t parse_i64(const char *s, int64_t *valp)
{
    char *endptr = NULL;
    *valp = strtoll(s, &endptr, 10);
    return (int)(endptr - s);
}

static int parse_bool(const char *s, int *valp)
{
    int ret = parse_int(s, valp);
    if (ret < 0)
        return ret;
    if (*valp != -1)
        *valp = !!*valp;
    return ret;
}

#define DECLARE_FLT_PARSE_FUNC(type, nbit, shift_exp)                       \
static int parse_##type(const char *s, type *valp)                          \
{                                                                           \
    int consumed = 0;                                                       \
    union { uint##nbit##_t i; type f; } u = {.i = 0};                       \
                                                                            \
    if (*s == '-') {                                                        \
        u.i = 1ULL << (nbit - 1);                                           \
        consumed++;                                                         \
    }                                                                       \
                                                                            \
    char *endptr = NULL;                                                    \
    uint##nbit##_t exp = nbit == 64 ? strtoull(s + consumed, &endptr, 16)   \
                                    : strtoul(s + consumed, &endptr, 16);   \
    if (*endptr++ != 'z')                                                   \
        return -1;                                                          \
    uint##nbit##_t mant = nbit == 64 ? strtoull(endptr, &endptr, 16)        \
                                     : strtoul(endptr, &endptr, 16);        \
                                                                            \
    u.i |= exp<<shift_exp | mant;                                           \
                                                                            \
    *valp = u.f;                                                            \
    return (int)(endptr - s);                                               \
}

DECLARE_FLT_PARSE_FUNC(float,  32, 23)
DECLARE_FLT_PARSE_FUNC(double, 64, 52)

#define DECLARE_PARSE_LIST_FUNC(type, parse_func)                           \
static int parse_func##s(const char *s, type **valsp, int *nb_valsp)        \
{                                                                           \
    type *vals = NULL;                                                      \
    int nb_vals = 0, consumed = 0, len;                                     \
                                                                            \
    for (;;) {                                                              \
        type v;                                                             \
        len = parse_func(s, &v);                                            \
        if (len < 0) {                                                      \
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

DECLARE_PARSE_LIST_FUNC(float,  parse_float)
DECLARE_PARSE_LIST_FUNC(double, parse_double)
DECLARE_PARSE_LIST_FUNC(int,    parse_hexint)

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
        int n = sscanf(s, "%63[^=]=%x%n", key, &val, &len);
        if (n != 2) {
            consumed = -1;
            break;
        }

        char **new_keys = realloc(keys, (nb_vals + 1) * sizeof(*new_keys));
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
    int len = -1;

    switch (par->type) {
        CASE_LITERAL(PARAM_TYPE_INT, int,     parse_int)
        CASE_LITERAL(PARAM_TYPE_BOOL,int,     parse_bool)
        CASE_LITERAL(PARAM_TYPE_I64, int64_t, parse_i64)
        CASE_LITERAL(PARAM_TYPE_DBL, double,  parse_double)

        case PARAM_TYPE_RATIONAL: {
            int r[2] = {0};
            int ret = sscanf(str, "%d/%d%n", &r[0], &r[1], &len);
            if (ret != 2)
                return -1;
            ret = ngli_params_vset(base_ptr, par, r[0], r[1]);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_FLAGS:
        case PARAM_TYPE_SELECT: {
            len = strcspn(str, " \n");
            char *s = malloc(len + 1);
            if (!s)
                return -1;
            memcpy(s, str, len);
            s[len] = 0;
            int ret = ngli_params_vset(base_ptr, par, s);
            free(s);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_STR: {
            len = strcspn(str, " \n");
            char *s = malloc(len + 1);
            if (!s)
                return -1;
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
            int ret = ngli_params_vset(base_ptr, par, sstart);
            free(sstart);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_DATA: {
            int size = 0;
            int consumed = 0;
            const char *cur = str;
            const char *end = str + strlen(str);
            int ret = sscanf(str, "%d,%n", &size, &consumed);
            if (ret != 1)
                return -1;
            if (!size)
                break;
            if (cur >= end - consumed)
                return -1;
            cur += consumed;
            uint8_t *data = calloc(size, sizeof(*data));
            if (!data)
                return -1;
            for (int i = 0; i < size; i++) {
                if (cur > end - 2) {
                    free(data);
                    return -1;
                }
                static const uint8_t hexm[256] = {
                    ['0'] = 0x0, ['1'] = 0x1, ['2'] = 0x2, ['3'] = 0x3,
                    ['4'] = 0x4, ['5'] = 0x5, ['6'] = 0x6, ['7'] = 0x7,
                    ['8'] = 0x8, ['9'] = 0x9, ['a'] = 0xa, ['b'] = 0xb,
                    ['c'] = 0xc, ['d'] = 0xd, ['e'] = 0xe, ['f'] = 0xf,
                };
                data[i] = hexm[(uint8_t)cur[0]]<<4 | hexm[(uint8_t)cur[1]];
                cur += 2;
            }
            ret = ngli_params_vset(base_ptr, par, size, data);
            free(data);
            if (ret < 0)
                return ret;
            len = cur - str;
            break;
        }

        case PARAM_TYPE_VEC2:
        case PARAM_TYPE_VEC3:
        case PARAM_TYPE_VEC4: {
            const int n = par->type - PARAM_TYPE_VEC2 + 2;
            float *v = NULL;
            int nb_flts;
            len = parse_floats(str, &v, &nb_flts);
            if (len < 0 || nb_flts != n) {
                free(v);
                return -1;
            }
            int ret = ngli_params_vset(base_ptr, par, v);
            free(v);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_MAT4: {
            float *m = NULL;
            int nb_flts;
            len = parse_floats(str, &m, &nb_flts);
            if (len < 0 || nb_flts != 16) {
                free(m);
                return -1;
            }
            int ret = ngli_params_vset(base_ptr, par, m);
            free(m);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_NODE: {
            int node_id;
            len = parse_hexint(str, &node_id);
            if (len < 0 || node_id < 0 || node_id >= sctx->nb_nodes)
                return -1;
            struct ngl_node *node = sctx->nodes[node_id];
            int ret = ngli_params_vset(base_ptr, par, node);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_NODELIST: {
            int *node_ids, nb_node_ids;
            len = parse_hexints(str, &node_ids, &nb_node_ids);
            if (len < 0)
                return -1;
            for (int i = 0; i < nb_node_ids; i++) {
                const int node_id = node_ids[i];
                if (node_id < 0 || node_id >= sctx->nb_nodes) {
                    free(node_ids);
                    return -1;
                }
                struct ngl_node *node = sctx->nodes[node_id];
                int ret = ngli_params_add(base_ptr, par, 1, &node);
                if (ret < 0) {
                    free(node_ids);
                    return ret;
                }
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
            int ret = ngli_params_add(base_ptr, par, nb_dbls, dbls);
            free(dbls);
            if (ret < 0)
                return ret;
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
                int ret = ngli_params_vset(base_ptr, par, key, node);
                if (ret < 0) {
                    FREE_KVS(nb_nodes, node_keys, node_ids);
                    return ret;
                }
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

        if (!(par->flags & PARAM_FLAG_CONSTRUCTOR))
            break;

        int ret = parse_param(sctx, base_ptr, par, str);
        if (ret < 0) {
            LOG(ERROR, "Invalid value specified for parameter %s.%s",
                node->class->name, par->key);
            return -1;
        }

        str += ret;
        if (*str != ' ')
            break;
        str++;
    }

    for (;;) {
        char *eok = strchr(str, ':');
        if (!eok)
            break;
        *eok = 0;

        const struct node_param *par = ngli_node_param_find(node, str, &base_ptr);
        if (!par) {
            LOG(ERROR, "Unable to find parameter %s.%s",
                node->class->name, str);
            return -1;
        }

        str = eok + 1;
        int ret = parse_param(sctx, base_ptr, par, str);
        if (ret < 0) {
            LOG(ERROR, "Invalid value specified for parameter %s.%s",
                node->class->name, par->key);
            return -1;
        }

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
    if (*s == '\n')
        s++;

    while (s < end - 4) {
        const int type = NGLI_FOURCC(s[0], s[1], s[2], s[3]);
        s += 4;
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

        ret = set_node_params(&sctx, s, node);
        if (ret < 0) {
            ngl_node_unrefp(&node);
            break;
        }

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
