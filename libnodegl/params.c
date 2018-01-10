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

#include "bstr.h"
#include "log.h"
#include "hmap.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"
#include "utils.h"

const struct param_specs ngli_params_specs[] = {
    [PARAM_TYPE_INT] = {
        .name = "int",
        .size = sizeof(int),
    },
    [PARAM_TYPE_I64] = {
        .name = "i64",
        .size = sizeof(int64_t),
    },
    [PARAM_TYPE_DBL] = {
        .name = "double",
        .size = sizeof(double),
    },
    [PARAM_TYPE_STR] = {
        .name = "string",
        .size = sizeof(char *),
    },
    [PARAM_TYPE_DATA] = {
        .name = "data",
        .size = sizeof(void *) + sizeof(int),
    },
    [PARAM_TYPE_VEC2] = {
        .name = "vec2",
        .size = sizeof(float[2]),
    },
    [PARAM_TYPE_VEC3] = {
        .name = "vec3",
        .size = sizeof(float[3]),
    },
    [PARAM_TYPE_VEC4] = {
        .name = "vec4",
        .size = sizeof(float[4]),
    },
    [PARAM_TYPE_MAT4] = {
        .name = "mat4",
        .size = sizeof(float[4*4]),
    },
    [PARAM_TYPE_NODE] = {
        .name = "Node",
        .size = sizeof(struct ngl_node *),
    },
    [PARAM_TYPE_NODELIST] = {
        .name = "NodeList",
        .size = sizeof(struct ngl_node **) + sizeof(int),
    },
    [PARAM_TYPE_DBLLIST] = {
        .name = "doubleList",
        .size = sizeof(double *) + sizeof(int),
    },
    [PARAM_TYPE_NODEDICT] = {
        .name = "NodeDict",
        .size = sizeof(struct hmap *),
    },
    [PARAM_TYPE_SELECT] = {
        .name = "select",
        .size = sizeof(int),
    },
    [PARAM_TYPE_FLAGS] = {
        .name = "flags",
        .size = sizeof(int),
    },
};

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

int ngli_params_get_select_val(const struct param_const *consts, const char *s, int *dst)
{
    for (int i = 0; consts[i].key; i++) {
        if (!strcmp(consts[i].key, s)) {
            *dst = consts[i].value;
            return 0;
        }
    }
    return -1;
}

const char *ngli_params_get_select_str(const struct param_const *consts, int val)
{
    for (int i = 0; consts[i].key; i++)
        if (consts[i].value == val)
            return consts[i].key;
    return NULL;
}

/* The first separator (`+`) is used as default separator because `|` can not
 * be used due to markdown table formatting, and ` ` will cause parsing issue
 * in the serialization.
 */
#define FLAGS_SEP "+| "

int ngli_params_get_flags_val(const struct param_const *consts, const char *s, int *dst)
{
    *dst = 0;
    if (!strcmp(s, "0"))
        return 0;
    if (!strcmp(s, "unset")) {
        *dst = -1;
        return 0;
    }
    while (*s) {
        int i;
        const size_t len = strcspn(s, FLAGS_SEP);
        for (i = 0; consts[i].key; i++) {
            if (!strncmp(consts[i].key, s, len)) {
                /* -1 is a reserved value and means that the mask is unset */
                ngli_assert(consts[i].value != -1);
                *dst |= consts[i].value;
                break;
            }
        }
        if (!consts[i].key) {
            LOG(ERROR, "unrecognized \"%.*s\" flag", (int)len, s);
            return -1;
        }
        s += len;
        while (*s && strchr(FLAGS_SEP, *s))
            s++;
    }
    return 0;
}

char *ngli_params_get_flags_str(const struct param_const *consts, int val)
{
    if (!val)
        return ngli_strdup("0");

    if (val == -1)
        return ngli_strdup("unset");

    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    for (int i = 0; consts[i].key; i++)
        if (val & consts[i].value)
            ngli_bstr_print(b, "%.1s%s", *ngli_bstr_strptr(b) ? FLAGS_SEP : "", consts[i].key);

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

void ngli_params_bstr_print_val(struct bstr *b, uint8_t *base_ptr, const struct node_param *par)
{
    switch (par->type) {
        case PARAM_TYPE_DBL: {
            const double v = *(double *)(base_ptr + par->offset);
            ngli_bstr_print(b, "%g", v);
            break;
        }
        case PARAM_TYPE_SELECT: {
            const int v = *(int *)(base_ptr + par->offset);
            const char *s = ngli_params_get_select_str(par->choices->consts, v);
            ngli_assert(s);
            ngli_bstr_print(b, "%s", s);
            break;
        }
        case PARAM_TYPE_FLAGS: {
            const int v = *(int *)(base_ptr + par->offset);
            char *s = ngli_params_get_flags_str(par->choices->consts, v);
            if (!s)
                break;
            ngli_assert(*s);
            ngli_bstr_print(b, "%s", s);
            free(s);
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
        case PARAM_TYPE_MAT4: {
            const float *m = (const float *)(base_ptr + par->offset);
            ngli_bstr_print(b,
                            "(%g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g)",
                            m[ 0], m[ 1], m[ 2], m[ 3],
                            m[ 4], m[ 5], m[ 6], m[ 7],
                            m[ 8], m[ 9], m[10], m[11],
                            m[12], m[13], m[14], m[15]);
            break;
        }
        case PARAM_TYPE_STR: {
            const char *s = *(const char **)(base_ptr + par->offset);
            if (!s)
                ngli_bstr_print(b, "\"\"");
            else if (strchr(s, '\n')) // print a checksum when the string is multiline (typically, shaders)
                ngli_bstr_print(b, "%08X <i>(CRC32)</i>", ngli_crc32(s));
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
        case PARAM_TYPE_SELECT: {
            int v;
            const char *s = va_arg(*ap, const char *);
            int ret = ngli_params_get_select_val(par->choices->consts, s, &v);
            if (ret < 0) {
                LOG(ERROR, "unrecognized constant \"%s\" for option %s", s, par->key);
                return ret;
            }
            LOG(VERBOSE, "set %s to %s (%d)", par->key, s, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_FLAGS: {
            int v;
            const char *s = va_arg(*ap, const char *);
            int ret = ngli_params_get_flags_val(par->choices->consts, s, &v);
            if (ret < 0) {
                LOG(ERROR, "unrecognized flags \"%s\" for option %s", s, par->key);
                return ret;
            }
            LOG(VERBOSE, "set %s to %s (%d)", par->key, s, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
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
        case PARAM_TYPE_DATA: {
            int size = va_arg(*ap, int);
            void *data = va_arg(*ap, void *);
            LOG(VERBOSE, "set %s to %p (of size %d)", par->key, data, size);
            uint8_t **dst = (uint8_t **)dstp;

            free(*dst);
            *dst = NULL;
            if (data && size) {
                *dst = malloc(size);
                if (!*dst)
                    return -1;
                memcpy(*dst, data, size);
            } else {
                size = 0;
            }
            memcpy(dstp + sizeof(void *), &size, sizeof(size));
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
        case PARAM_TYPE_MAT4: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE,
                "set %s to (%g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g)",
                par->key,
                v[ 0], v[ 1], v[ 2], v[ 3],
                v[ 4], v[ 5], v[ 6], v[ 7],
                v[ 8], v[ 9], v[10], v[11],
                v[12], v[13], v[14], v[15]);
            memcpy(dstp, v, 16 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_NODE: {
            struct ngl_node *node = va_arg(*ap, struct ngl_node *);
            if (!allowed_node(node, par->node_types)) {
                LOG(ERROR, "%s (%s) is not an allowed type for %s",
                    node->name, node->class->name, par->key);
                return -1;
            }
            ngl_node_unrefp((struct ngl_node **)dstp);
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

    if (par->flags & PARAM_FLAG_USER_SET) {
        dstp += ngli_params_specs[par->type].size;
        *(int *)dstp = 1;
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
                case PARAM_TYPE_SELECT: {
                    const int v = (int)par->def_value.i64;
                    const char *s = ngli_params_get_select_str(par->choices->consts, v);
                    ngli_assert(s);
                    ngli_params_vset(base_ptr, par, s);
                    break;
                }
                case PARAM_TYPE_FLAGS: {
                    const int v = (int)par->def_value.i64;
                    char *s = ngli_params_get_flags_str(par->choices->consts, v);
                    if (!s)
                        return -1;
                    ngli_assert(*s);
                    ngli_params_vset(base_ptr, par, s);
                    free(s);
                    break;
                }
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
                case PARAM_TYPE_MAT4:
                    ngli_params_vset(base_ptr, par, par->def_value.mat);
                    break;
                case PARAM_TYPE_DATA:
                    ngli_params_vset(base_ptr, par, 0, par->def_value.p);
                    break;
            }

            if (par->flags & PARAM_FLAG_USER_SET) {
                uint8_t *dstp = base_ptr + par->offset + ngli_params_specs[par->type].size;
                *(int *)dstp = 0;
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

    if (par->flags & PARAM_FLAG_USER_SET) {
        uint8_t *dstp = base_ptr + par->offset + ngli_params_specs[par->type].size;
        *(int *)dstp = 1;
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
            case PARAM_TYPE_DATA: {
                uint8_t *data = *(uint8_t **)parp;
                free(data);
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
