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

#include <CoreVideo/CoreVideo.h>

#include "glincludes.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

#define NGLI_CFRELEASE(ref) do { \
    if (ref) {                   \
        CFRelease(ref);          \
        ref = NULL;              \
    }                            \
} while (0)

struct hwupload_vt_ios {
    struct ngl_node *quad;
    struct ngl_node *program;
    struct ngl_node *render;
    struct ngl_node *textures[2];
    struct ngl_node *target_texture;
    struct ngl_node *rtt;
    CVOpenGLESTextureRef ios_textures[2];
};

#define FRAGMENT_SHADER_HWUPLOAD_NV12_DATA                                              \
    "#version 100"                                                                 "\n" \
    ""                                                                             "\n" \
    "precision mediump float;"                                                     "\n" \
    "uniform sampler2D tex0_sampler;"                                              "\n" \
    "uniform sampler2D tex1_sampler;"                                              "\n" \
    "varying vec2 var_tex0_coord;"                                                 "\n" \
    "const mat4 conv = mat4("                                                      "\n" \
    "    1.164,     1.164,    1.164,   0.0,"                                       "\n" \
    "    0.0,      -0.213,    2.112,   0.0,"                                       "\n" \
    "    1.787,    -0.531,    0.0,     0.0,"                                       "\n" \
    "   -0.96625,   0.29925, -1.12875, 1.0);"                                      "\n" \
    "void main(void)"                                                              "\n" \
    "{"                                                                            "\n" \
    "    vec3 yuv;"                                                                "\n" \
    "    yuv.x = texture2D(tex0_sampler, var_tex0_coord).r;"                       "\n" \
    "    yuv.yz = texture2D(tex1_sampler, var_tex0_coord).%s;"                     "\n" \
    "    gl_FragColor = conv * vec4(yuv, 1.0);"                                    "\n" \
    "}"

static int vt_ios_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    ngli_assert(cvformat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);

    s->data_format = NGLI_FORMAT_B8G8R8A8_UNORM;
    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    int frame_width = CVPixelBufferGetWidth(cvpixbuf);
    int frame_height = CVPixelBufferGetHeight(cvpixbuf);

    ret = ngli_texture_update_data(node, frame_width, frame_height, 0, NULL);
    if (ret < 0)
        return ret;

    static const float corner[3] = {-1.0, -1.0, 0.0};
    static const float width[3]  = { 2.0,  0.0, 0.0};
    static const float height[3] = { 0.0,  2.0, 0.0};

    vt->quad = ngl_node_create(NGL_NODE_QUAD);
    if (!vt->quad)
        return -1;

    ngl_node_param_set(vt->quad, "corner", corner);
    ngl_node_param_set(vt->quad, "width", width);
    ngl_node_param_set(vt->quad, "height", height);

    vt->program = ngl_node_create(NGL_NODE_PROGRAM);
    if (!vt->program)
        return -1;

    ngl_node_param_set(vt->program, "name", "vt-read-nv12");

    const char *uv = gl->version < 300 ? "ra": "rg";
    char *fragment_shader = ngli_asprintf(FRAGMENT_SHADER_HWUPLOAD_NV12_DATA, uv);
    if (!fragment_shader)
        return -1;
    ngl_node_param_set(vt->program, "fragment", fragment_shader);
    free(fragment_shader);

    vt->textures[0] = ngl_node_create(NGL_NODE_TEXTURE2D);
    if (!vt->textures[0])
        return -1;

    struct texture *t = vt->textures[0]->priv_data;
    t->externally_managed = 1;
    t->data_format     = NGLI_FORMAT_R8_UNORM;
    t->width           = s->width;
    t->height          = s->height;
    ngli_mat4_identity(t->coordinates_matrix);

    t->layout = NGLI_TEXTURE_LAYOUT_DEFAULT;
    t->planes[0].id = 0;
    t->planes[0].target = GL_TEXTURE_2D;

    ret = ngli_format_get_gl_format_type(gl,
                                         t->data_format,
                                         &t->format,
                                         &t->internal_format,
                                         &t->type);
    if (ret < 0)
        return ret;

    vt->textures[1] = ngl_node_create(NGL_NODE_TEXTURE2D);
    if (!vt->textures[1])
        return -1;

    t = vt->textures[1]->priv_data;
    t->externally_managed = 1;
    t->data_format     = NGLI_FORMAT_R8G8_UNORM;
    t->width           = (s->width + 1) >> 1;
    t->height          = (s->height + 1) >> 1;
    ngli_mat4_identity(t->coordinates_matrix);

    t->layout = NGLI_TEXTURE_LAYOUT_DEFAULT;
    t->planes[0].id = 0;
    t->planes[0].target = GL_TEXTURE_2D;

    ret = ngli_format_get_gl_format_type(gl,
                                         t->data_format,
                                         &t->format,
                                         &t->internal_format,
                                         &t->type);
    if (ret < 0)
        return ret;

    vt->target_texture = ngl_node_create(NGL_NODE_TEXTURE2D);
    if (!vt->target_texture)
        return -1;

    t = vt->target_texture->priv_data;
    t->externally_managed = 1;
    t->data_format     = s->data_format;
    t->format          = s->format;
    t->internal_format = s->internal_format;
    t->type            = s->type;
    t->width           = s->width;
    t->height          = s->height;
    t->min_filter      = s->min_filter;
    t->mag_filter      = s->mag_filter;
    t->wrap_s          = s->wrap_s;
    t->wrap_t          = s->wrap_t;
    t->id              = s->id;
    t->target          = GL_TEXTURE_2D;
    ngli_mat4_identity(t->coordinates_matrix);

    vt->render = ngl_node_create(NGL_NODE_RENDER, vt->quad);
    if (!vt->render)
        return -1;

    ngl_node_param_set(vt->render, "name", "vt-nv12-render");
    ngl_node_param_set(vt->render, "program", vt->program);
    ngl_node_param_set(vt->render, "textures", "tex0", vt->textures[0]);
    ngl_node_param_set(vt->render, "textures", "tex1", vt->textures[1]);

    vt->rtt = ngl_node_create(NGL_NODE_RENDERTOTEXTURE, vt->render, vt->target_texture);
    if (!vt->rtt)
        return -1;

    ret = ngli_node_attach_ctx(vt->rtt, node->ctx);
    if (ret < 0)
        return ret;

    return 0;
}

static void vt_ios_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    if (vt->rtt)
        ngli_node_detach_ctx(vt->rtt);

    ngl_node_unrefp(&vt->quad);
    ngl_node_unrefp(&vt->program);
    ngl_node_unrefp(&vt->render);
    ngl_node_unrefp(&vt->textures[0]);
    ngl_node_unrefp(&vt->textures[1]);
    ngl_node_unrefp(&vt->target_texture);
    ngl_node_unrefp(&vt->rtt);

    NGLI_CFRELEASE(vt->ios_textures[0]);
    NGLI_CFRELEASE(vt->ios_textures[1]);
}

static int vt_ios_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVOpenGLESTextureRef textures[2] = {0};
    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    ngli_assert(cvformat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);

    int width                = CVPixelBufferGetWidth(cvpixbuf);
    int height               = CVPixelBufferGetHeight(cvpixbuf);
    s->coordinates_matrix[0] = 1.0;

    int ret = ngli_texture_update_data(node, width, height, 0, NULL);
    if (ret < 0)
        return ret;

    if (ret) {
        vt_ios_uninit(node);
        ret = vt_ios_init(node, frame);
        if (ret < 0)
            return ret;
    }

    for (int i = 0; i < 2; i++) {
        struct texture *t = vt->textures[i]->priv_data;

        switch (i) {
        case 0:
            t->width = s->width;
            t->height = s->height;
            break;
        case 1:
            t->width = (s->width + 1) >> 1;
            t->height = (s->height + 1) >> 1;
            break;
        default:
            ngli_assert(0);
        }

        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    *texture_cache,
                                                                    cvpixbuf,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    t->internal_format,
                                                                    t->width,
                                                                    t->height,
                                                                    t->format,
                                                                    t->type,
                                                                    i,
                                                                    &textures[i]);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
            NGLI_CFRELEASE(textures[0]);
            NGLI_CFRELEASE(textures[1]);
            return -1;
        }

        GLint id = CVOpenGLESTextureGetName(textures[i]);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t->min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t->mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, t->wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t->wrap_t);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

        t->planes[0].id = id;
        t->planes[0].target = GL_TEXTURE_2D;
    }

    ctx->activitycheck_nodes.count = 0;
    ret = ngli_node_visit(vt->rtt, 1, 0.0);
    if (ret < 0) {
        NGLI_CFRELEASE(textures[0]);
        NGLI_CFRELEASE(textures[1]);
        return ret;
    }

    ret = ngli_node_honor_release_prefetch(&ctx->activitycheck_nodes);
    if (ret < 0) {
        NGLI_CFRELEASE(textures[0]);
        NGLI_CFRELEASE(textures[1]);
        return ret;
    }

    ret = ngli_node_update(vt->rtt, 0.0);
    if (ret < 0) {
        NGLI_CFRELEASE(textures[0]);
        NGLI_CFRELEASE(textures[1]);
        return ret;
    }

    ngli_node_draw(vt->rtt);

    NGLI_CFRELEASE(textures[0]);
    NGLI_CFRELEASE(textures[1]);

    struct texture *t = vt->target_texture->priv_data;
    memcpy(s->coordinates_matrix, t->coordinates_matrix, sizeof(s->coordinates_matrix));

    switch (s->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        break;
    }

    return 0;
}

static int vt_ios_dr_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32RGBA:
        s->layout = NGLI_TEXTURE_LAYOUT_DEFAULT;
        s->planes[0].id = 0;
        s->planes[0].target = GL_TEXTURE_2D;
        break;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        s->layout = NGLI_TEXTURE_LAYOUT_NV12;
        for (int i = 0; i < 2; i++) {
            s->planes[i].id = 0;
            s->planes[i].target = GL_TEXTURE_2D;
        }
        break;
    default:
        return -1;
    }

    return 0;
}

static int vt_ios_dr_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    s->width                 = CVPixelBufferGetWidth(cvpixbuf);
    s->height                = CVPixelBufferGetHeight(cvpixbuf);
    s->coordinates_matrix[0] = 1.0;

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32RGBA: {
        int data_format;
        GLint format;
        GLint internal_format;
        GLenum type;

        switch (cvformat) {
        case kCVPixelFormatType_32BGRA:
            data_format = NGLI_FORMAT_B8G8R8A8_UNORM;
            break;
        case kCVPixelFormatType_32RGBA:
            data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
            break;
        default:
            ngli_assert(0);
        }

        int ret = ngli_format_get_gl_format_type(gl, data_format, &format, &internal_format, &type);
        if (ret < 0)
            return ret;

        NGLI_CFRELEASE(vt->ios_textures[0]);

        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    *texture_cache,
                                                                    cvpixbuf,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    internal_format,
                                                                    s->width,
                                                                    s->height,
                                                                    format,
                                                                    type,
                                                                    0,
                                                                    &vt->ios_textures[0]);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
            return -1;
        }

        GLint id = CVOpenGLESTextureGetName(vt->ios_textures[0]);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
        switch (s->min_filter) {
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
            break;
        }
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

        s->layout = NGLI_TEXTURE_LAYOUT_DEFAULT;
        s->planes[0].id = id;
        s->planes[0].target = GL_TEXTURE_2D;
        break;
    }
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
        for (int i = 0; i < 2; i++) {
            int width;
            int height;
            int data_format;
            GLint format;
            GLint internal_format;
            GLenum type;

            switch (i) {
            case 0:
                width = s->width;
                height = s->height;
                data_format = NGLI_FORMAT_R8_UNORM;
                break;
            case 1:
                width = (s->width + 1) >> 1;
                height = (s->height + 1) >> 1;
                data_format = NGLI_FORMAT_R8G8_UNORM;
                break;
            default:
                ngli_assert(0);
            }

            int ret = ngli_format_get_gl_format_type(gl, data_format, &format, &internal_format, &type);
            if (ret < 0)
                return ret;

            NGLI_CFRELEASE(vt->ios_textures[i]);

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
                                                                        &(vt->ios_textures[i]));
            if (err != noErr) {
                LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
                NGLI_CFRELEASE(vt->ios_textures[0]);
                NGLI_CFRELEASE(vt->ios_textures[1]);
                return -1;
            }

            GLint id = CVOpenGLESTextureGetName(vt->ios_textures[i]);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

            s->planes[i].id = id;
            s->planes[i].target = GL_TEXTURE_2D;
        }
        break;
        }
    default:
        ngli_assert(0);
    }

    return 0;
}

static void vt_ios_dr_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    struct hwupload_vt_ios *vt = s->hwupload_priv_data;

    NGLI_CFRELEASE(vt->ios_textures[0]);
    NGLI_CFRELEASE(vt->ios_textures[1]);
}

static const struct hwmap_class hwmap_vt_ios_class = {
    .name      = "videotoolbox (nv12 → rgba)",
    .priv_size = sizeof(struct hwupload_vt_ios),
    .init      = vt_ios_init,
    .map_frame = vt_ios_map_frame,
    .uninit    = vt_ios_uninit,
};

static const struct hwmap_class hwmap_vt_ios_dr_class = {
    .name      = "videotoolbox (zero-copy)",
    .priv_size = sizeof(struct hwupload_vt_ios),
    .init      = vt_ios_dr_init,
    .map_frame = vt_ios_dr_map_frame,
    .uninit    = vt_ios_dr_uninit,
};

static const struct hwmap_class *vt_ios_get_hwmap(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
        return &hwmap_vt_ios_dr_class;
    case kCVPixelFormatType_32RGBA:
        return &hwmap_vt_ios_dr_class;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        if (s->direct_rendering)
            return &hwmap_vt_ios_dr_class;
        else
            return &hwmap_vt_ios_class;
    default:
        return NULL;
    }
}

const struct hwupload_class ngli_hwupload_vt_ios_class = {
    .get_hwmap = vt_ios_get_hwmap,
};
