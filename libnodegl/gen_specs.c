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

#include "nodegl.h"
#include "nodes.h"
#include "nodes_register.h"

#define CLASS_LIST(type_name, class) extern const struct node_class class;
NODE_MAP_TYPE2CLASS(CLASS_LIST)

extern const struct node_param ngli_base_node_params[];

static const char *param_type_strings[] = {
    [PARAM_TYPE_INT]      = "int",
    [PARAM_TYPE_I64]      = "i64",
    [PARAM_TYPE_DBL]      = "double",
    [PARAM_TYPE_STR]      = "string",
    [PARAM_TYPE_DATA]     = "data",
    [PARAM_TYPE_VEC2]     = "vec2",
    [PARAM_TYPE_VEC3]     = "vec3",
    [PARAM_TYPE_VEC4]     = "vec4",
    [PARAM_TYPE_MAT4]     = "mat4",
    [PARAM_TYPE_NODE]     = "Node",
    [PARAM_TYPE_NODELIST] = "NodeList",
    [PARAM_TYPE_DBLLIST]  = "doubleList",
    [PARAM_TYPE_NODEDICT] = "NodeDict",
};

static void print_param(const struct node_param *p)
{
    printf("        - [%s, %s]\n",
           p->key, param_type_strings[p->type]);
}

static void print_node_params(const char *name, const struct node_param *p)
{
    printf("- %s:\n", name);
    if (p) {
        if (p->key && (p->flags & PARAM_FLAG_CONSTRUCTOR)) {
            printf("    constructors:\n");
            while (p->key && (p->flags & PARAM_FLAG_CONSTRUCTOR)) {
                print_param(p);
                p++;
            }
        }
        if (p->key && !(p->flags & PARAM_FLAG_CONSTRUCTOR)) {
            printf("    optional:\n");
            while (p->key && !(p->flags & PARAM_FLAG_CONSTRUCTOR)) {
                print_param(p);
                p++;
            }
        }
    }
    printf("\n");
}

#define CLASS_COMMALIST(type_name, class) &class,

int main(void)
{
    printf("#\n# Nodes specifications for node.gl v%d.%d.%d\n#\n\n",
           NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    print_node_params("_Node", ngli_base_node_params);

    static const struct node_class *node_classes[] = {
        NODE_MAP_TYPE2CLASS(CLASS_COMMALIST)
    };

    for (int i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        if (c) {
            const struct node_param *p = &c->params[0];
            print_node_params(c->name, p);
        }
    }
    return 0;
}
