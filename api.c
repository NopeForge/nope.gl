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

#include <stdlib.h>
#include <stdio.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

struct ngl_ctx {
    struct glcontext *glcontext;
    struct ngl_node *scene;
};

struct ngl_ctx *ngl_create(void)
{
    struct ngl_ctx *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    LOG(INFO, "Context create in node.gl v%d.%d.%d",
        NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    return s;
}

int ngl_set_glcontext(struct ngl_ctx *s, void *display, void *window, void *handle, int platform, int api)
{
    s->glcontext = ngli_glcontext_new_wrapped(display, window, handle, platform, api);
    if (!s->glcontext)
        return -1;

    ngli_glcontext_load_extensions(s->glcontext);

    return 0;
}

int ngl_set_viewport(struct ngl_ctx *s, int x, int y, int w, int h)
{
    LOG(DEBUG, "update viewport to %d,%d %dx%d", x, y, w, h);
    glViewport(x, y, w, h);
    return 0;
}

int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene)
{
    ngl_node_ref(scene);
    ngl_node_unrefp(&s->scene);
    s->scene = scene;
    return 0;
}

int ngl_draw(struct ngl_ctx *s, double t)
{
    struct ngl_node *scene = s->scene;

    if (!scene) {
        LOG(ERROR, "scene is not set, can not draw");
        return -1;
    }

    LOG(DEBUG, "draw scene %s @ t=%f", scene->name, t);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ngli_node_check_resources(scene, t);
    ngli_node_update(scene, t);
    ngli_node_draw(scene);

    return 0;
}

void ngl_free(struct ngl_ctx **ss)
{
    struct ngl_ctx *s = *ss;

    if (!s)
        return;

    ngli_glcontext_freep(&s->glcontext);
    free(*ss);
    *ss = NULL;
}
