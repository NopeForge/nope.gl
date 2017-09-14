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
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#if defined(TARGET_ANDROID)
#include <libavcodec/mediacodec.h>
#include "android_surface.h"
#endif

#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

enum {
    HWUPLOAD_FMT_NONE,
    HWUPLOAD_FMT_COMMON,
    HWUPLOAD_FMT_MEDIACODEC,
    HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA,
    HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA,
    HWUPLOAD_FMT_VIDEOTOOLBOX_NV12,
};

struct hwupload_config {
    int format;
    int width;
    int height;
    int linesize;
    float xscale;
    GLint gl_format;
    GLint gl_internal_format;
    GLint gl_type;
};

static int get_config_from_frame(struct sxplayer_frame *frame, struct hwupload_config *config)
{
    if (!frame)
        return -1;

    config->width = frame->width;
    config->height = frame->height;
    config->linesize = frame->linesize;
    config->xscale = config->width ? 1.0 : (config->linesize >> 2) / (float)config->width;

    switch (frame->pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
        config->format = HWUPLOAD_FMT_COMMON;
        config->gl_format = GL_RGBA;
        config->gl_internal_format = GL_RGBA;
        config->gl_type = GL_UNSIGNED_BYTE;
        break;
    case SXPLAYER_PIXFMT_BGRA:
        config->format = HWUPLOAD_FMT_COMMON;
        config->gl_format = GL_BGRA;
        config->gl_internal_format = GL_RGBA;
        config->gl_type = GL_UNSIGNED_BYTE;
        break;
    case SXPLAYER_SMPFMT_FLT:
        config->format = HWUPLOAD_FMT_COMMON;
        config->gl_format = GL_RED;
        config->gl_internal_format = GL_R32F;
        config->gl_type = GL_FLOAT;
        break;
#if defined(TARGET_ANDROID)
    case SXPLAYER_PIXFMT_MEDIACODEC: {
        config->format = HWUPLOAD_FMT_MEDIACODEC;
        break;
    }
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case SXPLAYER_PIXFMT_VT: {
        CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
        OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

        config->width = CVPixelBufferGetWidth(cvpixbuf);
        config->height = CVPixelBufferGetHeight(cvpixbuf);
        config->linesize = CVPixelBufferGetBytesPerRow(cvpixbuf);
        if (config->width)
            config->xscale = (config->linesize >> 2) / (float)config->width;

        switch (cvformat) {
        case kCVPixelFormatType_32BGRA:
            config->format = HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA;
            config->gl_format = GL_BGRA;
            break;
        case kCVPixelFormatType_32RGBA:
            config->format = HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA;
            config->gl_format = GL_RGBA;
            break;
#if defined(TARGET_IPHONE)
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            config->format = HWUPLOAD_FMT_VIDEOTOOLBOX_NV12;
            config->gl_format = GL_BGRA;
            break;
#endif
        default:
            ngli_assert(0);
        };
        config->gl_internal_format = GL_RGBA;
        config->gl_type = GL_UNSIGNED_BYTE;
        break;
    }
#endif
    default:
        ngli_assert(0);
    }

    return 0;
}

static int init_common(struct ngl_node *node, struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    s->upload_fmt = config->format;

    return 0;
}

static int upload_common_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    int dimension_changed = s->width != (config->linesize >> 2) || s->height != config->height;

    s->id                    = s->local_id;
    s->target                = s->local_target;
    s->format                = config->gl_format;
    s->internal_format       = config->gl_internal_format;
    s->type                  = config->gl_type;
    s->width                 = config->linesize >> 2;
    s->height                = config->height;
    s->coordinates_matrix[0] = config->xscale;

    ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
    if (dimension_changed)
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, frame->data);
    else
        ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, 0, s->width, s->height, s->format, s->type, frame->data);

    switch(s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        break;
    }
    ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

    return 0;
}

#if defined(TARGET_ANDROID) || defined(TARGET_IPHONE)
static int update_texture_dimensions(struct ngl_node *node, struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    if (s->width != config->width || s->height != config->height) {
        s->width = config->width;
        s->height = config->height;

        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, NULL);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
    }

    return 0;
}
#endif

#if defined(TARGET_ANDROID)
static const char fragment_shader_hwupload_oes_data[] = ""
    "#version 100"                                                                      "\n"
    "#extension GL_OES_EGL_image_external : require"                                    "\n"
    ""                                                                                  "\n"
    "precision mediump float;"                                                          "\n"
    "uniform samplerExternalOES tex0_sampler;"                                          "\n"
    "varying vec2 var_tex0_coords;"                                                     "\n"
    "void main(void)"                                                                   "\n"
    "{"                                                                                 "\n"
    "    vec4 t;"                                                                       "\n"
    "    t  = texture2D(tex0_sampler, var_tex0_coords);"                                "\n"
    "    gl_FragColor = vec4(t.rgb, 1.0);"                                              "\n"
    "}";

static int init_mc(struct ngl_node *node, struct hwupload_config *config)
{
    int ret;

    struct texture *s = node->priv_data, *t;
    struct media *media = s->data_src->priv_data;

    static const float corner[3] = { -1.0, -1.0, 0.0 };
    static const float width[3]  = {  2.0,  0.0, 0.0 };
    static const float height[3] = {  0.0,  2.0, 0.0 };

    if (s->upload_fmt == config->format)
        return 0;

    s->upload_fmt = config->format;

    ret = update_texture_dimensions(node, config);
    if (ret < 0)
        return ret;

    s->quad = ngl_node_create(NGL_NODE_QUAD);
    if (!s->quad)
        return -1;

    ngl_node_param_set(s->quad, "corner", corner);
    ngl_node_param_set(s->quad, "width", width);
    ngl_node_param_set(s->quad, "height", height);

    s->shader = ngl_node_create(NGL_NODE_SHADER);
    if (!s->shader)
        return -1;

    ngl_node_param_set(s->shader, "fragment_data", fragment_shader_hwupload_oes_data);

    s->textures[0] = ngl_node_create(NGL_NODE_TEXTURE);
    if (!s->textures[0])
        return -1;

    t = s->textures[0]->priv_data;
    t->target      = GL_TEXTURE_EXTERNAL_OES;
    t->width       = s->width;
    t->height      = s->height;
    t->external_id = media->android_texture_id;

    s->target_texture = ngl_node_create(NGL_NODE_TEXTURE);
    if (!s->target_texture)
        return -1;

    t = s->target_texture->priv_data;
    t->target          = s->target;
    t->format          = s->format;
    t->internal_format = s->internal_format;
    t->width           = s->width;
    t->height          = s->height;
    t->min_filter      = s->min_filter;
    t->mag_filter      = s->mag_filter;
    t->wrap_s          = s->wrap_s;
    t->wrap_t          = s->wrap_t;
    t->external_id     = s->local_id;

    s->tshape = ngl_node_create(NGL_NODE_TEXTUREDSHAPE, s->quad, s->shader);
    if (!s->tshape)
        return -1;

    ngl_node_param_set(s->tshape, "textures", "tex0", s->textures[0]);

    s->rtt = ngl_node_create(NGL_NODE_RTT, s->tshape, s->target_texture);
    if (!s->rtt)
        return -1;

    ngli_node_attach_ctx(s->rtt, node->ctx);
    ngli_node_init(s->rtt);

    return 0;
}

static int upload_mc_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    int ret;

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

    ret = update_texture_dimensions(node, config);
    if (ret < 0)
        return ret;

    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);

    struct texture *t = s->textures[0]->priv_data;
    ngli_mat4_mul(t->coordinates_matrix, flip_matrix, matrix);

    ngli_node_update(s->rtt, 0.0);
    ngli_node_draw(s->rtt);

    t = s->target_texture->priv_data;
    memcpy(s->coordinates_matrix, t->coordinates_matrix, sizeof(s->coordinates_matrix));

    return 0;
}
#endif

#if defined(TARGET_DARWIN)
static int init_vt(struct ngl_node *node, struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    s->upload_fmt = config->format;

    return 0;
}

static int upload_vt_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    CVPixelBufferLockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    uint8_t *data = CVPixelBufferGetBaseAddress(cvpixbuf);

    int dimension_changed = s->width != (config->linesize >> 2) || s->height != config->height;

    s->format                = config->gl_format;
    s->internal_format       = config->gl_internal_format;
    s->type                  = config->gl_type;
    s->width                 = config->linesize >> 2;
    s->height                = config->height;
    s->coordinates_matrix[0] = config->xscale;

    ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
    if (dimension_changed)
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s->internal_format, s->width, s->height, 0, s->format, s->type, data);
    else
        ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, 0, s->width, s->height, s->format, s->type, data);

    CVPixelBufferUnlockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    switch(s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        break;
    }
    ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

    return 0;
}
#endif

#if defined(TARGET_IPHONE)
const char fragment_shader_hwupload_nv12_data[] =
    "precision mediump float;"                                                     "\n"
    "uniform sampler2D tex0_sampler;"                                              "\n"
    "uniform sampler2D tex1_sampler;"                                              "\n"
    "varying vec2 var_tex0_coords;"                                                "\n"
    "const mat3 conv = mat3("                                                      "\n"
    "     1.0,      1.0,     1.0,"                                                 "\n"
    "     0.0,     -0.21482, 2.12798,"                                             "\n"
    "     1.28033, -0.38059, 0.0);"                                                "\n"
    "void main(void)"                                                              "\n"
    "{"                                                                            "\n"
    "    vec3 yuv;"                                                                "\n"
    "    vec3 rgb;"                                                                "\n"
    "    yuv.x = texture2D(tex0_sampler, var_tex0_coords).r;"                      "\n"
    "    yuv.yz = texture2D(tex1_sampler, var_tex0_coords).ra - vec2(0.5, 0.5);"   "\n"
    "    rgb = conv * yuv;"                                                        "\n"
    "    gl_FragColor = vec4(rgb, 1);"                                             "\n"
    "}";

static int init_vt(struct ngl_node *node, struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    if (config->format == HWUPLOAD_FMT_VIDEOTOOLBOX_NV12)
        update_texture_dimensions(node, config);

    if (s->upload_fmt == config->format)
        return 0;

    s->upload_fmt = config->format;

    if (s->upload_fmt == HWUPLOAD_FMT_VIDEOTOOLBOX_NV12) {
        struct texture *t;

        static const float corner[3] = { -1.0, -1.0, 0.0 };
        static const float width[3]  = {  2.0,  0.0, 0.0 };
        static const float height[3] = {  0.0,  2.0, 0.0 };

        s->quad = ngl_node_create(NGL_NODE_QUAD);
        if (!s->quad)
            return -1;

        ngl_node_param_set(s->quad, "corner", corner);
        ngl_node_param_set(s->quad, "width", width);
        ngl_node_param_set(s->quad, "height", height);

        s->shader = ngl_node_create(NGL_NODE_SHADER);
        if (!s->shader)
            return -1;

        ngl_node_param_set(s->shader, "fragment_data", fragment_shader_hwupload_nv12_data);

        s->textures[0] = ngl_node_create(NGL_NODE_TEXTURE);
        if (!s->textures[0])
            return -1;

        t = s->textures[0]->priv_data;
        t->target          = GL_TEXTURE_2D;
        s->format          = GL_LUMINANCE;
        s->internal_format = GL_LUMINANCE;
        s->type            = GL_UNSIGNED_BYTE;
        t->width           = s->width;
        t->height          = s->height;
        t->external_id     = UINT_MAX;

        s->textures[1] = ngl_node_create(NGL_NODE_TEXTURE);
        if (!s->textures[1])
            return -1;

        t = s->textures[1]->priv_data;
        t->target          = GL_TEXTURE_2D;
        s->format          = GL_LUMINANCE_ALPHA;
        s->internal_format = GL_LUMINANCE_ALPHA;
        s->type            = GL_UNSIGNED_BYTE;
        t->width           = (s->width + 1) >> 1;
        t->height          = (s->height + 1) >> 1;
        t->external_id     = UINT_MAX;

        s->target_texture = ngl_node_create(NGL_NODE_TEXTURE);
        if (!s->target_texture)
            return -1;

        t = s->target_texture->priv_data;
        t->target          = s->target;
        t->format          = s->format;
        t->internal_format = s->internal_format;
        t->width           = s->width;
        t->height          = s->height;
        t->min_filter      = s->min_filter;
        t->mag_filter      = s->mag_filter;
        t->wrap_s          = s->wrap_s;
        t->wrap_t          = s->wrap_t;
        t->external_id     = s->local_id;

        s->tshape = ngl_node_create(NGL_NODE_TEXTUREDSHAPE, s->quad, s->shader);
        if (!s->tshape)
            return -1;

        ngl_node_param_set(s->tshape, "textures", "tex0", s->textures[0]);
        ngl_node_param_set(s->tshape, "textures", "tex1", s->textures[1]);

        s->rtt = ngl_node_create(NGL_NODE_RTT, s->tshape, s->target_texture);
        if (!s->rtt)
            return -1;

        ngli_node_attach_ctx(s->rtt, node->ctx);
        ngli_node_init(s->rtt);
    }

    return 0;
}

static int upload_vt_frame(struct ngl_node *node, struct hwupload_config *config, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    struct texture *s = node->priv_data;

    CVOpenGLESTextureRef textures[2] = { 0 };
    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(glcontext);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;

    switch (s->upload_fmt) {
    case HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA: {
        s->format                = config->gl_format;
        s->internal_format       = config->gl_internal_format;
        s->type                  = config->gl_type;
        s->width                 = config->linesize >> 2;
        s->height                = config->height;
        s->coordinates_matrix[0] = config->xscale;

        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    *texture_cache,
                                                                    cvpixbuf,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    s->internal_format,
                                                                    s->width,
                                                                    s->height,
                                                                    s->format,
                                                                    s->type,
                                                                    0,
                                                                    &textures[0]);
        if (err != noErr) {
            LOG(ERROR, "Could not create CoreVideo texture from image: %d", err);
            s->id = s->local_id;
            return -1;
        }

        if (s->texture)
            CFRelease(s->texture);

        s->texture = textures[0];
        s->id = CVOpenGLESTextureGetName(s->texture);

        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
        switch(s->min_filter) {
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
            break;
        }
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        break;
    }
    case HWUPLOAD_FMT_VIDEOTOOLBOX_NV12: {
        s->format                = config->gl_format;
        s->internal_format       = config->gl_internal_format;
        s->type                  = config->gl_type;
        s->width                 = config->width;
        s->height                = config->height;
        s->coordinates_matrix[0] = 1.0;

        update_texture_dimensions(node, config);

        for (int i = 0; i < 2; i++) {
            int width;
            int height;
            GLenum format;
            GLenum internal_format;
            GLenum type = GL_UNSIGNED_BYTE;

            switch (i) {
            case 0:
                width = s->width;
                height = s->height;
                format = GL_LUMINANCE;
                internal_format = GL_LUMINANCE;
                break;
            case 1:
                width = (s->width + 1) >> 1;
                height = (s->height + 1) >> 1;
                format = GL_LUMINANCE_ALPHA;
                internal_format = GL_LUMINANCE_ALPHA;
                break;
            default:
                ngli_assert(0);
            }

            CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                        *texture_cache,
                                                                        cvpixbuf,
                                                                        NULL,
                                                                        GL_TEXTURE_2D,
                                                                        internal_format,
                                                                        width,
                                                                        height,
                                                                        format,
                                                                        type,
                                                                        i,
                                                                        &textures[i]);
            if (err != noErr) {
                LOG(ERROR, "Could not create CoreVideo texture from image: %d", err);
                for (int j = 0; j < 2; j++) {
                    if (textures[j])
                        CFRelease(textures[j]);
                }
                return -1;
            }

            struct texture *t = s->textures[i]->priv_data;

            t->id = t->external_id = CVOpenGLESTextureGetName(textures[i]);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, t->id);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t->min_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t->mag_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, t->wrap_s);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t->wrap_t);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        }

        ngli_node_update(s->rtt, 0.0);
        ngli_node_draw(s->rtt);

        CFRelease(textures[0]);
        CFRelease(textures[1]);

        struct texture *t = s->target_texture->priv_data;
        memcpy(s->coordinates_matrix, t->coordinates_matrix, sizeof(s->coordinates_matrix));

        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
        switch(s->min_filter) {
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
            break;
        }
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        break;
    }
    }

    return 0;
}
#endif

static int hwupload_init(struct ngl_node *node, struct hwupload_config *config)
{
    int ret = 0;

    switch (config->format) {
    case HWUPLOAD_FMT_COMMON:
        ret = init_common(node, config);
        break;
#if defined(TARGET_ANDROID)
    case HWUPLOAD_FMT_MEDIACODEC:
        ret = init_mc(node, config);
        break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
    case HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA:
    case HWUPLOAD_FMT_VIDEOTOOLBOX_NV12:
        ret = init_vt(node, config);
        break;
#endif
    }

    return ret;
}

int ngli_hwupload_upload_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    int ret;
    struct hwupload_config config = { 0 };

    ret = get_config_from_frame(frame, &config);
    if (ret < 0)
        return ret;

    ret = hwupload_init(node, &config);
    if (ret < 0)
        return ret;

    if (frame) {
        switch(frame->pix_fmt) {
        case SXPLAYER_PIXFMT_BGRA:
        case SXPLAYER_PIXFMT_RGBA:
        case SXPLAYER_SMPFMT_FLT:
            ret = upload_common_frame(node, &config, frame);
            break;
#if defined(TARGET_ANDROID)
        case SXPLAYER_PIXFMT_MEDIACODEC:
            ret = upload_mc_frame(node, &config, frame);
            break;
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
        case SXPLAYER_PIXFMT_VT:
            ret = upload_vt_frame(node, &config, frame);
            break;
#endif
        default:
            ngli_assert(0);
        }
    }

    return ret;
}

void ngli_hwupload_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;

    s->upload_fmt = HWUPLOAD_FMT_NONE;

    if (s->rtt)
        ngli_node_detach_ctx(s->rtt);

    ngl_node_unrefp(&s->quad);
    ngl_node_unrefp(&s->shader);
    ngl_node_unrefp(&s->tshape);
    ngl_node_unrefp(&s->textures[0]);
    ngl_node_unrefp(&s->textures[1]);
    ngl_node_unrefp(&s->textures[2]);
    ngl_node_unrefp(&s->target_texture);
    ngl_node_unrefp(&s->rtt);

#if defined(TARGET_IPHONE)
    if (s->texture)
        CFRelease(s->texture);
#endif
}
