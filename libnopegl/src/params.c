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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "internal.h"
#include "log.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "params.h"
#include "utils/bstr.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "utils/crc32.h"
#include "utils/string.h"
#include "utils/utils.h"

/* We depend on the monotonically incrementing by 1 property of these fields */
NGLI_STATIC_ASSERT(param_vec,  NGLI_PARAM_TYPE_VEC4  - NGLI_PARAM_TYPE_VEC2 == 2);
NGLI_STATIC_ASSERT(param_ivec, NGLI_PARAM_TYPE_IVEC4 - NGLI_PARAM_TYPE_I32  == 3);
NGLI_STATIC_ASSERT(param_uvec, NGLI_PARAM_TYPE_UVEC4 - NGLI_PARAM_TYPE_U32  == 3);

const struct param_specs ngli_params_specs[] = {
    [NGLI_PARAM_TYPE_I32] = {
        .name = "i32",
        .size = sizeof(int32_t),
        .desc = NGLI_DOCSTRING("32-bit integer"),
    },
    [NGLI_PARAM_TYPE_IVEC2] = {
        .name = "ivec2",
        .size = sizeof(int32_t[2]),
        .desc = NGLI_DOCSTRING("2 32-bit integers"),
    },
    [NGLI_PARAM_TYPE_IVEC3] = {
        .name = "ivec3",
        .size = sizeof(int32_t[3]),
        .desc = NGLI_DOCSTRING("3 32-bit integers"),
    },
    [NGLI_PARAM_TYPE_IVEC4] = {
        .name = "ivec4",
        .size = sizeof(int32_t[4]),
        .desc = NGLI_DOCSTRING("4 32-bit integers"),
    },
    [NGLI_PARAM_TYPE_U32] = {
        .name = "u32",
        .size = sizeof(uint32_t),
        .desc = NGLI_DOCSTRING("32-bit unsigned integer"),
    },
    [NGLI_PARAM_TYPE_UVEC2] = {
        .name = "uvec2",
        .size = sizeof(uint32_t[2]),
        .desc = NGLI_DOCSTRING("2 32-bit unsigned integers"),
    },
    [NGLI_PARAM_TYPE_UVEC3] = {
        .name = "uvec3",
        .size = sizeof(uint32_t[3]),
        .desc = NGLI_DOCSTRING("3 32-bit unsigned integers"),
    },
    [NGLI_PARAM_TYPE_UVEC4] = {
        .name = "uvec4",
        .size = sizeof(uint32_t[4]),
        .desc = NGLI_DOCSTRING("4 32-bit unsigned integers"),
    },
    [NGLI_PARAM_TYPE_BOOL] = {
        .name = "bool",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Boolean (map to `int` in C)"),
    },
    [NGLI_PARAM_TYPE_F64] = {
        .name = "f64",
        .size = sizeof(double),
        .desc = NGLI_DOCSTRING("64-bit float"),
    },
    [NGLI_PARAM_TYPE_STR] = {
        .name = "str",
        .size = sizeof(char *),
        .desc = NGLI_DOCSTRING("String"),
    },
    [NGLI_PARAM_TYPE_DATA] = {
        .name = "data",
        .size = sizeof(void *) + sizeof(size_t),
        .desc = NGLI_DOCSTRING("Agnostic data buffer"),
    },
    [NGLI_PARAM_TYPE_F32] = {
        .name = "f32",
        .size = sizeof(float),
        .desc = NGLI_DOCSTRING("32-bit float"),
    },
    [NGLI_PARAM_TYPE_VEC2] = {
        .name = "vec2",
        .size = sizeof(float[2]),
        .desc = NGLI_DOCSTRING("2 32-bit floats"),
    },
    [NGLI_PARAM_TYPE_VEC3] = {
        .name = "vec3",
        .size = sizeof(float[3]),
        .desc = NGLI_DOCSTRING("3 32-bit floats"),
    },
    [NGLI_PARAM_TYPE_VEC4] = {
        .name = "vec4",
        .size = sizeof(float[4]),
        .desc = NGLI_DOCSTRING("4 32-bit floats"),
    },
    [NGLI_PARAM_TYPE_MAT4] = {
        .name = "mat4",
        .size = sizeof(float[4*4]),
        .desc = NGLI_DOCSTRING("4x4 32-bit floats, column major memory layout "
                               "(the first 4 floats corresponds to 1 mathematical matrix column)"),
    },
    [NGLI_PARAM_TYPE_NODE] = {
        .name = "node",
        .size = sizeof(struct ngl_node *),
        .desc = NGLI_DOCSTRING("nope.gl Node"),
    },
    [NGLI_PARAM_TYPE_NODELIST] = {
        .name = "node_list",
        .size = sizeof(struct ngl_node **) + sizeof(size_t),
        .desc = NGLI_DOCSTRING("List of nope.gl Node"),
    },
    [NGLI_PARAM_TYPE_F64LIST] = {
        .name = "f64_list",
        .size = sizeof(double *) + sizeof(size_t),
        .desc = NGLI_DOCSTRING("List of 64-bit floats"),
    },
    [NGLI_PARAM_TYPE_NODEDICT] = {
        .name = "node_dict",
        .size = sizeof(struct hmap *),
        .desc = NGLI_DOCSTRING("Dictionary mapping arbitrary string identifiers to nope.gl Nodes"),
    },
    [NGLI_PARAM_TYPE_SELECT] = {
        .name = "select",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Selection of one constant (expressed as a string)"),
    },
    [NGLI_PARAM_TYPE_FLAGS] = {
        .name = "flags",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Combination of constants (expressed as strings), using `+` as separator. Can be empty for none."),
    },
    [NGLI_PARAM_TYPE_RATIONAL] = {
        .name = "rational",
        .size = sizeof(int32_t[2]),
        .desc = NGLI_DOCSTRING("Rational number (expressed as 2 32-bit integers, respectively as numerator and denominator)"),
    },
};

static const char *get_param_type_name(enum param_type param_type)
{
    if (param_type >= NGLI_ARRAY_NB(ngli_params_specs))
        return "???";
    return ngli_params_specs[param_type].name;
}

const struct node_param *ngli_params_find(const struct node_param *params, const char *key)
{
    if (!params)
        return NULL;
    for (size_t i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];
        if (!strcmp(par->key, key))
            return par;
    }
    return NULL;
}

int ngli_params_get_select_val(const struct param_const *consts, const char *s, int *dst)
{
    for (size_t i = 0; consts[i].key; i++) {
        if (!strcmp(consts[i].key, s)) {
            *dst = consts[i].value;
            return 0;
        }
    }
    return NGL_ERROR_INVALID_ARG;
}

const char *ngli_params_get_select_str(const struct param_const *consts, int val)
{
    for (size_t i = 0; consts[i].key; i++)
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
            return NGL_ERROR_INVALID_ARG;
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

    for (size_t i = 0; consts[i].key; i++)
        if (val & consts[i].value)
            ngli_bstr_printf(b, "%.1s%s", *ngli_bstr_strptr(b) ? FLAGS_SEP : "", consts[i].key);

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

void ngli_params_bstr_print_val(struct bstr *b, uint8_t *base_ptr, const struct node_param *par)
{
    const uint8_t *srcp = base_ptr + par->offset;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        srcp += sizeof(struct ngl_node *);

    switch (par->type) {
        case NGLI_PARAM_TYPE_SELECT: {
            const int v = *(const int *)srcp;
            const char *s = ngli_params_get_select_str(par->choices->consts, v);
            ngli_assert(s);
            ngli_bstr_print(b, s);
            break;
        }
        case NGLI_PARAM_TYPE_FLAGS: {
            const int v = *(const int *)srcp;
            char *s = ngli_params_get_flags_str(par->choices->consts, v);
            if (!s)
                break;
            ngli_assert(*s);
            ngli_bstr_print(b, s);
            ngli_free(s);
            break;
        }
        case NGLI_PARAM_TYPE_BOOL: {
            const int v = *(const int *)srcp;
            if (v == -1)
                ngli_bstr_print(b, "unset");
            else
                ngli_bstr_printf(b, "%d", v);
            break;
        }
        case NGLI_PARAM_TYPE_F32:    ngli_bstr_printf(b, "%g",            *(const float *)srcp);                  break;
        case NGLI_PARAM_TYPE_F64:    ngli_bstr_printf(b, "%g",            *(const double *)srcp);                 break;
        case NGLI_PARAM_TYPE_I32:    ngli_bstr_printf(b, "%d",            *(const int32_t *)srcp);                break;
        case NGLI_PARAM_TYPE_U32:    ngli_bstr_printf(b, "%u",            *(const uint32_t *)srcp);               break;
        case NGLI_PARAM_TYPE_IVEC2:  ngli_bstr_printf(b, "(%d,%d)",       NGLI_ARG_VEC2((const int32_t *)srcp));  break;
        case NGLI_PARAM_TYPE_IVEC3:  ngli_bstr_printf(b, "(%d,%d,%d)",    NGLI_ARG_VEC3((const int32_t *)srcp));  break;
        case NGLI_PARAM_TYPE_IVEC4:  ngli_bstr_printf(b, "(%d,%d,%d,%d)", NGLI_ARG_VEC4((const int32_t *)srcp));  break;
        case NGLI_PARAM_TYPE_UVEC2:  ngli_bstr_printf(b, "(%u,%u)",       NGLI_ARG_VEC2((const uint32_t *)srcp)); break;
        case NGLI_PARAM_TYPE_UVEC3:  ngli_bstr_printf(b, "(%u,%u,%u)",    NGLI_ARG_VEC3((const uint32_t *)srcp)); break;
        case NGLI_PARAM_TYPE_UVEC4:  ngli_bstr_printf(b, "(%u,%u,%u,%u)", NGLI_ARG_VEC4((const uint32_t *)srcp)); break;
        case NGLI_PARAM_TYPE_VEC2:   ngli_bstr_printf(b, "(%g,%g)",       NGLI_ARG_VEC2((const float *)srcp));    break;
        case NGLI_PARAM_TYPE_VEC3:   ngli_bstr_printf(b, "(%g,%g,%g)",    NGLI_ARG_VEC3((const float *)srcp));    break;
        case NGLI_PARAM_TYPE_VEC4:   ngli_bstr_printf(b, "(%g,%g,%g,%g)", NGLI_ARG_VEC4((const float *)srcp));    break;
        case NGLI_PARAM_TYPE_MAT4:
            ngli_bstr_printf(b, "(%g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g)", NGLI_ARG_MAT4((const float *)srcp));
            break;
        case NGLI_PARAM_TYPE_STR: {
            const char *s = *(const char **)srcp;
            if (!s)
                ngli_bstr_print(b, "\"\"");
            else if (strchr(s, '\n')) // print a checksum when the string is multiline (typically, shaders)
                ngli_bstr_printf(b, "%08X <i>(CRC32)</i>", ngli_crc32(s));
            else if (strlen(s) > 32)
                ngli_bstr_printf(b, "\"...%s\"", s + strlen(s) - 32);
            else
                ngli_bstr_printf(b, "\"%s\"", s);
            break;
        }
        case NGLI_PARAM_TYPE_F64LIST: {
            const uint8_t *nb_elems_p = srcp + sizeof(double *);
            const double *elems = *(const double **)srcp;
            const size_t nb_elems = *(const size_t *)nb_elems_p;
            for (size_t i = 0; i < nb_elems; i++)
                ngli_bstr_printf(b, "%s%g", i ? "," : "", elems[i]);
            break;
        }
        case NGLI_PARAM_TYPE_RATIONAL:
            ngli_bstr_printf(b, "%d/%d", NGLI_ARG_VEC2((const int32_t *)srcp));
            break;
        default:
            break;
    }
}

static int allowed_node(const struct ngl_node *node, const uint32_t *allowed_ids)
{
    if (!allowed_ids)
        return 1;
    const uint32_t id = node->cls->id;
    for (size_t i = 0; allowed_ids[i] != NGLI_NODE_NONE; i++)
        if (id == allowed_ids[i])
            return 1;
    return 0;
}

static void node_hmap_free(void *user_arg, void *data)
{
    struct ngl_node *node = data;
    ngl_node_unrefp(&node);
}

static int check_param_type(const struct node_param *par, enum param_type expected_type)
{
    if (par->type != expected_type) {
        LOG(ERROR, "invalid type: %s is of type %s, not %s", par->key,
            get_param_type_name(par->type), get_param_type_name(expected_type));
        return NGL_ERROR_INVALID_ARG;
    }
    return 0;
}

int ngli_params_set_bool(uint8_t *dstp, const struct node_param *par, int value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_BOOL);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    if (value != -1)
        value = !!value;
    LOG(VERBOSE, "set %s to %d", par->key, value);
    memcpy(dstp, &value, sizeof(value));
    return 0;
}

int ngli_params_set_data(uint8_t *dstp, const struct node_param *par, size_t size, const void *data)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_DATA);
    if (ret < 0)
        return ret;

    LOG(VERBOSE, "set %s to %p (of size %zu)", par->key, data, size);
    uint8_t **dst = (uint8_t **)dstp;

    ngli_freep(dst);
    if (data && size) {
        *dst = ngli_memdup(data, size);
        if (!*dst)
            return NGL_ERROR_MEMORY;
    } else {
        size = 0;
    }
    memcpy(dstp + sizeof(void *), &size, sizeof(size));
    return 0;
}

int ngli_params_set_dict(uint8_t *dstp, const struct node_param *par, const char *name, struct ngl_node *node)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_NODEDICT);
    if (ret < 0)
        return ret;

    if (node && !allowed_node(node, par->node_types)) {
        LOG(ERROR, "%s (%s) is not an allowed type for %s",
            node->label, node->cls->name, par->key);
        return NGL_ERROR_INVALID_ARG;
    }
    LOG(VERBOSE, "set %s to (%s,%p)", par->key, name, node);
    struct hmap **hmapp = (struct hmap **)dstp;
    if (!*hmapp) {
        *hmapp = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
        if (!*hmapp)
            return NGL_ERROR_MEMORY;
        ngli_hmap_set_free_func(*hmapp, node_hmap_free, NULL);
    }

    ret = ngli_hmap_set_str(*hmapp, name, node);
    if (ret < 0)
        return ret;
    if (node)
        ngl_node_ref(node);
    return 0;
}

int ngli_params_set_f32(uint8_t *dstp, const struct node_param *par, float value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_F32);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to %g", par->key, value);
    memcpy(dstp, &value, sizeof(value));
    return 0;
}

int ngli_params_set_f64(uint8_t *dstp, const struct node_param *par, double value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_F64);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to %g", par->key, value);
    memcpy(dstp, &value, sizeof(value));
    return 0;
}

int ngli_params_set_flags(uint8_t *dstp, const struct node_param *par, const char *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_FLAGS);
    if (ret < 0)
        return ret;

    int v;
    ret = ngli_params_get_flags_val(par->choices->consts, value, &v);
    if (ret < 0) {
        LOG(ERROR, "unrecognized flags \"%s\" for option %s", value, par->key);
        return ret;
    }
    LOG(VERBOSE, "set %s to %s (%d)", par->key, value, v);
    memcpy(dstp, &v, sizeof(v));
    return 0;
}

int ngli_params_set_i32(uint8_t *dstp, const struct node_param *par, int32_t value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_I32);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to %d", par->key, value);
    memcpy(dstp, &value, sizeof(value));
    return 0;
}

int ngli_params_set_ivec2(uint8_t *dstp, const struct node_param *par, const int32_t *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_IVEC2);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%d,%d)", par->key, NGLI_ARG_VEC2(value));
    memcpy(dstp, value, 2 * sizeof(*value));
    return 0;
}

int ngli_params_set_ivec3(uint8_t *dstp, const struct node_param *par, const int32_t *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_IVEC3);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%d,%d,%d)", par->key, NGLI_ARG_VEC3(value));
    memcpy(dstp, value, 3 * sizeof(*value));
    return 0;
}

int ngli_params_set_ivec4(uint8_t *dstp, const struct node_param *par, const int32_t *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_IVEC4);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%d,%d,%d,%d)", par->key, NGLI_ARG_VEC4(value));
    memcpy(dstp, value, 4 * sizeof(*value));
    return 0;
}

int ngli_params_set_mat4(uint8_t *dstp, const struct node_param *par, const float *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_MAT4);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g)", par->key, NGLI_ARG_MAT4(value));
    memcpy(dstp, value, 16 * sizeof(*value));
    return 0;
}

static const uint32_t * const param_type_to_nodes[] = {
    [NGLI_PARAM_TYPE_BOOL] = (const uint32_t[]){
        NGL_NODE_UNIFORMBOOL,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_F32] = (const uint32_t[]){
        NGL_NODE_ANIMATEDFLOAT,
        NGL_NODE_EVALFLOAT,
        NGL_NODE_NOISEFLOAT,
        NGL_NODE_STREAMEDFLOAT,
        NGL_NODE_TIME,
        NGL_NODE_UNIFORMFLOAT,
        NGL_NODE_VELOCITYFLOAT,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_I32] = (const uint32_t[]){
        NGL_NODE_STREAMEDINT,
        NGL_NODE_UNIFORMINT,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_IVEC2] = (const uint32_t[]){
        NGL_NODE_STREAMEDIVEC2,
        NGL_NODE_UNIFORMIVEC2,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_IVEC3] = (const uint32_t[]){
        NGL_NODE_STREAMEDIVEC3,
        NGL_NODE_UNIFORMIVEC3,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_IVEC4] = (const uint32_t[]){
        NGL_NODE_STREAMEDIVEC4,
        NGL_NODE_UNIFORMIVEC4,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_MAT4] = (const uint32_t[]){
        NGL_NODE_ANIMATEDQUAT,
        NGL_NODE_STREAMEDMAT4,
        NGL_NODE_UNIFORMMAT4,
        NGL_NODE_UNIFORMQUAT,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_RATIONAL] = (const uint32_t[]){
        NGL_NODE_STREAMEDIVEC2,
        NGL_NODE_UNIFORMIVEC2,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_U32] = (const uint32_t[]){
        NGL_NODE_STREAMEDUINT,
        NGL_NODE_UNIFORMUINT,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_UVEC2] = (const uint32_t[]){
        NGL_NODE_STREAMEDUIVEC2,
        NGL_NODE_UNIFORMUIVEC2,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_UVEC3] = (const uint32_t[]){
        NGL_NODE_STREAMEDUIVEC3,
        NGL_NODE_UNIFORMUIVEC3,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_UVEC4] = (const uint32_t[]){
        NGL_NODE_STREAMEDUIVEC4,
        NGL_NODE_UNIFORMUIVEC4,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_VEC2] = (const uint32_t[]){
        NGL_NODE_ANIMATEDVEC2,
        NGL_NODE_EVALVEC2,
        NGL_NODE_NOISEVEC2,
        NGL_NODE_STREAMEDVEC2,
        NGL_NODE_UNIFORMVEC2,
        NGL_NODE_VELOCITYVEC2,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_VEC3] = (const uint32_t[]){
        NGL_NODE_ANIMATEDCOLOR,
        NGL_NODE_ANIMATEDPATH,
        NGL_NODE_ANIMATEDVEC3,
        NGL_NODE_EVALVEC3,
        NGL_NODE_NOISEVEC3,
        NGL_NODE_STREAMEDVEC3,
        NGL_NODE_UNIFORMCOLOR,
        NGL_NODE_UNIFORMVEC3,
        NGL_NODE_VELOCITYVEC3,
        NGLI_NODE_NONE
    },
    [NGLI_PARAM_TYPE_VEC4] = (const uint32_t[]){
        NGL_NODE_ANIMATEDQUAT,
        NGL_NODE_ANIMATEDVEC4,
        NGL_NODE_EVALVEC4,
        NGL_NODE_NOISEVEC4,
        NGL_NODE_STREAMEDVEC4,
        NGL_NODE_UNIFORMQUAT,
        NGL_NODE_UNIFORMVEC4,
        NGL_NODE_VELOCITYVEC4,
        NGLI_NODE_NONE
    },
};

int ngli_params_set_node(uint8_t *dstp, const struct node_param *par, struct ngl_node *node)
{
    if (par->type == NGLI_PARAM_TYPE_NODE) {
        if (!allowed_node(node, par->node_types)) {
            LOG(ERROR, "%s (%s) is not an allowed type for %s",
                node->label, node->cls->name, par->key);
            return NGL_ERROR_INVALID_ARG;
        }
    } else {
        if (!(par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)) {
            LOG(ERROR, "parameter %s doesn't accept nodes", par->key);
            return NGL_ERROR_INVALID_ARG;
        }

        /*
         * The asserts can only be triggered in case of a bug: that is, if the
         * code has the NGLI_PARAM_FLAG_ALLOW_NODE flag set but no remapping
         * actually exists.
         */
        ngli_assert(par->type >= 0 && par->type < NGLI_ARRAY_NB(param_type_to_nodes));
        const uint32_t *node_types = param_type_to_nodes[par->type];
        ngli_assert(node_types);

        if (!allowed_node(node, node_types)) {
            LOG(ERROR, "node of type %s is not allowed for parameter %s (type %s)",
                node->cls->name, par->key, get_param_type_name(par->type));
            return NGL_ERROR_INVALID_ARG;
        }

        if (node->cls->id == NGL_NODE_ANIMATEDQUAT || node->cls->id == NGL_NODE_UNIFORMQUAT) {
            struct variable_opts *quat = node->opts;
            if (par->type == NGLI_PARAM_TYPE_MAT4 && !quat->as_mat4) {
                LOG(ERROR, "when setting a quaternion node for a mat4 parameter, as_mat4 must be set");
                return NGL_ERROR_INVALID_ARG;
            } else if (par->type == NGLI_PARAM_TYPE_VEC4 && quat->as_mat4) {
                LOG(ERROR, "when setting a quaternion node for a vec4 parameter, as_mat4 must not be set");
                return NGL_ERROR_INVALID_ARG;
            }
        }
    }
    ngl_node_unrefp((struct ngl_node **)dstp);
    ngl_node_ref(node);
    LOG(VERBOSE, "set %s to %s", par->key, node->label);
    memcpy(dstp, &node, sizeof(node));
    return 0;
}

int ngli_params_set_rational(uint8_t *dstp, const struct node_param *par, int32_t num, int32_t den)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_RATIONAL);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to %d/%d", par->key, num, den);
    memcpy(dstp, &num, sizeof(num));
    memcpy(dstp + sizeof(num), &den, sizeof(den));
    return 0;
}

int ngli_params_set_select(uint8_t *dstp, const struct node_param *par, const char *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_SELECT);
    if (ret < 0)
        return ret;

    int v;
    ret = ngli_params_get_select_val(par->choices->consts, value, &v);
    if (ret < 0) {
        LOG(ERROR, "unrecognized constant \"%s\" for option %s", value, par->key);
        return ret;
    }
    LOG(VERBOSE, "set %s to %s (%d)", par->key, value, v);
    memcpy(dstp, &v, sizeof(v));
    return 0;
}

int ngli_params_set_str(uint8_t *dstp, const struct node_param *par, const char *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_STR);
    if (ret < 0)
        return ret;

    char *s = NULL;
    if (!value)
        value = par->def_value.str;
    if (value) {
        s = ngli_strdup(value);
        if (!s)
            return NGL_ERROR_MEMORY;
        LOG(VERBOSE, "set %s to \"%s\"", par->key, s);
    } else {
        LOG(VERBOSE, "set %s to NULL", par->key);
    }
    ngli_free(*(char **)dstp);
    memcpy(dstp, &s, sizeof(s));
    return 0;
}

int ngli_params_set_u32(uint8_t *dstp, const struct node_param *par, const uint32_t value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_U32);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to %u", par->key, value);
    memcpy(dstp, &value, sizeof(value));
    return 0;
}

int ngli_params_set_uvec2(uint8_t *dstp, const struct node_param *par, const uint32_t *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_UVEC2);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%u,%u)", par->key, NGLI_ARG_VEC2(value));
    memcpy(dstp, value, 2 * sizeof(*value));
    return 0;
}

int ngli_params_set_uvec3(uint8_t *dstp, const struct node_param *par, const uint32_t *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_UVEC3);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%u,%u,%u)", par->key, NGLI_ARG_VEC3(value));
    memcpy(dstp, value, 3 * sizeof(*value));
    return 0;
}

int ngli_params_set_uvec4(uint8_t *dstp, const struct node_param *par, const uint32_t *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_UVEC4);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%u,%u,%u,%u)", par->key, NGLI_ARG_VEC4(value));
    memcpy(dstp, value, 4 * sizeof(*value));
    return 0;
}

int ngli_params_set_vec2(uint8_t *dstp, const struct node_param *par, const float *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_VEC2);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%g,%g)", par->key, NGLI_ARG_VEC2(value));
    memcpy(dstp, value, 2 * sizeof(*value));
    return 0;
}

int ngli_params_set_vec3(uint8_t *dstp, const struct node_param *par, const float *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_VEC3);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%g,%g,%g)", par->key, NGLI_ARG_VEC3(value));
    memcpy(dstp, value, 3 * sizeof(*value));
    return 0;
}

int ngli_params_set_vec4(uint8_t *dstp, const struct node_param *par, const float *value)
{
    int ret = check_param_type(par, NGLI_PARAM_TYPE_VEC4);
    if (ret < 0)
        return ret;

    if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)
        dstp += sizeof(struct ngl_node *);

    LOG(VERBOSE, "set %s to (%g,%g,%g,%g)", par->key, NGLI_ARG_VEC4(value));
    memcpy(dstp, value, 4 * sizeof(*value));
    return 0;
}

int ngli_params_set_defaults(uint8_t *base_ptr, const struct node_param *params)
{
    size_t last_offset = 0;

    if (!params)
        return 0;

    for (size_t i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        // The offset must be monotonically incrementing to make the reset of the
        // non-params much simpler in the node uninit.
        if (par->offset < last_offset) {
            LOG(ERROR, "offset inconsistency detected around %s", par->key);
            ngli_assert(0);
        }
        last_offset = par->offset;

        uint8_t *dstp = base_ptr + par->offset;
        int ret = 0;
        switch (par->type) {
            case NGLI_PARAM_TYPE_SELECT: {
                const int v = par->def_value.i32;
                const char *s = ngli_params_get_select_str(par->choices->consts, v);
                ngli_assert(s);
                ret = ngli_params_set_select(dstp, par, s);
                break;
            }
            case NGLI_PARAM_TYPE_FLAGS: {
                const int v = par->def_value.i32;
                char *s = ngli_params_get_flags_str(par->choices->consts, v);
                if (!s)
                    return NGL_ERROR_INVALID_ARG;
                ngli_assert(*s);
                ret = ngli_params_set_flags(dstp, par, s);
                ngli_free(s);
                break;
            }
            case NGLI_PARAM_TYPE_BOOL:
                ret = ngli_params_set_bool(dstp, par, par->def_value.i32);
                break;
            case NGLI_PARAM_TYPE_I32:
                ret = ngli_params_set_i32(dstp, par, par->def_value.i32);
                break;
            case NGLI_PARAM_TYPE_U32:
                ret = ngli_params_set_u32(dstp, par, par->def_value.u32);
                break;
            case NGLI_PARAM_TYPE_F32:
                ret = ngli_params_set_f32(dstp, par, par->def_value.f32);
                break;
            case NGLI_PARAM_TYPE_F64:
                ret = ngli_params_set_f64(dstp, par, par->def_value.f64);
                break;
            case NGLI_PARAM_TYPE_STR:
                ret = ngli_params_set_str(dstp, par, par->def_value.str);
                break;
            case NGLI_PARAM_TYPE_IVEC2:
                ret = ngli_params_set_ivec2(dstp, par, par->def_value.ivec);
                break;
            case NGLI_PARAM_TYPE_IVEC3:
                ret = ngli_params_set_ivec3(dstp, par, par->def_value.ivec);
                break;
            case NGLI_PARAM_TYPE_IVEC4:
                ret = ngli_params_set_ivec4(dstp, par, par->def_value.ivec);
                break;
            case NGLI_PARAM_TYPE_UVEC2:
                ret = ngli_params_set_uvec2(dstp, par, par->def_value.uvec);
                break;
            case NGLI_PARAM_TYPE_UVEC3:
                ret = ngli_params_set_uvec3(dstp, par, par->def_value.uvec);
                break;
            case NGLI_PARAM_TYPE_UVEC4:
                ret = ngli_params_set_uvec4(dstp, par, par->def_value.uvec);
                break;
            case NGLI_PARAM_TYPE_VEC2:
                ret = ngli_params_set_vec2(dstp, par, par->def_value.vec);
                break;
            case NGLI_PARAM_TYPE_VEC3:
                ret = ngli_params_set_vec3(dstp, par, par->def_value.vec);
                break;
            case NGLI_PARAM_TYPE_VEC4:
                ret = ngli_params_set_vec4(dstp, par, par->def_value.vec);
                break;
            case NGLI_PARAM_TYPE_MAT4:
                ret = ngli_params_set_mat4(dstp, par, par->def_value.mat);
                break;
            case NGLI_PARAM_TYPE_DATA:
                ret = ngli_params_set_data(dstp, par, 0, NULL);
                break;
            case NGLI_PARAM_TYPE_RATIONAL:
                ret = ngli_params_set_rational(dstp, par, par->def_value.r[0], par->def_value.r[1]);
                break;
            default:
                break;
        }
        if (ret < 0)
            return ret;
    }
    return 0;
}

int ngli_params_add_nodes(uint8_t *dstp, const struct node_param *par,
                          size_t nb_nodes, struct ngl_node **nodes)
{
    uint8_t *cur_elems_p = dstp;
    uint8_t *nb_cur_elems_p = dstp + sizeof(struct ngl_node **);
    struct ngl_node **cur_elems = *(struct ngl_node ***)cur_elems_p;
    const size_t nb_cur_elems = *(size_t *)nb_cur_elems_p;
    const size_t nb_new_elems = nb_cur_elems + nb_nodes;
    struct ngl_node **new_elems = ngli_realloc(cur_elems, nb_new_elems, sizeof(*new_elems));
    struct ngl_node **new_elems_addp = new_elems + nb_cur_elems;

    if (!new_elems)
        return NGL_ERROR_MEMORY;

    *(struct ngl_node ***)cur_elems_p = new_elems;
    for (size_t i = 0; i < nb_nodes; i++) {
        const struct ngl_node *e = nodes[i];
        if (!allowed_node(e, par->node_types)) {
            LOG(ERROR, "%s (%s) is not an allowed type for %s list",
                e->label, e->cls->name, par->key);
            return NGL_ERROR_INVALID_ARG;
        }
    }
    for (size_t i = 0; i < nb_nodes; i++) {
        struct ngl_node *e = nodes[i];
        new_elems_addp[i] = ngl_node_ref(e);
    }
    *(size_t *)nb_cur_elems_p = nb_new_elems;
    return 0;
}

int ngli_params_add_f64s(uint8_t *dstp, const struct node_param *par,
                         size_t nb_f64s, const double *f64s)
{
    uint8_t *cur_elems_p = dstp;
    uint8_t *nb_cur_elems_p = dstp + sizeof(double *);
    double *cur_elems = *(double **)cur_elems_p;
    const size_t nb_cur_elems = *(size_t *)nb_cur_elems_p;
    const size_t nb_new_elems = nb_cur_elems + nb_f64s;
    double *new_elems = ngli_realloc(cur_elems, nb_new_elems, sizeof(*new_elems));
    double *new_elems_addp = new_elems + nb_cur_elems;

    if (!new_elems)
        return NGL_ERROR_MEMORY;
    for (size_t i = 0; i < nb_f64s; i++)
        new_elems_addp[i] = f64s[i];
    *(double **)cur_elems_p = new_elems;
    *(size_t *)nb_cur_elems_p = nb_new_elems;
    return 0;
}

int ngli_params_add(uint8_t *base_ptr, const struct node_param *par,
                    size_t nb_elems, void *elems)
{
    LOG(VERBOSE, "add %zu elems to %s", nb_elems, par->key);

    int ret = 0;
    uint8_t *dstp = base_ptr + par->offset;
    switch (par->type) {
    case NGLI_PARAM_TYPE_NODELIST: ret = ngli_params_add_nodes(dstp, par, nb_elems, elems); break;
    case NGLI_PARAM_TYPE_F64LIST:  ret = ngli_params_add_f64s(dstp, par, nb_elems, elems);  break;
    default:
        LOG(ERROR, "parameter %s is not a list", par->key);
        return NGL_ERROR_INVALID_USAGE;
    }
    return ret;
}

void ngli_params_free(uint8_t *base_ptr, const struct node_param *params)
{
    if (!params)
        return;

    for (size_t i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];
        uint8_t *parp = base_ptr + par->offset;

        if (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE) {
            struct ngl_node *node = *(struct ngl_node **)parp;
            ngl_node_unrefp(&node);
            continue;
        }

        switch (par->type) {
            case NGLI_PARAM_TYPE_STR: {
                char *s = *(char **)parp;
                ngli_free(s);
                break;
            }
            case NGLI_PARAM_TYPE_DATA: {
                uint8_t *data = *(uint8_t **)parp;
                ngli_free(data);
                break;
            }
            case NGLI_PARAM_TYPE_NODE: {
                struct ngl_node *node = *(struct ngl_node **)parp;
                ngl_node_unrefp(&node);
                break;
            }
            case NGLI_PARAM_TYPE_NODELIST: {
                uint8_t *nb_elems_p = parp + sizeof(struct ngl_node **);
                struct ngl_node **elems = *(struct ngl_node ***)parp;
                const size_t nb_elems = *(size_t *)nb_elems_p;
                for (size_t j = 0; j < nb_elems; j++)
                    ngl_node_unrefp(&elems[j]);
                ngli_free(elems);
                break;
            }
            case NGLI_PARAM_TYPE_F64LIST: {
                double *elems = *(double **)parp;
                ngli_free(elems);
                break;
            }
            case NGLI_PARAM_TYPE_NODEDICT: {
                struct hmap **hmapp = (struct hmap **)parp;
                ngli_hmap_freep(hmapp);
                break;
            }
            default:
                break;
        }
    }
}
