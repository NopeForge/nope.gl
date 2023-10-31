/*
 * Copyright 2023 Nope Forge
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
#include "log.h"
#include "memory.h"
#include "utils.h"

static void detach_root(struct ngl_scene *s)
{
    ngl_node_unrefp(&s->params.root);
}

static int attach_root(struct ngl_scene *s, struct ngl_node *node)
{
    s->params.root = ngl_node_ref(node);
    return 0;
}

struct ngl_scene *ngl_scene_create(void)
{
    struct ngl_scene *s = ngli_calloc(1, sizeof(*s));
    return s;
}

struct ngl_scene_params ngl_scene_default_params(struct ngl_node *root)
{
    const struct ngl_scene_params params = {
        .root         = root,
        .duration     = 30.0,
        .framerate    = {60, 1},
        .aspect_ratio = {1, 1},
    };
    return params;
}

int ngl_scene_init(struct ngl_scene *s, const struct ngl_scene_params *params)
{
    if (!params->root) {
        LOG(ERROR, "cannot initialize a scene without root node");
        return NGL_ERROR_INVALID_ARG;
    }
    if (params->duration < 0.0) {
        LOG(ERROR, "invalid scene duration %g", params->duration);
        return NGL_ERROR_INVALID_ARG;
    }
    if (params->framerate[0] <= 0 || params->framerate[1] <= 0) {
        LOG(ERROR, "invalid framerate %d/%d", NGLI_ARG_VEC2(params->framerate));
        return NGL_ERROR_INVALID_ARG;
    }
    if (params->aspect_ratio[0] <= 0 || params->aspect_ratio[1] <= 0) {
        LOG(ERROR, "invalid aspect ratio %d:%d", NGLI_ARG_VEC2(params->aspect_ratio));
        return NGL_ERROR_INVALID_ARG;
    }

    detach_root(s);
    s->params = *params;
    return attach_root(s, s->params.root);
}

int ngl_scene_init_from_str(struct ngl_scene *s, const char *str)
{
    detach_root(s);
    return ngli_scene_deserialize(s, str);
}

const struct ngl_scene_params *ngl_scene_get_params(const struct ngl_scene *s)
{
    return &s->params;
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
    detach_root(s);
    ngli_freep(sp);
}
