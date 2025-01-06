/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2018-2022 GoPro Inc.
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

#include "config.h"
#include "gpu_rendertarget_gl.h"
#include "gpu_ctx_gl.h"
#include "glcontext.h"
#include "glincludes.h"
#include "log.h"
#include "memory.h"
#include "gpu_texture_gl.h"
#include "utils.h"

static GLenum get_gl_attachment_index(GLenum format)
{
    switch (format) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
        return GL_DEPTH_ATTACHMENT;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
        return GL_DEPTH_STENCIL_ATTACHMENT;
    case GL_STENCIL_INDEX:
    case GL_STENCIL_INDEX8:
        return GL_STENCIL_ATTACHMENT;
    default:
        return GL_COLOR_ATTACHMENT0;
    }
}

static void resolve_no_draw_buffers(struct gpu_rendertarget *s)
{
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    const GLbitfield flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    gl->funcs.BlitFramebuffer(0, 0, s->width, s->height, 0, 0, s->width, s->height, flags, GL_NEAREST);
}

static void resolve_draw_buffers(struct gpu_rendertarget *s)
{
    struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct gpu_rendertarget_params *params = &s->params;

    for (size_t i = 0; i < params->nb_colors; i++) {
        const struct gpu_attachment *attachment = &params->colors[i];
        if (!attachment->resolve_target)
            continue;
        GLbitfield flags = GL_COLOR_BUFFER_BIT;
        if (i == 0)
            flags |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        gl->funcs.ReadBuffer(GL_COLOR_ATTACHMENT0 + (GLenum)i);
        GLenum draw_buffers[NGLI_GPU_MAX_COLOR_ATTACHMENTS] = {0};
        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + (GLenum)i;
        gl->funcs.DrawBuffers((GLsizei)i + 1, draw_buffers);
        gl->funcs.BlitFramebuffer(0, 0, s->width, s->height, 0, 0, s->width, s->height, flags, GL_NEAREST);
    }
    gl->funcs.ReadBuffer(GL_COLOR_ATTACHMENT0);
    gl->funcs.DrawBuffers((GLsizei)params->nb_colors, s_priv->draw_buffers);
}

static int create_fbo(struct gpu_rendertarget *s, int resolve, GLuint *idp)
{
    int ret = -1;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct gpu_limits *limits = &gl->limits;
    const struct gpu_rendertarget_params *params = &s->params;

    GLuint id = 0;
    int nb_color_attachments = 0;

    gl->funcs.GenFramebuffers(1, &id);
    gl->funcs.BindFramebuffer(GL_FRAMEBUFFER, id);

    for (size_t i = 0; i < params->nb_colors; i++) {
        const struct gpu_attachment *attachment = &params->colors[i];
        const struct gpu_texture *texture = resolve ? attachment->resolve_target : attachment->attachment;
        const int layer = resolve ? attachment->resolve_target_layer : attachment->attachment_layer;

        if (!texture)
            continue;

        const struct gpu_texture_gl *texture_gl = (const struct gpu_texture_gl *)texture;
        GLenum attachment_index = get_gl_attachment_index(texture_gl->format);
        ngli_assert(attachment_index == GL_COLOR_ATTACHMENT0);
        ngli_assert(nb_color_attachments < limits->max_color_attachments);
        attachment_index = attachment_index + nb_color_attachments++;

        switch (texture_gl->target) {
        case GL_RENDERBUFFER:
            gl->funcs.FramebufferRenderbuffer(GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, texture_gl->id);
            break;
        case GL_TEXTURE_2D:
            gl->funcs.FramebufferTexture2D(GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, texture_gl->id, 0);
            break;
        case GL_TEXTURE_2D_ARRAY:
            gl->funcs.FramebufferTextureLayer(GL_FRAMEBUFFER, attachment_index, texture_gl->id, 0, layer);
            break;
        case GL_TEXTURE_3D:
            gl->funcs.FramebufferTextureLayer(GL_FRAMEBUFFER, attachment_index, texture_gl->id, 0, layer);
            break;
        case GL_TEXTURE_CUBE_MAP:
            gl->funcs.FramebufferTexture2D(GL_FRAMEBUFFER, attachment_index++, GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, texture_gl->id, 0);
            break;
        default:
            ngli_assert(0);
        }
    }

    const struct gpu_attachment *attachment = &params->depth_stencil;
    struct gpu_texture *texture = resolve ? attachment->resolve_target : attachment->attachment;
    if (texture) {
        const struct gpu_texture_gl *texture_gl = (const struct gpu_texture_gl *)texture;
        const GLenum attachment_index = get_gl_attachment_index(texture_gl->format);
        ngli_assert(attachment_index != GL_COLOR_ATTACHMENT0);

        switch (texture_gl->target) {
        case GL_RENDERBUFFER:
            gl->funcs.FramebufferRenderbuffer(GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, texture_gl->id);
            break;
        case GL_TEXTURE_2D:
            gl->funcs.FramebufferTexture2D(GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, texture_gl->id, 0);
            break;
        default:
            ngli_assert(0);
        }
    }

    if (gl->funcs.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", id);
        goto fail;
    }

    *idp = id;

    return 0;

fail:
    gl->funcs.DeleteFramebuffers(1, &id);
    return ret;
}

static int require_resolve_fbo(struct gpu_rendertarget *s)
{
    const struct gpu_rendertarget_params *params = &s->params;
    for (size_t i = 0; i < params->nb_colors; i++) {
        const struct gpu_attachment *attachment = &params->colors[i];
        if (attachment->resolve_target)
            return 1;
    }
    return 0;
}

static void clear_buffers(struct gpu_rendertarget *s)
{
    struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct gpu_rendertarget_params *params = &s->params;

    for (size_t i = 0; i < params->nb_colors; i++) {
        const struct gpu_attachment *color = &params->colors[i];
        if (color->load_op != NGLI_GPU_LOAD_OP_LOAD) {
            gl->funcs.ClearBufferfv(GL_COLOR, (GLint)i, color->clear_value);
        }
    }

    if (params->depth_stencil.attachment || s_priv->wrapped) {
        const struct gpu_attachment *depth_stencil = &params->depth_stencil;
        if (depth_stencil->load_op != NGLI_GPU_LOAD_OP_LOAD) {
            gl->funcs.ClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
        }
    }
}

static void invalidate_noop(struct gpu_rendertarget *s)
{
}

static void invalidate(struct gpu_rendertarget *s)
{
    struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    gl->funcs.InvalidateFramebuffer(GL_FRAMEBUFFER, s_priv->nb_invalidate_attachments, s_priv->invalidate_attachments);
}

struct gpu_rendertarget *ngli_gpu_rendertarget_gl_create(struct gpu_ctx *gpu_ctx)
{
    struct gpu_rendertarget_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct gpu_rendertarget *)s;
}

int ngli_gpu_rendertarget_gl_init(struct gpu_rendertarget *s)
{
    struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;
    struct gpu_ctx *gpu_ctx = (struct gpu_ctx *)s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct gpu_limits *limits = &gl->limits;

    s_priv->wrapped = 0;

    int ret;
    if (require_resolve_fbo(s)) {
        ret = create_fbo(s, 1, &s_priv->resolve_id);
        if (ret < 0)
            goto done;
    }

    ret = create_fbo(s, 0, &s_priv->id);
    if (ret < 0)
        goto done;

    if (gl->features & NGLI_FEATURE_GL_INVALIDATE_SUBDATA) {
        s_priv->invalidate = invalidate;
    } else {
        s_priv->invalidate = invalidate_noop;
    }

    s_priv->clear = clear_buffers;
    s_priv->resolve = resolve_no_draw_buffers;

    ngli_assert(s->params.nb_colors <= limits->max_draw_buffers);
    if (s->params.nb_colors > 1) {
        for (size_t i = 0; i < s->params.nb_colors; i++)
            s_priv->draw_buffers[i] = GL_COLOR_ATTACHMENT0 + (GLenum)i;
        gl->funcs.DrawBuffers((GLsizei)s->params.nb_colors, s_priv->draw_buffers);
        s_priv->resolve = resolve_draw_buffers;
    }

    for (size_t i = 0; i < s->params.nb_colors; i++) {
        const struct gpu_attachment *color = &s->params.colors[i];
        if (color->load_op == NGLI_GPU_LOAD_OP_DONT_CARE ||
            color->load_op == NGLI_GPU_LOAD_OP_CLEAR) {
            s_priv->clear_flags |= GL_COLOR_BUFFER_BIT;
        }
        if (color->store_op == NGLI_GPU_STORE_OP_DONT_CARE) {
            s_priv->invalidate_attachments[s_priv->nb_invalidate_attachments++] = GL_COLOR_ATTACHMENT0 + (GLenum)i;
        }
    }

    const struct gpu_attachment *depth_stencil = &s->params.depth_stencil;
    if (depth_stencil->attachment) {
        if (depth_stencil->load_op == NGLI_GPU_LOAD_OP_DONT_CARE ||
            depth_stencil->load_op == NGLI_GPU_LOAD_OP_CLEAR) {
            s_priv->clear_flags |= (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
        if (depth_stencil->store_op == NGLI_GPU_STORE_OP_DONT_CARE) {
            s_priv->invalidate_attachments[s_priv->nb_invalidate_attachments++] = GL_DEPTH_ATTACHMENT;
            s_priv->invalidate_attachments[s_priv->nb_invalidate_attachments++] = GL_STENCIL_ATTACHMENT;
        }
    }

done:;
    struct gpu_rendertarget *rt = gpu_ctx->rendertarget;
    struct gpu_rendertarget_gl *rt_gl = (struct gpu_rendertarget_gl *)rt;
    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    gl->funcs.BindFramebuffer(GL_FRAMEBUFFER, fbo_id);

    return ret;
}

void ngli_gpu_rendertarget_gl_begin_pass(struct gpu_rendertarget *s)
{
    const struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;

    static const GLboolean default_color_write_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    if (memcmp(glstate->color_write_mask, default_color_write_mask, sizeof(default_color_write_mask))) {
        gl->funcs.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        memcpy(glstate->color_write_mask, &default_color_write_mask, sizeof(default_color_write_mask));
    }

    if (glstate->depth_write_mask != GL_TRUE) {
        gl->funcs.DepthMask(GL_TRUE);
        glstate->depth_write_mask = GL_TRUE;
    }

    if (glstate->stencil_write_mask != 0xff) {
        gl->funcs.StencilMask(0xff);
        glstate->stencil_write_mask = 0xff;
    }

    ngli_glstate_enable_scissor_test(gl, glstate, 0);

    gl->funcs.BindFramebuffer(GL_FRAMEBUFFER, s_priv->id);

    const struct gpu_viewport viewport = {
        .x      = 0,
        .y      = 0,
        .width  = s->width,
        .height = s->height,
    };
    ngli_glstate_update_viewport(gl, glstate, &viewport);

    const struct gpu_scissor scissor = {
        .x      = 0,
        .y      = 0,
        .width  = s->width,
        .height = s->height,
    };
    ngli_glstate_update_scissor(gl, glstate, &scissor);

    s_priv->clear(s);

    ngli_glstate_enable_scissor_test(gl, glstate, 1);
}

void ngli_gpu_rendertarget_gl_end_pass(struct gpu_rendertarget *s)
{
    const struct gpu_rendertarget_gl *s_priv = (const struct gpu_rendertarget_gl *)s;
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct glstate *glstate = &gpu_ctx_gl->glstate;

    if (s_priv->resolve_id) {
        gl->funcs.BindFramebuffer(GL_READ_FRAMEBUFFER, s_priv->id);
        gl->funcs.BindFramebuffer(GL_DRAW_FRAMEBUFFER, s_priv->resolve_id);

        ngli_glstate_enable_scissor_test(gl, glstate, 0);

        s_priv->resolve(s);

        ngli_glstate_enable_scissor_test(gl, glstate, 1);

        struct gpu_rendertarget *rt = gpu_ctx->rendertarget;
        struct gpu_rendertarget_gl *rt_gl = (struct gpu_rendertarget_gl *)rt;
        const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
        gl->funcs.BindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    }

    s_priv->invalidate(s);
}

void ngli_gpu_rendertarget_gl_freep(struct gpu_rendertarget **sp)
{
    if (!*sp)
        return;

    struct gpu_rendertarget *s = *sp;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;

    if (!s_priv->wrapped) {
        gl->funcs.DeleteFramebuffers(1, &s_priv->id);
        gl->funcs.DeleteFramebuffers(1, &s_priv->resolve_id);
    }

    ngli_freep(sp);
}

int ngli_gpu_rendertarget_gl_wrap(struct gpu_rendertarget *s, const struct gpu_rendertarget_params *params, GLuint id)
{
    struct gpu_rendertarget_gl *s_priv = (struct gpu_rendertarget_gl *)s;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    ngli_assert(params->nb_colors == 1);
    ngli_assert(!params->colors[0].attachment);
    ngli_assert(!params->colors[0].resolve_target);
    ngli_assert(!params->depth_stencil.attachment);
    ngli_assert(!params->depth_stencil.resolve_target);

    s->params = *params;
    s->width = params->width;
    s->height = params->height;

    s_priv->wrapped = 1;
    s_priv->id = id;

    if (gl->features & NGLI_FEATURE_GL_INVALIDATE_SUBDATA) {
        s_priv->invalidate = invalidate;
    } else {
        s_priv->invalidate = invalidate_noop;
    }

    s_priv->clear = clear_buffers;
    s_priv->resolve = resolve_no_draw_buffers;

    const struct gpu_attachment *color = &params->colors[0];
    if (color->load_op == NGLI_GPU_LOAD_OP_DONT_CARE ||
        color->load_op == NGLI_GPU_LOAD_OP_CLEAR) {
        s_priv->clear_flags |= GL_COLOR_BUFFER_BIT;
    }
    if (color->store_op == NGLI_GPU_STORE_OP_DONT_CARE) {
        s_priv->invalidate_attachments[s_priv->nb_invalidate_attachments++] = s_priv->id ? GL_COLOR_ATTACHMENT0 : GL_COLOR;
    }

    const struct gpu_attachment *depth_stencil = &params->depth_stencil;
    if (depth_stencil->load_op == NGLI_GPU_LOAD_OP_DONT_CARE ||
        depth_stencil->load_op == NGLI_GPU_LOAD_OP_CLEAR) {
        s_priv->clear_flags |= (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    if (depth_stencil->store_op == NGLI_GPU_STORE_OP_DONT_CARE) {
        s_priv->invalidate_attachments[s_priv->nb_invalidate_attachments++] = s_priv->id ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
        s_priv->invalidate_attachments[s_priv->nb_invalidate_attachments++] = s_priv->id ? GL_STENCIL_ATTACHMENT : GL_STENCIL;
    }

    return 0;
}
