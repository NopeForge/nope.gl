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

#include <stddef.h>
#include <stdio.h>

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct rtt, x)
static const struct node_param rtt_params[] = {
    {"child",   PARAM_TYPE_NODE, OFFSET(child),   .flags=PARAM_FLAG_CONSTRUCTOR},
    {"color_texture", PARAM_TYPE_NODE, OFFSET(color_texture), .flags=PARAM_FLAG_CONSTRUCTOR,
                      .node_types=(const int[]){NGL_NODE_TEXTURE, -1}},
    {"depth_texture", PARAM_TYPE_NODE, OFFSET(depth_texture), .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const int[]){NGL_NODE_TEXTURE, -1}},
    {NULL}
};

static int rtt_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct rtt *s = node->priv_data;
    struct texture *texture = s->color_texture->priv_data;
    struct texture *depth_texture = NULL;

    int ret = ngli_node_init(s->color_texture);
    if (ret < 0)
        return ret;
    s->width = texture->width;
    s->height = texture->height;

    if (s->depth_texture) {
        depth_texture = s->depth_texture->priv_data;
        ret = ngli_node_init(s->depth_texture);
        if (ret < 0)
            return ret;
        ngli_assert(s->width == depth_texture->width && s->height == depth_texture->height);
    }

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

    ngli_glGenFramebuffers(gl, 1, &s->framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);

    LOG(VERBOSE, "init rtt with texture %d", texture->id);
    ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);

    if (depth_texture) {
        ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture->id, 0);
    } else {
        ngli_glGenRenderbuffers(gl, 1, &s->renderbuffer_id);
        ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, s->renderbuffer_id);
        ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, s->width, s->height);
        ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
        ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s->renderbuffer_id);
    }

    ngli_assert(ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);

    /* flip vertically the color and depth textures so the coordinates match
     * how the uv coordinates system works */
    texture->coordinates_matrix[5] = -1.0f;
    texture->coordinates_matrix[13] = 1.0f;

    if (s->depth_texture) {
        struct texture *depth_texture = s->depth_texture->priv_data;
        depth_texture->coordinates_matrix[5] = -1.0f;
        depth_texture->coordinates_matrix[13] = 1.0f;
    }

    ret = ngli_node_init(s->child);
    if (ret < 0)
        return ret;

    return 0;
}

static void rtt_update(struct ngl_node *node, double t)
{
    struct rtt *s = node->priv_data;
    ngli_node_update(s->child, t);
    ngli_node_update(s->color_texture, t);
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    GLint viewport[4];
    struct rtt *s = node->priv_data;

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);

    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, s->width, s->height);
    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ngli_node_draw(s->child);

    ngli_assert(ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);

    struct texture *texture = s->color_texture->priv_data;
    switch(texture->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glBindTexture(gl, GL_TEXTURE_2D, texture->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        break;
    }

    texture->coordinates_matrix[5] = -1.0f;
    texture->coordinates_matrix[13] = 1.0f;

    if (s->depth_texture) {
        struct texture *depth_texture = s->depth_texture->priv_data;
        depth_texture->coordinates_matrix[5] = -1.0f;
        depth_texture->coordinates_matrix[13] = 1.0f;
    }

}

static void rtt_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct rtt *s = node->priv_data;

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);
    ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);

    ngli_glDeleteRenderbuffers(gl, 1, &s->renderbuffer_id);
    ngli_glDeleteFramebuffers(gl, 1, &s->framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
}

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RTT,
    .name      = "RTT",
    .init      = rtt_init,
    .update    = rtt_update,
    .draw      = rtt_draw,
    .uninit    = rtt_uninit,
    .priv_size = sizeof(struct rtt),
    .params    = rtt_params,
};
