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

enum fbo_attachment_type {
    FBO_ATTACHMENT_TYPE_NONE,
    FBO_ATTACHMENT_TYPE_RENDERBUFFER,
    FBO_ATTACHMENT_TYPE_TEXTURE,
};

struct fbo_attachment {
    enum fbo_attachment_type type;
    int is_external;
    GLuint id;
    GLenum index;
    GLenum format;
};

static GLenum get_gl_index(GLenum format)
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

static int has_attachment(struct fbo *fbo, int index)
{
    struct fbo_attachment *attachments = ngli_darray_data(&fbo->attachments);
    for (int i = 0; i < ngli_darray_count(&fbo->attachments); i++) {
        struct fbo_attachment *attachment = &attachments[i];
        if (attachment->index == index)
            return 1;
    }
    return 0;
}

int ngli_fbo_init(struct fbo *fbo, struct glcontext *gl, int width, int height, int samples)
{
    fbo->gl = gl;
    fbo->width = width;
    fbo->height = height;
    fbo->samples = samples;
    fbo->id = 0;
    ngli_darray_init(&fbo->attachments, sizeof(struct fbo_attachment), 0);
    ngli_darray_init(&fbo->depth_indices, sizeof(GLenum), 0);
    return 0;
}

int ngli_fbo_resize(struct fbo *fbo, int width, int height)
{
    struct glcontext *gl = fbo->gl;

    fbo->width = width;
    fbo->height = height;

    struct fbo_attachment *attachments = ngli_darray_data(&fbo->attachments);
    for (int i = 0; i < ngli_darray_count(&fbo->attachments); i++) {
        struct fbo_attachment *attachment = &attachments[i];
        if (!attachment->id || attachment->is_external)
            continue;
        if (attachment->type == FBO_ATTACHMENT_TYPE_RENDERBUFFER) {
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, attachment->id);
            if (fbo->samples > 0)
                ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, fbo->samples, attachment->format, fbo->width, fbo->height);
            else
                ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, attachment->format, fbo->width, fbo->height);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
        }
    }

    return 0;
}

int ngli_fbo_create_renderbuffer(struct fbo *fbo, int format)
{
    struct glcontext *gl = fbo->gl;

    GLenum gl_format;
    int ret = ngli_format_get_gl_renderbuffer_format(gl, format, (GLint *)&gl_format);
    if (ret < 0)
        return ret;

    GLenum gl_index = get_gl_index(gl_format);
    ngli_assert(!has_attachment(fbo, gl_index));

    if (gl->features & NGLI_FEATURE_INTERNALFORMAT_QUERY) {
        GLint samples;
        ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, gl_format, GL_SAMPLES, 1, &samples);
        if (fbo->samples > samples) {
            LOG(WARNING, "renderbuffer format 0x%x does not support requested samples %d (maximum %d)", gl_format, fbo->samples, samples);
            fbo->samples = samples;
        }
    }

    struct fbo_attachment *attachment = ngli_darray_push(&fbo->attachments, NULL);
    if (!attachment)
        return -1;

    attachment->type = FBO_ATTACHMENT_TYPE_RENDERBUFFER;
    attachment->is_external = 0;
    attachment->id = 0;
    attachment->index = gl_index;
    attachment->format = gl_format;

    return 0;
}

static int attach(struct fbo *fbo, enum fbo_attachment_type type, int format, GLuint id)
{
    struct glcontext *gl = fbo->gl;

    GLenum gl_format;
    int ret = ngli_format_get_gl_renderbuffer_format(gl, format, (GLint *)&gl_format);
    if (ret < 0)
        return ret;

    GLenum gl_index = get_gl_index(gl_format);
    ngli_assert(!has_attachment(fbo, gl_index));

    struct fbo_attachment *attachment = ngli_darray_push(&fbo->attachments, NULL);
    if (!attachment)
        return -1;

    attachment->type = type;
    attachment->is_external = 1;
    attachment->id = id;
    attachment->index = gl_index;
    attachment->format = gl_format;

    return 0;
}

int ngli_fbo_attach_renderbuffer(struct fbo *fbo, int format, GLuint renderbuffer)
{
    return attach(fbo, FBO_ATTACHMENT_TYPE_RENDERBUFFER, format, renderbuffer);
}

int ngli_fbo_attach_texture(struct fbo *fbo, int format, GLuint texture)
{
    return attach(fbo, FBO_ATTACHMENT_TYPE_TEXTURE, format, texture);
}

int ngli_fbo_allocate(struct fbo *fbo)
{
    struct glcontext *gl = fbo->gl;

    GLuint fbo_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&fbo_id);

    ngli_glGenFramebuffers(gl, 1, &fbo->id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo->id);

    struct fbo_attachment *attachments = ngli_darray_data(&fbo->attachments);
    for (int i = 0; i < ngli_darray_count(&fbo->attachments); i++) {
        struct fbo_attachment *attachment = &attachments[i];
        if (attachment->type == FBO_ATTACHMENT_TYPE_RENDERBUFFER) {
            if (!attachment->is_external) {
                ngli_glGenRenderbuffers(gl, 1, &attachment->id);
                ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, attachment->id);
                if (fbo->samples > 0)
                    ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, fbo->samples, attachment->format, fbo->width, fbo->height);
                else
                    ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, attachment->format, fbo->width, fbo->height);
                ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
            }
            if (gl->backend == NGL_BACKEND_OPENGLES && gl->version < 300 && attachment->index == GL_DEPTH_STENCIL_ATTACHMENT) {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
            } else {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment->index, GL_RENDERBUFFER, attachment->id);
            }
        } else if (attachment->type == FBO_ATTACHMENT_TYPE_TEXTURE) {
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment->index, GL_TEXTURE_2D, attachment->id, 0);
        } else {
            ngli_assert(0);
        }
        if (attachment->index != GL_COLOR_ATTACHMENT0) {
            ngli_darray_push(&fbo->depth_indices, &attachment->index);
        }
    }

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", fbo->id);
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);
        return -1;
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);
    return 0;
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

void ngli_fbo_blit(struct fbo *fbo, struct fbo *dst)
{
    struct glcontext *gl = fbo->gl;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT))
        return;

    ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, dst->id);
    ngli_glBlitFramebuffer(gl, 0, 0, dst->width, dst->height, 0, 0, fbo->width, fbo->height,
                           GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, fbo->id);
}

void ngli_fbo_reset(struct fbo *fbo)
{
    struct glcontext *gl = fbo->gl;
    if (!gl)
        return;

    ngli_glDeleteFramebuffers(gl, 1, &fbo->id);

    struct fbo_attachment *attachments = ngli_darray_data(&fbo->attachments);
    for (int i = 0; i < ngli_darray_count(&fbo->attachments); i++) {
        struct fbo_attachment *attachment = &attachments[i];
        if (attachment->is_external)
            continue;
        if (attachment->type == FBO_ATTACHMENT_TYPE_RENDERBUFFER) {
            ngli_glDeleteRenderbuffers(gl, 1, &attachment->id);
        }
    }
    ngli_darray_reset(&fbo->attachments);
    ngli_darray_reset(&fbo->depth_indices);

    memset(fbo, 0, sizeof(*fbo));
}
