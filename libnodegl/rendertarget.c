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

#include <string.h>

#include "rendertarget.h"
#include "format.h"
#include "glcontext.h"
#include "glincludes.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "texture.h"
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

static void blit(struct rendertarget *s, struct rendertarget *dst, int vflip, int flags)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;

    if (vflip)
        ngli_glBlitFramebuffer(gl, 0, 0, s->width, s->height, 0, dst->height, dst->width, 0, flags, GL_NEAREST);
    else
        ngli_glBlitFramebuffer(gl, 0, 0, s->width, s->height, 0, 0, dst->width, dst->height, flags, GL_NEAREST);
}

static void blit_no_draw_buffers(struct rendertarget *s, struct rendertarget *dst, int vflip)
{
    blit(s, dst, vflip, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void blit_draw_buffers(struct rendertarget *s, struct rendertarget *dst, int vflip)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;

    const GLenum *draw_buffers = s->blit_draw_buffers;
    const int nb_color_attachments = NGLI_MIN(s->nb_color_attachments, dst->nb_color_attachments);
    for (int i = 0; i < nb_color_attachments; i++) {
        GLbitfield flags = GL_COLOR_BUFFER_BIT;
        if (i == 0)
            flags |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        ngli_glReadBuffer(gl, GL_COLOR_ATTACHMENT0 + i);
        ngli_glDrawBuffers(gl, i + 1, draw_buffers);
        draw_buffers += i + 1;
        blit(s, dst, vflip, flags);
    }
    ngli_glReadBuffer(gl, GL_COLOR_ATTACHMENT0);
    ngli_glDrawBuffers(gl, s->nb_draw_buffers, s->draw_buffers);
}

int ngli_rendertarget_init(struct rendertarget *s, struct ngl_ctx *ctx, const struct rendertarget_params *params)
{
    int ret = -1;
    struct glcontext *gl = ctx->glcontext;

    s->ctx = ctx;
    s->width = params->width;
    s->height = params->height;

    ngli_glGenFramebuffers(gl, 1, &s->id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->id);

    s->nb_color_attachments = 0;
    for (int i = 0; i < params->nb_colors; i++) {
        const struct texture *attachment = params->colors[i];
        GLenum attachment_index = get_gl_attachment_index(attachment->format);
        ngli_assert(attachment_index == GL_COLOR_ATTACHMENT0);

        if (s->nb_color_attachments >= gl->max_color_attachments) {
            LOG(ERROR, "could not attach color buffer %d (maximum %d)",
                s->nb_color_attachments, gl->max_color_attachments);
            goto done;
        }
        attachment_index = attachment_index + s->nb_color_attachments++;

        switch (attachment->target) {
        case GL_RENDERBUFFER:
            ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, attachment->id);
            break;
        case GL_TEXTURE_2D:
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, attachment->id, 0);
            break;
        case GL_TEXTURE_CUBE_MAP:
            for (int face = 0; face < 6; face++)
                ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index++, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, attachment->id, 0);
            s->nb_color_attachments += 5;
            break;
        default:
            ngli_assert(0);
        }
    }

    if (params->depth_stencil) {
        const struct texture *attachment = params->depth_stencil;
        const GLenum attachment_index = get_gl_attachment_index(attachment->format);
        ngli_assert(attachment_index != GL_COLOR_ATTACHMENT0);

        switch (attachment->target) {
        case GL_RENDERBUFFER:
            if (gl->backend == NGL_BACKEND_OPENGLES && gl->version < 300 && attachment_index == GL_DEPTH_STENCIL_ATTACHMENT) {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
            } else {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, attachment->id);
            }
            break;
        case GL_TEXTURE_2D:
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, attachment->id, 0);
            break;
        default:
            ngli_assert(0);
        }
    }

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", s->id);
        goto done;
    }

    s->blit = blit_no_draw_buffers;
    if (gl->features & NGLI_FEATURE_DRAW_BUFFERS) {
        s->nb_draw_buffers = s->nb_color_attachments;
        if (s->nb_draw_buffers > gl->max_draw_buffers) {
            LOG(ERROR, "draw buffer count (%d) exceeds driver limit (%d)",
                s->nb_draw_buffers, gl->max_draw_buffers);
            goto done;
        }
        if (s->nb_draw_buffers > 1) {
            s->draw_buffers = ngli_calloc(s->nb_draw_buffers, sizeof(*s->draw_buffers));
            if (!s->draw_buffers)
                goto done;
            for (int i = 0; i < s->nb_draw_buffers; i++)
                s->draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
            ngli_glDrawBuffers(gl, s->nb_draw_buffers, s->draw_buffers);

            const int nb_blit_draw_buffers = s->nb_draw_buffers * (s->nb_draw_buffers + 1) / 2;
            s->blit_draw_buffers = ngli_calloc(nb_blit_draw_buffers, sizeof(*s->blit_draw_buffers));
            if (!s->blit_draw_buffers)
                goto done;

            GLenum *draw_buffers = s->blit_draw_buffers;
            for (int i = 0; i < s->nb_draw_buffers; i++) {
                draw_buffers += i + 1;
                draw_buffers[-1] = GL_COLOR_ATTACHMENT0 + i;
            }

            s->blit = blit_draw_buffers;
        }
    }

    ret = 0;

done:;
    struct rendertarget *rt = ctx->rendertarget;
    const GLuint fbo_id = rt ? rt->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    return ret;
}

void ngli_rendertarget_blit(struct rendertarget *s, struct rendertarget *dst, int vflip)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT))
        return;

    ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, s->id);
    ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, dst->id);
    s->blit(s, dst, vflip);

    struct rendertarget *rt = ctx->rendertarget;
    const GLuint fbo_id = rt ? rt->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);
}

void ngli_rendertarget_read_pixels(struct rendertarget *s, uint8_t *data)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rendertarget *rt = ctx->rendertarget;

    const GLuint fbo_id = rt ? rt->id : ngli_glcontext_get_default_framebuffer(gl);
    if (s->id != fbo_id)
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->id);

    ngli_glReadPixels(gl, 0, 0, s->width, s->height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    if (s->id != fbo_id)
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);
}

void ngli_rendertarget_reset(struct rendertarget *s)
{
    struct ngl_ctx *ctx = s->ctx;
    if (!ctx)
        return;

    struct glcontext *gl = ctx->glcontext;
    ngli_glDeleteFramebuffers(gl, 1, &s->id);

    ngli_free(s->draw_buffers);
    ngli_free(s->blit_draw_buffers);

    memset(s, 0, sizeof(*s));
}
