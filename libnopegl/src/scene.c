/*
 * Copyright 2023 Nope Project
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

#include "nopegl.h"
#include "internal.h"
#include "memory.h"

struct ngl_scene *ngl_scene_create(void)
{
    struct ngl_scene *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->duration = 30.0;
    s->aspect_ratio[0] = 1;
    s->aspect_ratio[1] = 1;
    s->framerate[0] = 60;
    s->framerate[1] = 1;
    return s;
}

int ngl_scene_init_from_node(struct ngl_scene *s, struct ngl_node *node)
{
    ngl_node_unrefp(&s->root);
    s->root = ngl_node_ref(node);
    return 0;
}

int ngl_scene_init_from_str(struct ngl_scene *s, const char *str)
{
    ngl_node_unrefp(&s->root);
    return ngli_scene_deserialize(s, str);
}

char *ngl_scene_serialize(const struct ngl_scene *s)
{
    return ngli_scene_serialize(s);
}

char *ngl_scene_dot(const struct ngl_scene *s)
{
    return ngli_scene_dot(s);
}

void ngl_scene_freep(struct ngl_scene **sp)
{
    struct ngl_scene *s = *sp;
    if (!s)
        return;
    ngl_node_unrefp(&s->root);
    ngli_freep(sp);
}
