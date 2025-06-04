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

#include <string.h>

#include "internal.h"
#include "nodes_register.h"
#include "nopegl.h"
#include "params.h"
#include "utils/hmap.h"
#include "utils/string.h"
#include "utils/memory.h"

#define CLASS_LIST(type_name, cls) extern const struct node_class cls;
NODE_MAP_TYPE2CLASS(CLASS_LIST)

extern const struct node_param ngli_base_node_params[];
extern const struct param_specs ngli_params_specs[];

#define REGISTER_NODE(type_name, cls)           \
    case type_name: {                           \
        extern const struct node_class cls;     \
        ngli_assert(cls.id == type_name);       \
        return &cls;                            \
    }                                           \

static const struct node_class *get_node_class(int type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(REGISTER_NODE)
    }
    return NULL;
}

static char *escape_json_str(const char *s)
{
    if (!s)
        return NULL;
    char *p = malloc(strlen(s) * 2 + 1);
    if (!p)
        abort();
    char *ret = p;
    while (*s) {
        if (*s == '"')
            *p++ = '\\';
        *p++ = *s++;
    }
    *p = 0;
    return ret;
}

#define I "          " /* keys indent */
#define D I "\"default\": " /* default key */

static void print_node_params(const char *name, const struct node_param *p, const char *file)
{
    printf("    \"%s\": {\n", name);
    if (file)
        printf("      \"file\": \"%s\",\n", file);
    printf("      \"params\": [\n");
    if (p) {
        while (p->key) {
            printf("        {\n");
            printf(I "\"name\": \"%s\",\n", p->key);
            printf(I "\"type\": \"%s\",\n", ngli_params_specs[p->type].name);
            switch (p->type) {
            case NGLI_PARAM_TYPE_SELECT: {
                const int v = p->def_value.i32;
                const char *s = ngli_params_get_select_str(p->choices->consts, v);
                ngli_assert(s);
                printf(D "\"%s\",\n", s);
                printf(I "\"choices\": \"%s\",\n", p->choices->name);
                break;
            }
            case NGLI_PARAM_TYPE_FLAGS: {
                const int v = p->def_value.i32;
                char *s = ngli_params_get_flags_str(p->choices->consts, v);
                if (!s)
                    abort();
                ngli_assert(*s);
                printf(D "\"%s\",\n", s);
                ngli_free(s);
                printf(I "\"choices\": \"%s\",\n", p->choices->name);
                break;
            }
            case NGLI_PARAM_TYPE_F32:
                printf(D "%f,\n", p->def_value.f32);
                break;
            case NGLI_PARAM_TYPE_F64:
                printf(D "%f,\n", p->def_value.f64);
                break;
            case NGLI_PARAM_TYPE_BOOL:
                printf(D "%d,\n", p->def_value.i32);
                break;
            case NGLI_PARAM_TYPE_I32:
                printf(D "%d,\n", p->def_value.i32);
                break;
            case NGLI_PARAM_TYPE_U32:
                printf(D "%u,\n", p->def_value.u32);
                break;
            case NGLI_PARAM_TYPE_IVEC2: printf(D "[%d,%d],\n",       NGLI_ARG_VEC2(p->def_value.ivec)); break;
            case NGLI_PARAM_TYPE_IVEC3: printf(D "[%d,%d,%d],\n",    NGLI_ARG_VEC3(p->def_value.ivec)); break;
            case NGLI_PARAM_TYPE_IVEC4: printf(D "[%d,%d,%d,%d],\n", NGLI_ARG_VEC4(p->def_value.ivec)); break;
            case NGLI_PARAM_TYPE_UVEC2: printf(D "[%u,%u],\n",       NGLI_ARG_VEC2(p->def_value.uvec)); break;
            case NGLI_PARAM_TYPE_UVEC3: printf(D "[%u,%u,%u],\n",    NGLI_ARG_VEC3(p->def_value.uvec)); break;
            case NGLI_PARAM_TYPE_UVEC4: printf(D "[%u,%u,%u,%u],\n", NGLI_ARG_VEC4(p->def_value.uvec)); break;
            case NGLI_PARAM_TYPE_VEC2:  printf(D "[%f,%f],\n",       NGLI_ARG_VEC2(p->def_value.vec));  break;
            case NGLI_PARAM_TYPE_VEC3:  printf(D "[%f,%f,%f],\n",    NGLI_ARG_VEC3(p->def_value.vec));  break;
            case NGLI_PARAM_TYPE_VEC4:  printf(D "[%f,%f,%f,%f],\n", NGLI_ARG_VEC4(p->def_value.vec));  break;
            case NGLI_PARAM_TYPE_MAT4:
                printf(D "[%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f],\n", NGLI_ARG_MAT4(p->def_value.mat));
                break;
            case NGLI_PARAM_TYPE_STR:
                if (p->def_value.str)
                    printf(D "\"%s\",\n", p->def_value.str);
                break;
            case NGLI_PARAM_TYPE_RATIONAL:
                printf(D "[%d,%d],\n", NGLI_ARG_VEC2(p->def_value.r));
                break;
            default:
                break;
            }
            if (p->node_types) {
                printf(I "\"node_types\": [");
                for (size_t i = 0; p->node_types[i] != NGLI_NODE_NONE; i++) {
                    const struct node_class *cls = get_node_class(p->node_types[i]);
                    ngli_assert(cls);
                    printf("%s\"%s\"", i ? ", " : "", cls->name);
                }
                printf("],\n");
            }
            static const struct {
                const char *name;
                const int flag;
            } flag_names[] = {
                {"live", NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE},
                {"node", NGLI_PARAM_FLAG_ALLOW_NODE},
                {"nonull", NGLI_PARAM_FLAG_NON_NULL},
                {"filepath", NGLI_PARAM_FLAG_FILEPATH},
            };
            printf(I "\"flags\": [");
            int ihasflag = 0;
            for (size_t i = 0; i < NGLI_ARRAY_NB(flag_names); i++) {
                if (!(p->flags & flag_names[i].flag))
                    continue;
                printf("%s\"%s\"", ihasflag ? ", " : "", flag_names[i].name);
                ihasflag = 1;
            }
            printf("],\n");
            char *esc_desc = escape_json_str(p->desc);
            printf(I "\"desc\": \"%s\"\n", esc_desc ? esc_desc : "");
            free(esc_desc);
            printf("        }%s\n", (p + 1)->key ? "," : "");
            p++;
        }
    }
    printf("      ]\n");
    printf("    }");
}

#define CLASS_COMMALIST(type_name, cls) &cls,

static const struct node_class *node_classes[] = {
    NODE_MAP_TYPE2CLASS(CLASS_COMMALIST)
};

static void print_types(void)
{
    printf("  \"types\": [\n");
    for (size_t i = 0; i < NGLI_PARAM_TYPE_NB; i++) {
        const struct param_specs *ps = &ngli_params_specs[i];
        ngli_assert(ps->name && ps->desc);
        printf("    {\n");
        printf("      \"name\": \"%s\",\n", ps->name);
        printf("      \"size\": %zu,\n", ps->size);
        printf("      \"desc\": \"%s\"\n", ps->desc);
        printf("    }%s\n", i != NGLI_PARAM_TYPE_NB - 1 ? "," : "");
    }
    printf("  ]");
}

static void print_constants(const struct param_choices *choices)
{
    const struct param_const *consts = choices->consts;

    printf("    \"%s\": [\n", choices->name);
    for (size_t i = 0; consts[i].key; i++) {
        const struct param_const *pc = &consts[i];
        ngli_assert(pc->key && pc->desc);
        printf("      {\n");
        printf("        \"name\": \"%s\",\n", pc->key);
        printf("        \"desc\": \"%s\"\n", pc->desc);
        printf("      }%s\n", consts[i + 1].key ? "," : "");
    }
    printf("    ]");
}

static int print_choices(void)
{
    printf("  \"choices\": {\n");

    struct hmap *choices_map = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!choices_map)
        return -1;

    for (size_t i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];

        if (!c->params)
            continue;
        for (size_t j = 0; c->params[j].key; j++) {
            const struct node_param *p = &c->params[j];
            if (!p->choices)
                continue;
            void *mapped_choices = ngli_hmap_get_str(choices_map, p->choices->name);
            if (mapped_choices) {
                ngli_assert(mapped_choices == p->choices);
            } else {
                if (ngli_hmap_count(choices_map))
                    printf(",\n");
                print_constants(p->choices);
                ngli_hmap_set_str(choices_map, p->choices->name, (void *)p->choices);

            }
        }
    }

    printf("\n  }");

    ngli_hmap_freep(&choices_map);
    return 0;
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
    if (!par)
        return 0;
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
        default:
            return NULL;
    }
}

static int check_node_params(const struct node_class *cls)
{
    const struct node_param *par = cls->params;
    if (!par)
        return 0;

    if (cls->params && !cls->opts_size) {
        fprintf(stderr, "node type %s has parameters but no opts_size is specified\n", cls->name);
        return NGL_ERROR_BUG;
    }

    if (cls->category == NGLI_NODE_CATEGORY_TRANSFORM) {
        if (strcmp(par->key, "child") ||
            !(par->flags & NGLI_PARAM_FLAG_NON_NULL)) {
            fprintf(stderr, "transform nodes are expected to have a non-null child as first parameter");
            return NGL_ERROR_BUG;
        }
    }

    while (par->key) {
        if ((par->flags & NGLI_PARAM_FLAG_NON_NULL) && !pointer_based_parameter(par)) {
            fprintf(stderr, "parameter %s.%s has a non-applicable non-null flag\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        const uint32_t invalid_flags = NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_ALLOW_NODE;
        if (NGLI_HAS_ALL_FLAGS(par->flags, invalid_flags)) {
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

            for (size_t i = 0; par->node_types[i] != NGLI_NODE_NONE; i++) {
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
                    "so it can not be live changed\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        if ((par->flags & NGLI_PARAM_FLAG_ALLOW_NODE) && pointer_based_parameter(par)) {
            fprintf(stderr, "%s.%s is already a pointer-based parameter, "
                    "so the allow node flag should not be present\n", cls->name, par->key);
            return NGL_ERROR_BUG;
        }

        if (par->flags & NGLI_PARAM_FLAG_FILEPATH) {
            if (par->type != NGLI_PARAM_TYPE_STR) {
                fprintf(stderr, "filepath parameter %s.%s must be a string\n", cls->name, par->key);
                return NGL_ERROR_BUG;
            }
        }

        par++;
    }

    return 0;
}

static int check_common_node_params(const struct node_param *par)
{
    while (par->key) {
        if (par->type == NGLI_PARAM_TYPE_NODE ||
            par->type == NGLI_PARAM_TYPE_NODEDICT ||
            par->type == NGLI_PARAM_TYPE_NODELIST ||
            (par->flags & NGLI_PARAM_FLAG_ALLOW_NODE)) {
            fprintf(stderr, "the common node parameters must not include any node-based parameter\n");
            return NGL_ERROR_BUG;
        }
        par++;
    }
    return 0;
}

static int print_nodes(void)
{
    printf("  \"nodes\": {\n");
    print_node_params("_Node", ngli_base_node_params, NULL);

    struct hmap *params_map = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!params_map)
        return -1;

    int ret = check_common_node_params(ngli_base_node_params);
    if (ret < 0)
        goto end;

    for (size_t i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        const struct node_param *p = &c->params[0];

        ret = check_node_params(c);
        if (ret < 0)
            goto end;

        if (c->params_id) {
            void *mapped_param = ngli_hmap_get_str(params_map, c->params_id);
            char *pname = ngli_asprintf("_%s", c->params_id);
            if (!pname) {
                ngli_free(pname);
                ret = -1;
                goto end;
            }
            if (mapped_param) {
                ngli_assert(mapped_param == p);
            } else {
                printf(",\n");
                print_node_params(pname, p, c->file);
                ngli_hmap_set_str(params_map, c->params_id, (void *)p);
            }
            printf(",\n");
            printf("    \"%s\": \"%s\"", c->name, pname);
            ngli_free(pname);
        } else {
            printf(",\n");
            print_node_params(c->name, p, c->file);
        }
    }
    printf("\n  }");

end:
    ngli_hmap_freep(&params_map);
    return ret;
}

int main(void)
{
    printf("{\n");

    print_types();
    printf(",\n");

    print_choices();
    printf(",\n");

    int ret = print_nodes();
    if (ret < 0)
        return 1;
    printf("\n");

    printf("}\n");
    return 0;
}
