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

#define NGLI_CFRELEASE(ref) do { \
    if (ref) {                   \
        CFRelease(ref);          \
        ref = NULL;              \
    }                            \
} while (0)

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

    if (s->hwupload_fmt == config->format)
        return 0;

    s->hwupload_fmt = config->format;
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

void ngli_hwupload_vt_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    s->hwupload_fmt = NGLI_HWUPLOAD_FMT_NONE;
}
#elif defined(TARGET_IPHONE)
struct hwupload_vt {
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

int ngli_hwupload_vt_init(struct ngl_node *node,
                          struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;

    if (s->hwupload_fmt == config->format)
        return 0;

    struct hwupload_vt *vt = calloc(1, sizeof(*vt));
    if (!vt)
        return -1;
    s->hwupload_fmt = config->format;
    s->hwupload_priv_data = vt;

    ngli_mat4_identity(s->coordinates_matrix);

    if (s->hwupload_fmt == NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12) {
        s->data_format = config->data_format;
        int ret = ngli_format_get_gl_format_type(gl,
                                                 s->data_format,
                                                 &s->format,
                                                 &s->internal_format,
                                                 &s->type);
        if (ret < 0)
            return ret;

        ret = ngli_texture_update_local_texture(node, config->width, config->height, 0, NULL);
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

        ngli_node_attach_ctx(vt->rtt, node->ctx);
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
    struct hwupload_vt *vt = s->hwupload_priv_data;

    CVOpenGLESTextureRef textures[2] = {0};
    CVOpenGLESTextureCacheRef *texture_cache = ngli_glcontext_get_texture_cache(gl);
    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;

    switch (s->hwupload_fmt) {
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
            return -1;
        }

        NGLI_CFRELEASE(vt->ios_textures[0]);

        vt->ios_textures[0] = textures[0];
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
    case NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12: {
        s->coordinates_matrix[0] = 1.0;

        int ret = ngli_texture_update_local_texture(node, config->width, config->height, 0, NULL);
        if (ret < 0)
            return ret;

        if (ret) {
            ngli_hwupload_uninit(node);
            ret = ngli_hwupload_vt_init(node, config);
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

void ngli_hwupload_vt_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    s->hwupload_fmt = NGLI_HWUPLOAD_FMT_NONE;

    struct hwupload_vt *vt = s->hwupload_priv_data;
    if (!vt)
        return;

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

    free(s->hwupload_priv_data);
    s->hwupload_priv_data = NULL;
}


int ngli_hwupload_vt_dr_init(struct ngl_node *node,
                             struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    if (s->hwupload_fmt == config->format)
        return 0;

    struct hwupload_vt *vt = calloc(1, sizeof(*vt));
    if (!vt)
        return -1;
    s->hwupload_fmt = config->format;
    s->hwupload_priv_data = vt;

    s->layout = NGLI_TEXTURE_LAYOUT_NV12;
    for (int i = 0; i < 2; i++) {
        s->planes[i].id = 0;
        s->planes[i].target = GL_TEXTURE_2D;
    }

    return 0;
}

int ngli_hwupload_vt_dr_upload(struct ngl_node *node,
                               struct hwupload_config *config,
                               struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;
    struct hwupload_vt *vt = s->hwupload_priv_data;

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

    return 0;
}

void ngli_hwupload_vt_dr_uninit(struct ngl_node *node)
{
    struct texture *s = node->priv_data;
    s->hwupload_fmt = NGLI_HWUPLOAD_FMT_NONE;

    struct hwupload_vt *vt = s->hwupload_priv_data;
    if (!vt)
        return;

    NGLI_CFRELEASE(vt->ios_textures[0]);
    NGLI_CFRELEASE(vt->ios_textures[1]);

    free(s->hwupload_priv_data);
    s->hwupload_priv_data = NULL;
}
#endif
