/*
 * Copyright 2018 GoPro Inc.
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
#include <string.h>
#include <sxplayer.h>

#include <libavcodec/mediacodec.h>

#include "android_surface.h"
#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "program.h"

struct hwupload_mc {
    GLuint framebuffer_id;
    GLuint vao_id;
    GLuint program_id;
    GLuint vertices_id;
    GLint position_location;
    GLint texture_location;
    GLint texture_matrix_location;
};

static const char oes_copy_vertex_data[] =
    "#version 100"                                                                      "\n"
    "precision highp float;"                                                            "\n"
    "attribute vec4 position;"                                                          "\n"
    "uniform mat4 tex_coord_matrix;"                                                    "\n"
    "varying vec2 tex_coord;"                                                           "\n"
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    gl_Position = vec4(position.xy, 0.0, 1.0);"                                    "\n"
    "    tex_coord = (tex_coord_matrix * vec4(position.zw, 0.0, 1.0)).xy;"              "\n"
    "}";

static const char oes_copy_fragment_data[] = ""
    "#version 100"                                                                      "\n"
    "#extension GL_OES_EGL_image_external : require"                                    "\n"
    "precision mediump float;"                                                          "\n"
    "uniform samplerExternalOES tex;"                                                   "\n"
    "varying vec2 tex_coord;"                                                           "\n"
    "void main(void)"                                                                   "\n"
    "{"                                                                                 "\n"
    "    vec4 color = texture2D(tex, tex_coord);"                                       "\n"
    "    gl_FragColor = vec4(color.rgb, 1.0);"                                          "\n"
    "}";

static int mc_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;
    struct hwupload_mc *mc = s->hwupload_priv_data;

    s->data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    ret = ngli_texture_update_data(node, frame->width, frame->height, 0, NULL);
    if (ret < 0)
        return ret;

    GLuint framebuffer_id;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

    ngli_glGenFramebuffers(gl, 1, &mc->framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, mc->framebuffer_id);
    ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->id, 0);
    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", mc->framebuffer_id);
        goto fail;
    }

    mc->program_id = ngli_program_load(gl, oes_copy_vertex_data, oes_copy_fragment_data);
    if (!mc->program_id)
        goto fail;
    ngli_glUseProgram(gl, mc->program_id);

    mc->position_location = ngli_glGetAttribLocation(gl, mc->program_id, "position");
    if (mc->position_location < 0)
        goto fail;

    mc->texture_location = ngli_glGetUniformLocation(gl, mc->program_id, "tex");
    if (mc->texture_location < 0)
        goto fail;
    ngli_glUniform1i(gl, mc->texture_location, 0);

    mc->texture_matrix_location = ngli_glGetUniformLocation(gl, mc->program_id, "tex_coord_matrix");
    if (mc->texture_matrix_location < 0)
        goto fail;

    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
    };
    ngli_glGenBuffers(gl, 1, &mc->vertices_id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, mc->vertices_id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &mc->vao_id);
        ngli_glBindVertexArray(gl, mc->vao_id);

        ngli_glEnableVertexAttribArray(gl, mc->position_location);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, mc->vertices_id);
        ngli_glVertexAttribPointer(gl, mc->position_location, 4, GL_FLOAT, GL_FALSE, 4 * 4, NULL);
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
    return 0;
fail:
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
    return -1;
}

static void mc_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;
    struct hwupload_mc *mc = s->hwupload_priv_data;

    ngli_glDeleteFramebuffers(gl, 1, &mc->framebuffer_id);
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glDeleteVertexArrays(gl, 1, &mc->vao_id);
    ngli_glDeleteProgram(gl, mc->program_id);
    ngli_glDeleteBuffers(gl, 1, &mc->vertices_id);
}

static int mc_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;
    struct hwupload_mc *mc = s->hwupload_priv_data;

    struct media *media = s->data_src->priv_data;
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;

    NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    int ret = ngli_texture_update_data(node, frame->width, frame->height, 0, NULL);
    if (ret < 0)
        return ret;

    if (ret) {
        mc_uninit(node);
        ret = mc_init(node, frame);
        if (ret < 0)
            return ret;
    }

    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);

    GLuint framebuffer_id;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, mc->framebuffer_id);

    GLint viewport[4];
    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, frame->width, frame->height);
    ngli_glClear(gl, GL_COLOR_BUFFER_BIT);

    ngli_glUseProgram(gl, mc->program_id);
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glBindVertexArray(gl, mc->vao_id);
    } else {
        ngli_glEnableVertexAttribArray(gl, mc->position_location);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, mc->vertices_id);
        ngli_glVertexAttribPointer(gl, mc->position_location, 4, GL_FLOAT, GL_FALSE, 4 * 4, NULL);

    }
    ngli_glActiveTexture(gl, GL_TEXTURE0);
    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, media->android_texture_id);
    ngli_glUniformMatrix4fv(gl, mc->texture_matrix_location, 1, GL_FALSE, matrix);
    ngli_glDrawArrays(gl, GL_TRIANGLE_FAN, 0, 4);
    if (!(gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)) {
        ngli_glDisableVertexAttribArray(gl, mc->position_location);
    }

    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);

    return 0;
}

static int mc_dr_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;

    GLint id = media->android_texture_id;
    GLenum target = media->android_texture_target;

    ngli_glBindTexture(gl, target, id);
    ngli_glTexParameteri(gl, target, GL_TEXTURE_MIN_FILTER, s->min_filter);
    ngli_glTexParameteri(gl, target, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    ngli_glBindTexture(gl, target, 0);

    s->layout = NGLI_TEXTURE_LAYOUT_MEDIACODEC;
    s->planes[0].id = id;
    s->planes[0].target = target;

    return 0;
}

static int mc_dr_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;

    NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    NGLI_ALIGNED_MAT(flip_matrix) = {
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  1.0f, 0.0f, 1.0f,
    };

    s->width  = frame->width;
    s->height = frame->height;

    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);
    ngli_mat4_mul(s->coordinates_matrix, flip_matrix, matrix);

    return 0;
}

static const struct hwmap_class hwmap_mc_class = {
    .name      = "mediacodec (oes → 2d)",
    .priv_size = sizeof(struct hwupload_mc),
    .init      = mc_init,
    .map_frame = mc_map_frame,
    .uninit    = mc_uninit,
};

static const struct hwmap_class hwmap_mc_dr_class = {
    .name      = "mediacodec (oes zero-copy)",
    .init      = mc_dr_init,
    .map_frame = mc_dr_map_frame,
};

static const struct hwmap_class *mc_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;

    if (s->direct_rendering) {
        if (s->min_filter != GL_NEAREST && s->min_filter != GL_LINEAR) {
            LOG(WARNING, "external textures only support nearest and linear filtering: "
                "disabling direct rendering");
            s->direct_rendering = 0;
        } else if (s->wrap_s != GL_CLAMP_TO_EDGE || s->wrap_t != GL_CLAMP_TO_EDGE) {
            LOG(WARNING, "external textures only support clamp to edge wrapping: "
                "disabling direct rendering");
            s->direct_rendering = 0;
        }
    }

    if (s->direct_rendering)
        return &hwmap_mc_dr_class;

    return &hwmap_mc_class;
}

const struct hwupload_class ngli_hwupload_mc_class = {
    .get_hwmap = mc_get_hwmap,
};
