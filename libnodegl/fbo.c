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

#include "fbo.h"
#include "format.h"
#include "glcontext.h"
#include "glincludes.h"
#include "log.h"
#include "memory.h"
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

static const GLenum depth_stencil_attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};

int ngli_fbo_init(struct fbo *fbo, struct glcontext *gl, const struct fbo_params *params)
{
    int ret = -1;

    fbo->gl = gl;
    fbo->width = params->width;
    fbo->height = params->height;

    ngli_darray_init(&fbo->depth_indices, sizeof(GLenum), 0);

    GLuint fbo_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&fbo_id);

    ngli_glGenFramebuffers(gl, 1, &fbo->id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo->id);

    fbo->nb_color_attachments = 0;
    for (int i = 0; i < params->nb_attachments; i++) {
        const struct texture *attachment = params->attachments[i];

        GLenum attachment_index = get_gl_attachment_index(attachment->format);
        const int is_color_attachment = attachment_index == GL_COLOR_ATTACHMENT0;
        if (is_color_attachment) {
            if (fbo->nb_color_attachments >= gl->max_color_attachments) {
                LOG(ERROR, "could not attach color buffer %d (maximum %d)",
                    fbo->nb_color_attachments, gl->max_color_attachments);
                goto done;
            }
            attachment_index = attachment_index + fbo->nb_color_attachments++;
        }

        switch (attachment->target) {
        case GL_RENDERBUFFER:
            if (gl->backend == NGL_BACKEND_OPENGLES && gl->version < 300 && attachment_index == GL_DEPTH_STENCIL_ATTACHMENT) {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
                if (!ngli_darray_push(&fbo->depth_indices, depth_stencil_attachments) ||
                    !ngli_darray_push(&fbo->depth_indices, depth_stencil_attachments + 1))
                    return -1;
            } else {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, attachment->id);
                if (!is_color_attachment) {
                    if (gl->platform == NGL_PLATFORM_IOS && attachment_index == GL_DEPTH_STENCIL_ATTACHMENT) {
                        if (!ngli_darray_push(&fbo->depth_indices, depth_stencil_attachments) ||
                            !ngli_darray_push(&fbo->depth_indices, depth_stencil_attachments + 1))
                            return -1;
                    } else {
                        if (!ngli_darray_push(&fbo->depth_indices, &attachment_index))
                            return -1;
                    }
                }
            }
            break;
        case GL_TEXTURE_2D:
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, attachment->id, 0);
            break;
        case GL_TEXTURE_CUBE_MAP:
            for (int face = 0; face < 6; face++)
                ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index++, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, attachment->id, 0);
            fbo->nb_color_attachments += 5;
            break;
        default:
            ngli_assert(0);
        }
    }

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", fbo->id);
        goto done;
    }

    if (gl->features & NGLI_FEATURE_DRAW_BUFFERS) {
        fbo->nb_draw_buffers = fbo->nb_color_attachments;
        if (fbo->nb_draw_buffers > gl->max_draw_buffers) {
            LOG(ERROR, "draw buffer count (%d) exceeds driver limit (%d)",
                fbo->nb_draw_buffers, gl->max_draw_buffers);
            goto done;
        }
        if (fbo->nb_draw_buffers > 1) {
            fbo->draw_buffers = ngli_calloc(fbo->nb_draw_buffers, sizeof(*fbo->draw_buffers));
            if (!fbo->draw_buffers)
                goto done;
            for (int i = 0; i < fbo->nb_draw_buffers; i++)
                fbo->draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
            ngli_glDrawBuffers(gl, fbo->nb_draw_buffers, fbo->draw_buffers);
        }
    }

    ret = 0;
done:
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    return ret;
}

int ngli_fbo_bind(struct fbo *fbo)
{
    struct glcontext *gl = fbo->gl;

    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&fbo->prev_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo->id);

    return 0;
}

int ngli_fbo_unbind(struct fbo *fbo)
{
    struct glcontext *gl = fbo->gl;

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo->prev_id);
    fbo->prev_id = 0;

    return 0;
}

void ngli_fbo_invalidate_depth_buffers(struct fbo *fbo)
{
    struct glcontext *gl = fbo->gl;

    if (!(gl->features & NGLI_FEATURE_INVALIDATE_SUBDATA))
        return;

    int nb_attachments = ngli_darray_count(&fbo->depth_indices);
    if (nb_attachments) {
        GLenum *attachments = ngli_darray_data(&fbo->depth_indices);
        ngli_glInvalidateFramebuffer(gl, GL_FRAMEBUFFER, nb_attachments, attachments);
    }
}

void ngli_fbo_blit(struct fbo *fbo, struct fbo *dst, int vflip)
{
    struct glcontext *gl = fbo->gl;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT))
        return;

    ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, dst->id);
    if (vflip)
        ngli_glBlitFramebuffer(gl,
                               0, 0, fbo->width, fbo->height, 0, dst->height, dst->width, 0,
                               GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                               GL_NEAREST);
    else
        ngli_glBlitFramebuffer(gl,
                               0, 0, fbo->width, fbo->height, 0, 0, dst->width, dst->height,
                               GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                               GL_NEAREST);
    ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, fbo->id);
}

void ngli_fbo_read_pixels(struct fbo *fbo, uint8_t *data)
{
    struct glcontext *gl = fbo->gl;
    ngli_glReadPixels(gl, 0, 0, fbo->width, fbo->height, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

void ngli_fbo_reset(struct fbo *fbo)
{
    struct glcontext *gl = fbo->gl;
    if (!gl)
        return;

    ngli_glDeleteFramebuffers(gl, 1, &fbo->id);

    ngli_darray_reset(&fbo->depth_indices);
    ngli_free(fbo->draw_buffers);

    memset(fbo, 0, sizeof(*fbo));
}
