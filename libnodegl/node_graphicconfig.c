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

#include <stddef.h>
#include <string.h>

#include "glincludes.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct graphicconfig, x)
#define ADD_PARAM(name, NAME) \
    {#name, PARAM_TYPE_NODE, OFFSET(name), .node_types=(const int[]){NGL_NODE_CONFIG##NAME, -1}}
static const struct node_param graphicconfig_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR},
    ADD_PARAM(blend, BLEND),
    ADD_PARAM(colormask, COLORMASK),
    ADD_PARAM(depth, DEPTH),
    ADD_PARAM(polygonmode, POLYGONMODE),
    ADD_PARAM(stencil, STENCIL),
    {NULL}
};

static int graphicconfig_init(struct ngl_node *node)
{
    struct graphicconfig *s = node->priv_data;

    return ngli_node_init(s->child);
}

static int graphicconfig_update(struct ngl_node *node, double t)
{
    struct graphicconfig *s = node->priv_data;
    struct ngl_node *child = s->child;

    memcpy(child->modelview_matrix, node->modelview_matrix, sizeof(node->modelview_matrix));
    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    return ngli_node_update(child, t);
}

static void honor_config(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct graphicconfig *s = node->priv_data;

    if (s->blend) {
        struct configblend *c = s->blend->priv_data;
        ngli_glGetIntegerv(gl, c->capability, (GLint *)&c->enabled[1]);
        if (c->enabled[0]) {
            ngli_glGetIntegerv(gl, GL_BLEND_SRC_RGB, (GLint *)&c->src_rgb[1]);
            ngli_glGetIntegerv(gl, GL_BLEND_DST_RGB, (GLint *)&c->dst_rgb[1]);
            ngli_glGetIntegerv(gl, GL_BLEND_SRC_ALPHA, (GLint *)&c->src_alpha[1]);
            ngli_glGetIntegerv(gl, GL_BLEND_DST_ALPHA, (GLint *)&c->dst_alpha[1]);
            ngli_glGetIntegerv(gl, GL_BLEND_EQUATION_RGB, (GLint *)&c->mode_rgb[1]);
            ngli_glGetIntegerv(gl, GL_BLEND_EQUATION_ALPHA, (GLint *)&c->mode_alpha[1]);
            ngli_glEnable(gl, c->capability);
            ngli_glBlendFuncSeparate(gl, c->src_rgb[0], c->dst_rgb[0], c->src_alpha[0], c->dst_alpha[0]);
            ngli_glBlendEquationSeparate(gl, c->mode_rgb[0], c->mode_alpha[0]);
        } else {
            ngli_glDisable(gl, c->capability);
        }
    }

    if (s->colormask) {
        struct configcolormask *c = s->colormask->priv_data;
        GLboolean rgba[4];
        ngli_glGetBooleanv(gl, GL_COLOR_WRITEMASK, rgba);
        c->rgba[1][0] = rgba[0];
        c->rgba[1][1] = rgba[1];
        c->rgba[1][2] = rgba[2];
        c->rgba[1][3] = rgba[3];
        ngli_glColorMask(gl, c->rgba[0][0], c->rgba[0][1], c->rgba[0][2], c->rgba[0][3]);
    }

    if (s->depth) {
        struct configdepth *c = s->depth->priv_data;
        ngli_glGetIntegerv(gl, c->capability, (GLint *)&c->enabled[1]);
        if (c->enabled[0]) {
            ngli_glEnable(gl, c->capability);
        } else {
            ngli_glDisable(gl, c->capability);
        }
    }

    if (s->polygonmode) {
        struct configpolygonmode *c = s->polygonmode->priv_data;
        ngli_glGetIntegerv(gl, GL_POLYGON_MODE, (GLint *)&c->mode[1]);
        ngli_glPolygonMode(gl, GL_FRONT_AND_BACK, c->mode[0]);
    }

    if (s->stencil) {
        struct configstencil *c = s->stencil->priv_data;
        ngli_glGetIntegerv(gl, c->capability, (GLint *)&c->enabled[1]);
        if (c->enabled[0]) {
            ngli_glGetIntegerv(gl, GL_STENCIL_WRITEMASK, (GLint *)&c->writemask[1]);
            ngli_glGetIntegerv(gl, GL_STENCIL_FUNC, (GLint *)&c->func[1]);
            ngli_glGetIntegerv(gl, GL_STENCIL_REF, (GLint *)&c->func_ref[1]);
            ngli_glGetIntegerv(gl, GL_STENCIL_VALUE_MASK, (GLint *)&c->func_mask[1]);
            ngli_glGetIntegerv(gl, GL_STENCIL_FAIL, (GLint *)&c->op_sfail[1]);
            ngli_glGetIntegerv(gl, GL_STENCIL_PASS_DEPTH_FAIL, (GLint *)&c->op_dpfail[1]);
            ngli_glGetIntegerv(gl, GL_STENCIL_PASS_DEPTH_PASS, (GLint *)&c->op_dppass[1]);
            ngli_glEnable(gl, c->capability);
            ngli_glStencilMask(gl, c->writemask[0]);
            ngli_glStencilFunc(gl, c->func[0], c->func_ref[0], c->func_mask[0]);
            ngli_glStencilOp(gl, c->op_sfail[0], c->op_dpfail[0], c->op_dppass[0]);
        } else {
            ngli_glDisable(gl, c->capability);
        }
    }
}

static void restore_config(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct graphicconfig *s = node->priv_data;

    if (s->blend) {
        struct configblend *c = s->blend->priv_data;
        if (c->enabled[1]) {
            ngli_glEnable(gl, c->capability);
        } else {
            ngli_glDisable(gl, c->capability);
        }
        if (c->enabled[0]) {
            ngli_glBlendFuncSeparate(gl, c->src_rgb[1], c->dst_rgb[1], c->src_alpha[1], c->dst_alpha[1]);
            ngli_glBlendEquationSeparate(gl, c->mode_rgb[1], c->mode_alpha[1]);
        }
    }

    if (s->colormask) {
        struct configcolormask *c = s->colormask->priv_data;
        ngli_glColorMask(gl, c->rgba[1][0], c->rgba[1][1], c->rgba[1][2], c->rgba[1][3]);
    }

    if (s->depth) {
        struct configdepth *c = s->depth->priv_data;
        if (c->enabled[1]) {
            ngli_glEnable(gl, c->capability);
        } else {
            ngli_glDisable(gl, c->capability);
        }
    }

    if (s->polygonmode) {
        struct configpolygonmode *c = s->polygonmode->priv_data;
        ngli_glPolygonMode(gl, GL_FRONT_AND_BACK, c->mode[1]);
    }

    if (s->stencil) {
        struct configstencil *c = s->stencil->priv_data;
        if (c->enabled[1]) {
            ngli_glEnable(gl, c->capability);
        } else {
            ngli_glDisable(gl, c->capability);
        }
        if (c->enabled[0]) {
            ngli_glStencilMask(gl, c->writemask[1]);
            ngli_glStencilFunc(gl, c->func[1], c->func_ref[1], c->func_mask[1]);
            ngli_glStencilOp(gl, c->op_sfail[1], c->op_dpfail[1], c->op_dppass[1]);
        }
    }
}

static void graphicconfig_draw(struct ngl_node *node)
{
    struct graphicconfig *s = node->priv_data;

    honor_config(node);
    ngli_node_draw(s->child);
    restore_config(node);
}

const struct node_class ngli_graphicconfig_class = {
    .id        = NGL_NODE_GRAPHICCONFIG,
    .name      = "GraphicConfig",
    .init      = graphicconfig_init,
    .update    = graphicconfig_update,
    .draw      = graphicconfig_draw,
    .priv_size = sizeof(struct graphicconfig),
    .params    = graphicconfig_params,
};
