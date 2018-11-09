#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include <CoreVideo/CoreVideo.h>

#include "glincludes.h"
#include "hwupload.h"
#include "hwupload_videotoolbox.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"


int ngli_hwupload_vt_get_config_from_frame(struct ngl_node *node,
                                           struct sxplayer_frame *frame,
                                           struct hwupload_config *config)
{
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    OSType cvformat = CVPixelBufferGetPixelFormatType(cvpixbuf);

    config->width = CVPixelBufferGetWidth(cvpixbuf);
    config->height = CVPixelBufferGetHeight(cvpixbuf);
    config->linesize = CVPixelBufferGetBytesPerRow(cvpixbuf);

    switch (cvformat) {
    case kCVPixelFormatType_32BGRA:
        config->format = NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA;
        config->data_format = NGLI_FORMAT_B8G8R8A8_UNORM;
        break;
    case kCVPixelFormatType_32RGBA:
        config->format = NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA;
        config->data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
        break;
#if defined(TARGET_IPHONE)
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
        struct texture *s = node->priv_data;

        if (s->direct_rendering) {
            config->format = NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12_DR;
            config->data_format = NGLI_FORMAT_UNDEFINED;
        } else {
            config->format = NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12;
            config->data_format = NGLI_FORMAT_B8G8R8A8_UNORM;
        }
        break;
    }
#endif
    default:
        ngli_assert(0);
    }

    return 0;
}

#if defined(TARGET_DARWIN)
int ngli_hwupload_vt_init(struct ngl_node *node,
                          struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;

    if (s->upload_fmt == config->format)
        return 0;

    s->upload_fmt = config->format;
    s->data_format = config->data_format;

    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    ngli_mat4_identity(s->coordinates_matrix);

    return 0;
}

int ngli_hwupload_vt_upload(struct ngl_node *node,
                            struct hwupload_config *config,
                            struct sxplayer_frame *frame)
{
    struct texture *s = node->priv_data;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    CVPixelBufferLockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    uint8_t *data = CVPixelBufferGetBaseAddress(cvpixbuf);

    const int linesize       = config->linesize >> 2;
    s->coordinates_matrix[0] = linesize ? config->width / (float)linesize : 1.0;

    ngli_texture_update_local_texture(node, config->linesize >> 2, config->height, 0, data);

    CVPixelBufferUnlockBaseAddress(cvpixbuf, kCVPixelBufferLock_ReadOnly);

    return 0;
}
#elif defined(TARGET_IPHONE)
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

int ngli_hwupload_vt_init(struct ngl_node *node,
                          struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;

    if (config->format == NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12) {
        int ret = ngli_texture_update_local_texture(node, config->width, config->height, 0, NULL);
        if (ret < 0)
            return ret;
    }

    if (s->upload_fmt == config->format)
        return 0;

    s->upload_fmt = config->format;
    s->data_format = config->data_format;

    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    ngli_mat4_identity(s->coordinates_matrix);

    if (s->upload_fmt == NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12) {
        struct texture *t;

        static const float corner[3] = {-1.0, -1.0, 0.0};
        static const float width[3]  = { 2.0,  0.0, 0.0};
        static const float height[3] = { 0.0,  2.0, 0.0};

        s->quad = ngl_node_create(NGL_NODE_QUAD);
        if (!s->quad)
            return -1;

        ngl_node_param_set(s->quad, "corner", corner);
        ngl_node_param_set(s->quad, "width", width);
        ngl_node_param_set(s->quad, "height", height);

        s->program = ngl_node_create(NGL_NODE_PROGRAM);
        if (!s->program)
            return -1;

        ngl_node_param_set(s->program, "name", "vt-read-nv12");

        const char *uv = gl->version < 300 ? "ra": "rg";
        char *fragment_shader = ngli_asprintf(FRAGMENT_SHADER_HWUPLOAD_NV12_DATA, uv);
        if (!fragment_shader)
            return -1;
        ngl_node_param_set(s->program, "fragment", fragment_shader);
        free(fragment_shader);

        s->textures[0] = ngl_node_create(NGL_NODE_TEXTURE2D);
        if (!s->textures[0])
            return -1;

        t = s->textures[0]->priv_data;
        t->data_format     = NGLI_FORMAT_R8_UNORM;
        t->width           = s->width;
        t->height          = s->height;
        t->external_id     = UINT_MAX;
        t->external_target = GL_TEXTURE_2D;

        ret = ngli_format_get_gl_format_type(gl,
                                             t->data_format,
                                             &t->format,
                                             &t->internal_format,
                                             &t->type);
        if (ret < 0)
            return ret;

        s->textures[1] = ngl_node_create(NGL_NODE_TEXTURE2D);
        if (!s->textures[1])
            return -1;

        t = s->textures[1]->priv_data;
        t->data_format     = NGLI_FORMAT_R8G8_UNORM;
        t->width           = (s->width + 1) >> 1;
        t->height          = (s->height + 1) >> 1;
        t->external_id     = UINT_MAX;
        t->external_target = GL_TEXTURE_2D;

        int ret = ngli_format_get_gl_format_type(gl,
                                                 t->data_format,
                                                 &t->format,
                                                 &t->internal_format,
                                                 &t->type);
        if (ret < 0)
            return ret;

        s->target_texture = ngl_node_create(NGL_NODE_TEXTURE2D);
        if (!s->target_texture)
            return -1;

        t = s->target_texture->priv_data;
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
        t->external_id     = s->local_id;
        t->external_target = GL_TEXTURE_2D;

        s->render = ngl_node_create(NGL_NODE_RENDER, s->quad);
        if (!s->render)
            return -1;

        ngl_node_param_set(s->render, "name", "vt-nv12-render");
        ngl_node_param_set(s->render, "program", s->program);
        ngl_node_param_set(s->render, "textures", "tex0", s->textures[0]);
        ngl_node_param_set(s->render, "textures", "tex1", s->textures[1]);

        s->rtt = ngl_node_create(NGL_NODE_RENDERTOTEXTURE, s->render, s->target_texture);
        if (!s->rtt)
            return -1;

        ngli_node_attach_ctx(s->rtt, node->ctx);
    }

    return 0;
}

int ngli_hwupload_vt_upload(struct ngl_node *node,
                            struct hwupload_config *config,
                            struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;

    CVOpenGLESTextureRef textures[2] = {0};
    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;

    switch (s->upload_fmt) {
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA:
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA: {
        s->width                 = config->width;
        s->height                = config->height;
        s->coordinates_matrix[0] = 1.0;

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
            LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
            s->id = s->local_id;
            return -1;
        }

        if (s->ios_textures[0])
            CFRelease(s->ios_textures[0]);

        s->ios_textures[0] = textures[0];
        s->id = CVOpenGLESTextureGetName(s->ios_textures[0]);

        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->id);
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
        break;
    }
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12: {
        s->coordinates_matrix[0] = 1.0;

        int ret = ngli_texture_update_local_texture(node, config->width, config->height, 0, NULL);
        if (ret < 0)
            return ret;

        if (ret) {
            ngli_hwupload_uninit(node);
            ret = init_vt(node, config);
            if (ret < 0)
                return ret;
        }

        for (int i = 0; i < 2; i++) {
            struct texture *t = s->textures[i]->priv_data;

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
                for (int j = 0; j < 2; j++) {
                    if (textures[j])
                        CFRelease(textures[j]);
                }
                return -1;
            }

            t->id = t->external_id = CVOpenGLESTextureGetName(textures[i]);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, t->id);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t->min_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t->mag_filter);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, t->wrap_s);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t->wrap_t);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
        }

        ctx->activitycheck_nodes.count = 0;
        ret = ngli_node_visit(s->rtt, 1, 0.0);
        if (ret < 0) {
            CFRelease(textures[0]);
            CFRelease(textures[1]);
            return ret;
        }

        ret = ngli_node_honor_release_prefetch(&ctx->activitycheck_nodes);
        if (ret < 0) {
            CFRelease(textures[0]);
            CFRelease(textures[1]);
            return ret;
        }

        ret = ngli_node_update(s->rtt, 0.0);
        if (ret < 0) {
            CFRelease(textures[0]);
            CFRelease(textures[1]);
            return ret;
        }

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
        switch (s->min_filter) {
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
    default:
        ngli_assert(0);
    }

    return 0;
}

int ngli_hwupload_vt_dr_init(struct ngl_node *node,
                             struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;

    if (s->upload_fmt == config->format)
        return 0;

    s->upload_fmt = config->format;
    s->data_format = config->data_format;

    int ret = ngli_format_get_gl_format_type(gl,
                                             s->data_format,
                                             &s->format,
                                             &s->internal_format,
                                             &s->type);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_hwupload_vt_dr_upload(struct ngl_node *node,
                               struct hwupload_config *config,
                               struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;

    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;

    s->width                 = config->width;
    s->height                = config->height;
    s->coordinates_matrix[0] = 1.0;

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

        if (s->ios_textures[i])
            CFRelease(s->ios_textures[i]);

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
                                                                    &(s->ios_textures[i]));
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from image: %d", err);
            for (int j = 0; j < 2; j++) {
                if (s->ios_textures[j]) {
                    CFRelease(s->ios_textures[j]);
                    s->ios_textures[j] = NULL;
                }
            }
            return -1;
        }

        GLint id = CVOpenGLESTextureGetName(s->ios_textures[i]);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->min_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->mag_filter);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s->wrap_s);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s->wrap_t);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
    }

    return 0;
}
#endif
