#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include <libavcodec/mediacodec.h>

#include "android_surface.h"
#include "glincludes.h"
#include "hwupload.h"
#include "hwupload_mediacodec.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

int ngli_hwupload_mc_get_config_from_frame(struct ngl_node *node,
                                           struct sxplayer_frame *frame,
                                           struct hwupload_config *config)
{
    struct texture *s = node->priv_data;

    config->width = frame->width;
    config->height = frame->height;
    config->linesize = frame->linesize;

    if (s->direct_rendering) {
        if (s->min_filter != GL_NEAREST && s->min_filter != GL_LINEAR) {
            LOG(WARNING,
                "External textures only support nearest and linear filtering: disabling direct rendering");
            s->direct_rendering = 0;
        } else if (s->wrap_s != GL_CLAMP_TO_EDGE || s->wrap_t != GL_CLAMP_TO_EDGE) {
            LOG(WARNING,
                "External textures only support clamp to edge wrapping: disabling direct rendering");
            s->direct_rendering = 0;
        }
    }

    if (s->direct_rendering) {
        config->format = NGLI_HWUPLOAD_FMT_MEDIACODEC_DR;
        config->data_format = NGLI_FORMAT_UNDEFINED;
    } else {
        config->format = NGLI_HWUPLOAD_FMT_MEDIACODEC;
        config->data_format = NGLI_FORMAT_R8G8B8A8_UNORM;
    }

    return 0;
}

static const char fragment_shader_hwupload_oes_data[] = ""
    "#version 100"                                                                      "\n"
    "#extension GL_OES_EGL_image_external : require"                                    "\n"
    ""                                                                                  "\n"
    "precision mediump float;"                                                          "\n"
    "uniform samplerExternalOES tex0_external_sampler;"                                 "\n"
    "varying vec2 var_tex0_coord;"                                                      "\n"
    "void main(void)"                                                                   "\n"
    "{"                                                                                 "\n"
    "    vec4 t = texture2D(tex0_external_sampler, var_tex0_coord);"                    "\n"
    "    gl_FragColor = vec4(t.rgb, 1.0);"                                              "\n"
    "}";

int ngli_hwupload_mc_init(struct ngl_node *node,
                          struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;

    static const float corner[3] = {-1.0, -1.0, 0.0};
    static const float width[3]  = { 2.0,  0.0, 0.0};
    static const float height[3] = { 0.0,  2.0, 0.0};

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

    ret = ngli_texture_update_local_texture(node, config->width, config->height, 0, NULL);
    if (ret < 0)
        return ret;

    s->quad = ngl_node_create(NGL_NODE_QUAD);
    if (!s->quad)
        return -1;

    ngl_node_param_set(s->quad, "corner", corner);
    ngl_node_param_set(s->quad, "width", width);
    ngl_node_param_set(s->quad, "height", height);

    s->program = ngl_node_create(NGL_NODE_PROGRAM);
    if (!s->program)
        return -1;

    ngl_node_param_set(s->program, "name", "mc-read-oes");
    ngl_node_param_set(s->program, "fragment", fragment_shader_hwupload_oes_data);

    s->textures[0] = ngl_node_create(NGL_NODE_TEXTURE2D);
    if (!s->textures[0])
        return -1;

    struct texture *t = s->textures[0]->priv_data;
    t->data_format = NGLI_FORMAT_UNDEFINED;
    t->width       = s->width;
    t->height      = s->height;
    t->external_id = media->android_texture_id;
    t->external_target = GL_TEXTURE_EXTERNAL_OES;

    ret = ngli_format_get_gl_format_type(gl,
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
    t->external_target = s->local_target;

    s->render = ngl_node_create(NGL_NODE_RENDER, s->quad);
    if (!s->render)
        return -1;

    ngl_node_param_set(s->render, "name", "mc-rtt-render");
    ngl_node_param_set(s->render, "program", s->program);
    ngl_node_param_set(s->render, "textures", "tex0", s->textures[0]);

    s->rtt = ngl_node_create(NGL_NODE_RENDERTOTEXTURE, s->render, s->target_texture);
    if (!s->rtt)
        return -1;

    ngli_node_attach_ctx(s->rtt, node->ctx);

    return 0;
}

int ngli_hwupload_mc_upload(struct ngl_node *node,
                            struct hwupload_config *config,
                            struct sxplayer_frame *frame)
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

    ret = ngli_texture_update_local_texture(node, config->width, config->height, 0, NULL);
    if (ret < 0)
        return ret;

    if (ret) {
        ngli_hwupload_uninit(node);
        ret = ngli_hwupload_mc_init(node, config);
        if (ret < 0)
            return ret;
    }

    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);

    struct texture *t = s->textures[0]->priv_data;
    ngli_mat4_mul(t->coordinates_matrix, flip_matrix, matrix);

    node->ctx->activitycheck_nodes.count = 0;
    ret = ngli_node_visit(s->rtt, 1, 0.0);
    if (ret < 0)
        return ret;

    ret = ngli_node_honor_release_prefetch(&node->ctx->activitycheck_nodes);
    if (ret < 0)
        return ret;

    ret = ngli_node_update(s->rtt, 0.0);
    if (ret < 0)
        return ret;

    ngli_node_draw(s->rtt);

    t = s->target_texture->priv_data;
    memcpy(s->coordinates_matrix, t->coordinates_matrix, sizeof(s->coordinates_matrix));

    return 0;
}

int ngli_hwupload_mc_dr_init(struct ngl_node *node,
                             struct hwupload_config *config)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct texture *s = node->priv_data;
    struct media *media = s->data_src->priv_data;

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

    s->id = media->android_texture_id;
    s->target = media->android_texture_target;

    ngli_glBindTexture(gl, s->target, s->id);
    ngli_glTexParameteri(gl, s->target, GL_TEXTURE_MIN_FILTER, s->min_filter);
    ngli_glTexParameteri(gl, s->target, GL_TEXTURE_MAG_FILTER, s->mag_filter);
    ngli_glBindTexture(gl, s->target, 0);

    return 0;
}

int ngli_hwupload_mc_dr_upload(struct ngl_node *node,
                               struct hwupload_config *config,
                               struct sxplayer_frame *frame)
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

    s->width  = config->width;
    s->height = config->height;

    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);
    ngli_mat4_mul(s->coordinates_matrix, flip_matrix, matrix);

    return 0;
}
