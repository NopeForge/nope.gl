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

#include "config.h"
#include "hmap.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"
#include "nodes_register.h"
#include "utils.h"

#if CONFIG_SMALL
#error gen doc can not work with CONFIG_SMALL set
#endif

#define CLASS_LIST(type_name, cls) extern const struct node_class cls;
NODE_MAP_TYPE2CLASS(CLASS_LIST)

extern const struct node_param ngli_base_node_params[];
extern const struct param_specs ngli_params_specs[];

#define TYPE2CLASS(type_name, cls)              \
    case type_name: {                           \
        extern const struct node_class cls;     \
        return &cls;                            \
    }                                           \

static const struct node_class *get_node_class(int type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(TYPE2CLASS)
    }
    return NULL;
}

#define LOWER(c) (((c) >= 'A' && (c) <= 'Z') ? (c) ^ 0x20 : (c))

static void print_node_type(struct bstr *b, const struct node_class *cls)
{
    const char *class_ref = cls->params_id ? cls->params_id : cls->name;

    ngli_bstr_printf(b, "[%s](#", cls->name);
    for (int i = 0; class_ref[i]; i++)
        ngli_bstr_printf(b, "%c", LOWER(class_ref[i]));
    ngli_bstr_print(b, ")");
}

static char *get_type_str(const struct node_param *p)
{
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    if (p->choices) {
        ngli_bstr_printf(b, "[`%s`](#%s-choices)", p->choices->name, p->choices->name);
    } else {
        ngli_bstr_printf(b, "[`%s`](#parameter-types)", ngli_params_specs[p->type].name);
    }
    if (p->node_types) {
        ngli_bstr_print(b, " (");
        for (int i = 0; p->node_types[i] != -1; i++) {
            if (i)
                ngli_bstr_print(b, ", ");
            print_node_type(b, get_node_class(p->node_types[i]));
        }
        ngli_bstr_print(b, ")");
    }

    char *type = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return type;
}

static char *get_default_str(const struct node_param *p)
{
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    switch (p->type) {
        case NGLI_PARAM_TYPE_SELECT: {
            const int v = (int)p->def_value.i64;
            const char *s = ngli_params_get_select_str(p->choices->consts, v);
            ngli_assert(s);
            ngli_bstr_printf(b, "`%s`", s);
            break;
        }
        case NGLI_PARAM_TYPE_FLAGS: {
            const int v = (int)p->def_value.i64;
            char *s = ngli_params_get_flags_str(p->choices->consts, v);
            if (!s)
                return NULL;
            ngli_assert(*s);
            ngli_bstr_printf(b, "`%s`", s);
            ngli_free(s);
            break;
        }
        case NGLI_PARAM_TYPE_F32:
            ngli_bstr_printf(b, "`%g`", p->def_value.f32);
            break;
        case NGLI_PARAM_TYPE_F64:
            ngli_bstr_printf(b, "`%g`", p->def_value.dbl);
            break;
        case NGLI_PARAM_TYPE_BOOL:
            if (p->def_value.i64 < 0)
                ngli_bstr_print(b, "`unset`");
            else
                ngli_bstr_printf(b, "`%d`", (int)p->def_value.i64);
            break;
        case NGLI_PARAM_TYPE_I32:
            ngli_bstr_printf(b, "`%d`", (int)p->def_value.i64);
            break;
        case NGLI_PARAM_TYPE_U32:
            ngli_bstr_printf(b, "`%u`", (unsigned)p->def_value.i64);
            break;
        case NGLI_PARAM_TYPE_IVEC2: ngli_bstr_printf(b, "(`%d`,`%d`)",           NGLI_ARG_VEC2(p->def_value.ivec)); break;
        case NGLI_PARAM_TYPE_IVEC3: ngli_bstr_printf(b, "(`%d`,`%d`,`%d`)",      NGLI_ARG_VEC3(p->def_value.ivec)); break;
        case NGLI_PARAM_TYPE_IVEC4: ngli_bstr_printf(b, "(`%d`,`%d`,`%d`,`%d`)", NGLI_ARG_VEC4(p->def_value.ivec)); break;
        case NGLI_PARAM_TYPE_UVEC2: ngli_bstr_printf(b, "(`%u`,`%u`)",           NGLI_ARG_VEC2(p->def_value.uvec)); break;
        case NGLI_PARAM_TYPE_UVEC3: ngli_bstr_printf(b, "(`%u`,`%u`,`%u`)",      NGLI_ARG_VEC3(p->def_value.uvec)); break;
        case NGLI_PARAM_TYPE_UVEC4: ngli_bstr_printf(b, "(`%u`,`%u`,`%u`,`%u`)", NGLI_ARG_VEC4(p->def_value.uvec)); break;
        case NGLI_PARAM_TYPE_VEC2:  ngli_bstr_printf(b, "(`%g`,`%g`)",           NGLI_ARG_VEC2(p->def_value.vec));  break;
        case NGLI_PARAM_TYPE_VEC3:  ngli_bstr_printf(b, "(`%g`,`%g`,`%g`)",      NGLI_ARG_VEC3(p->def_value.vec));  break;
        case NGLI_PARAM_TYPE_VEC4:  ngli_bstr_printf(b, "(`%g`,`%g`,`%g`,`%g`)", NGLI_ARG_VEC4(p->def_value.vec));  break;
    }

    char *def = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return def;
}

static int node_based_parameter(const struct node_param *par)
{
    return par->type == NGLI_PARAM_TYPE_NODE ||
           par->type == NGLI_PARAM_TYPE_NODELIST ||
           par->type == NGLI_PARAM_TYPE_NODEDICT;
}

static int pointer_based_parameter(const struct node_param *par)
{
    return node_based_parameter(par) ||
           par->type == NGLI_PARAM_TYPE_DATA ||
           par->type == NGLI_PARAM_TYPE_STR;
}

static int node_has_children(const struct node_class *cls)
{
    const struct node_param *par = cls->params;
    while (par->key) {
        if (node_based_parameter(par))
            return 1;
        par++;
    }
    return 0;
}

#define MAP_NODE_CLS(type_name, cls) case type_name: return &cls;

static const struct node_class *node_type_to_class(int type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(MAP_NODE_CLS)
    }
    return NULL;
}

static int check_node_params(const struct node_class *cls)
{
    const struct node_param *par = cls->params;
    if (!par)
        return 0;

    while (par->key) {
        if ((par->flags & NGLI_PARAM_FLAG_NON_NULL) && !pointer_based_parameter(par)) {
            fprintf(stderr, "parameter %s.%s has a non-applicable non-null flag\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        const uint32_t invalid_flags = NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_ALLOW_NODE;
        if ((par->flags & invalid_flags) == invalid_flags) {
            fprintf(stderr, "parameter %s.%s can not be non-null and "
                    "allow a node at the same time\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        if (par->flags & NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED) {
            if (par->type != NGLI_PARAM_TYPE_NODELIST && par->type != NGLI_PARAM_TYPE_NODEDICT) {
                fprintf(stderr, "parameter %s.%s is not a node list nor a node dict, "
                        "so packed display in dot is not supported\n", cls->name, par->key);
                return NGL_ERROR_UNSUPPORTED;
            }

            for (int i = 0; par->node_types[i] != -1; i++) {
                const struct node_class *child_cls = node_type_to_class(par->node_types[i]);
                if (node_has_children(child_cls)) {
                    fprintf(stderr, "parameter %s.%s could be a node that has children nodes, "
                            "so packed display in dot should not be set\n", cls->name, par->key);
                    return NGL_ERROR_BUG;
                }
            }
        }

        if ((par->flags & NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME) && (par->type != NGLI_PARAM_TYPE_NODE)) {
            fprintf(stderr, "parameter %s.%s is not a node parameter, "
                    "so the DOT_DISPLAY_FIELDNAME is not needed\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        if ((par->flags & NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE) && node_based_parameter(par)) {
            fprintf(stderr, "%s.%s is a node based parameter, "
                    "so it can not be live changed", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        if ((par->flags & NGLI_PARAM_FLAG_ALLOW_NODE) && pointer_based_parameter(par)) {
            fprintf(stderr, "%s.%s is already a pointer-based parameter, "
                    "so the allow node flag should not be present\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        par++;
    }

    return 0;
}

static void print_node_params(const char *name, const struct node_param *p)
{
    printf("\n## %s\n\n", name);
    if (!p)
        return;
    printf("Parameter | Flags | Type | Description | Default\n");
    printf("--------- | ----- | ---- | ----------- | :-----:\n");
    while (p->key) {
        char *type = get_type_str(p);
        char *def = get_default_str(p);

        if (type && def) {
            ngli_assert(p->desc);
            printf("`%s` | %s%s%s | %s | %s | %s\n",
                   p->key,
                   (p->flags & NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE) ? " [`live`](#Parameter-flags)" : "",
                   (p->flags & NGLI_PARAM_FLAG_ALLOW_NODE)        ? " [`node`](#Parameter-flags)" : "",
                   (p->flags & NGLI_PARAM_FLAG_NON_NULL)          ? " [`nonull`](#Parameter-flags)" : "",
                   type, p->desc, def);
        }

        ngli_free(def);
        ngli_free(type);
        p++;
    }
    printf("\n\n");
}

static void print_choices(const struct param_choices *choices)
{
    printf("\n## %s choices\n\n", choices->name);
    const struct param_const *consts = choices->consts;
    printf("Constant | Description\n");
    printf("-------- | -----------\n");
    for (int i = 0; consts[i].key; i++) {
        const struct param_const *c = &consts[i];
        ngli_assert(c->desc);
        printf("`%s` | %s\n", c->key, c->desc);
    }
}

static void print_parameter_flags(void)
{
    printf(
        "Parameter flags\n"
        "===============\n"
        "\n"
        "Marker   | Meaning\n"
        "-------- | -------\n"
        "`live`   | value is live-changeable between draw calls\n"
        "`node`   | nodes with the same data size are also allowed"
        " (e.g a `vec3` parameter can accept `AnimatedVec3`, `EvalVec3`, `NoiseVec3`, …)\n"
        "`nonull` | parameter must be set\n"
        "\n"
    );
}

#define CLASS_COMMALIST(type_name, cls) &cls,

static void print_source(const char *cfile)
{
    ngli_assert(cfile);
    printf("**Source**: [%s](/libnodegl/%s)\n\n", cfile, cfile);
}

int main(void)
{
    printf("libnodegl\n");
    printf("=========\n");

    static const struct node_class *node_classes[] = {
        NODE_MAP_TYPE2CLASS(CLASS_COMMALIST)
    };

    struct hmap *params_map = ngli_hmap_create();
    if (!params_map)
        return -1;

    for (int i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        const struct node_param *p = &c->params[0];

        int ret = check_node_params(c);
        if (ret < 0)
            return EXIT_FAILURE;

        if (c->params_id) {
            void *mapped_param = ngli_hmap_get(params_map, c->params_id);
            char *pname = ngli_asprintf("%s*", c->params_id);
            if (!pname) {
                ngli_hmap_freep(&params_map);
                return -1;
            }
            if (mapped_param) {
                ngli_assert(mapped_param == p);
            } else {
                print_node_params(pname, p);
                ngli_hmap_set(params_map, c->params_id, (void *)p);
                print_source(c->file);
                printf("List of `%s*` nodes:\n\n", c->params_id);
            }
            printf("- `%s`\n", c->name);
            ngli_free(pname);
        } else {
            print_node_params(c->name, p);
            print_source(c->file);
        }
    }

    ngli_hmap_freep(&params_map);

    printf("Parameter types\n");
    printf("===============\n");
    printf("\n");
    printf("Type | Description\n");
    printf("---- | -----------\n");
    for (int i = 0; i < NGLI_PARAM_TYPE_NB; i++) {
        const struct param_specs *ps = &ngli_params_specs[i];
        ngli_assert(ps->name && ps->desc);
        printf("`%s` | %s\n", ps->name, ps->desc);
    }
    printf("\n");

    print_parameter_flags();

    printf("Constants for choices parameters\n");
    printf("================================\n");

    struct hmap *choices_map = ngli_hmap_create();
    if (!choices_map)
        return -1;

    for (int i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        const struct node_param *p = c->params;

        if (p) {
            while (p->key) {
                if (p->choices) {
                    void *mapped_choices = ngli_hmap_get(choices_map, p->choices->name);
                    if (!mapped_choices) {
                        print_choices(p->choices);
                        ngli_hmap_set(choices_map, p->choices->name, (void *)p->choices);
                    }
                }
                p++;
            }
        }
    }

    ngli_hmap_freep(&choices_map);
    return 0;
}
