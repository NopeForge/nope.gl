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

#include "hmap.h"
#include "memory.h"
#include "nopegl.h"
#include "internal.h"
#include "nodes_register.h"
#include "params.h"

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

#define I "      " /* keys indent */
#define D I "\"default\": " /* default key */

static void print_node_params(const char *name, const struct node_param *p)
{
    printf("  \"%s\": [\n", name);
    if (p) {
        while (p->key) {
            printf("    {\n");
            printf(I "\"name\": \"%s\",\n", p->key);
            printf(I "\"type\": \"%s\",\n", ngli_params_specs[p->type].name);
            switch (p->type) {
            case NGLI_PARAM_TYPE_SELECT: {
                const int v = p->def_value.i32;
                const char *s = ngli_params_get_select_str(p->choices->consts, v);
                ngli_assert(s);
                printf(D "\"%s\",\n", s);
                printf(I "\"choices\": [");
                for (size_t i = 0; p->choices->consts[i].key; i++)
                    printf("%s\"%s\"", i ? ", " : "", p->choices->consts[i].key);
                printf("],\n");
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
                printf(I "\"choices\": [");
                for (size_t i = 0; p->choices->consts[i].key; i++)
                    printf("%s\"%s\"", i ? ", " : "", p->choices->consts[i].key);
                printf("],\n");
                break;
            }
            case NGLI_PARAM_TYPE_F32:
                printf(D "%f,\n", p->def_value.f32);
                break;
            case NGLI_PARAM_TYPE_F64:
                printf(D "%f,\n", p->def_value.f64);
                break;
            case NGLI_PARAM_TYPE_BOOL:
                if (p->def_value.i32 < 0)
                    printf(D "\"unset\",\n");
                else
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
            printf("    }%s\n", (p + 1)->key ? "," : "");
            p++;
        }
    }
    printf("  ]");
}

#define CLASS_COMMALIST(type_name, cls) &cls,

int main(void)
{
    printf("{\n");
    print_node_params("_Node", ngli_base_node_params);

    static const struct node_class *node_classes[] = {
        NODE_MAP_TYPE2CLASS(CLASS_COMMALIST)
    };

    struct hmap *params_map = ngli_hmap_create();
    if (!params_map)
        return -1;

    for (size_t i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        const struct node_param *p = &c->params[0];

        if (c->params_id) {
            void *mapped_param = ngli_hmap_get(params_map, c->params_id);
            char *pname = ngli_asprintf("_%s", c->params_id);
            if (!pname) {
                ngli_free(pname);
                return -1;
            }
            if (mapped_param) {
                ngli_assert(mapped_param == p);
            } else {
                printf(",\n");
                print_node_params(pname, p);
                ngli_hmap_set(params_map, c->params_id, (void *)p);
            }
            printf(",\n");
            printf("  \"%s\": \"%s\"", c->name, pname);
            ngli_free(pname);
        } else {
            printf(",\n");
            print_node_params(c->name, p);
        }
    }

    ngli_hmap_freep(&params_map);
    printf("\n}\n");
    return 0;
}
