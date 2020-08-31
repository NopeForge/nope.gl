/*
 * Copyright 2019 GoPro Inc.
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

#include "gctx.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "rendertarget.h"

extern const struct gctx_class ngli_gctx_gl;
extern const struct gctx_class ngli_gctx_gles;

static const struct gctx_class *backend_map[] = {
#ifdef BACKEND_GL
    [NGL_BACKEND_OPENGL]   = &ngli_gctx_gl,
    [NGL_BACKEND_OPENGLES] = &ngli_gctx_gles,
#endif
};

struct gctx *ngli_gctx_create(const struct ngl_config *config)
{
    if (config->backend < 0 ||
        config->backend >= NGLI_ARRAY_NB(backend_map) ||
        !backend_map[config->backend]) {
        LOG(ERROR, "unknown backend %d", config->backend);
        return NULL;
    }
    const struct gctx_class *class = backend_map[config->backend];
    struct gctx *s = class->create(config);
    if (!s)
        return NULL;
    s->config = *config;
    s->class = class;
    return s;
}

int ngli_gctx_init(struct gctx *s)
{
    return s->class->init(s);
}

int ngli_gctx_resize(struct gctx *s, int width, int height, const int *viewport)
{
    const struct gctx_class *class = s->class;
    return class->resize(s, width, height, viewport);
}

int ngli_gctx_draw(struct gctx *s, struct ngl_node *scene, double t)
{
    const struct gctx_class *class = s->class;

    int ret = class->pre_draw(s, t);
    if (ret < 0)
        goto end;

    if (scene) {
        LOG(DEBUG, "draw scene %s @ t=%f", scene->label, t);
        ngli_node_draw(scene);
    }

end:;
    int end_ret = class->post_draw(s, t);
    if (end_ret < 0)
        return end_ret;

    return ret;
}

void ngli_gctx_freep(struct gctx **sp)
{
    if (!*sp)
        return;

    struct gctx *s = *sp;
    const struct gctx_class *class = s->class;
    if (class)
        class->destroy(s);

    ngli_freep(sp);
}

void ngli_gctx_transform_projection_matrix(struct gctx *s, float *dst)
{
    s->class->transform_projection_matrix(s, dst);
}

void ngli_gctx_get_rendertarget_uvcoord_matrix(struct gctx *s, float *dst)
{
    s->class->get_rendertarget_uvcoord_matrix(s, dst);
}

void ngli_gctx_set_rendertarget(struct gctx *s, struct rendertarget *rt)
{
    s->class->set_rendertarget(s, rt);
}

struct rendertarget *ngli_gctx_get_rendertarget(struct gctx *s)
{
    return s->class->get_rendertarget(s);
}

const struct rendertarget_desc *ngli_gctx_get_default_rendertarget_desc(struct gctx *s)
{
    return s->class->get_default_rendertarget_desc(s);
}

void ngli_gctx_set_viewport(struct gctx *s, const int *viewport)
{
    s->class->set_viewport(s, viewport);
}

void ngli_gctx_get_viewport(struct gctx *s, int *viewport)
{
    s->class->get_viewport(s, viewport);
}

void ngli_gctx_set_scissor(struct gctx *s, const int *scissor)
{
    s->class->set_scissor(s, scissor);
}

void ngli_gctx_get_scissor(struct gctx *s, int *scissor)
{
    s->class->get_scissor(s, scissor);
}

void ngli_gctx_set_clear_color(struct gctx *s, const float *color)
{
    s->class->set_clear_color(s, color);
}

void ngli_gctx_get_clear_color(struct gctx *s, float *color)
{
    s->class->get_clear_color(s, color);
}

void ngli_gctx_clear_color(struct gctx *s)
{
    s->class->clear_color(s);
}

void ngli_gctx_clear_depth_stencil(struct gctx *s)
{
    s->class->clear_depth_stencil(s);
}

void ngli_gctx_invalidate_depth_stencil(struct gctx *s)
{
    s->class->invalidate_depth_stencil(s);
}

int ngli_gctx_get_preferred_depth_format(struct gctx *s)
{
    return s->class->get_preferred_depth_format(s);
}

int ngli_gctx_get_preferred_depth_stencil_format(struct gctx *s)
{
    return s->class->get_preferred_depth_stencil_format(s);
}
