/*
 * Copyright 2016 GoPro Inc.
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

#include "config.h"

#if defined(TARGET_ANDROID)
#include <libavcodec/mediacodec.h>
#include "android_imagereader.h"
#endif

#include "log.h"
#include "nodegl.h"
#include "internal.h"

#if defined(TARGET_ANDROID)
#include "gpu_ctx.h"
#include "backends/gl/texture_gl.h"
#endif

struct media_opts {
    const char *filename;
    int sxplayer_min_level;
    struct ngl_node *anim;
    int audio_tex;
    int max_nb_packets;
    int max_nb_frames;
    int max_nb_sink;
    int max_pixels;
    int stream_idx;
    int hwaccel;
    char *filters;
    char *vt_pix_fmt;
};

static const struct param_choices sxplayer_log_level_choices = {
    .name = "sxplayer_log_level",
    .consts = {
        {"verbose", SXPLAYER_LOG_VERBOSE, .desc=NGLI_DOCSTRING("verbose messages")},
        {"debug",   SXPLAYER_LOG_DEBUG,   .desc=NGLI_DOCSTRING("debugging messages")},
        {"info",    SXPLAYER_LOG_INFO,    .desc=NGLI_DOCSTRING("informational messages")},
        {"warning", SXPLAYER_LOG_WARNING, .desc=NGLI_DOCSTRING("warning messages")},
        {"error",   SXPLAYER_LOG_ERROR,   .desc=NGLI_DOCSTRING("error messages")},
        {NULL}
    }
};

#define HWACCEL_DISABLED 0
#define HWACCEL_AUTO     1

static const struct param_choices sxplayer_hwaccel_choices = {
    .name = "sxplayer_hwaccel",
    .consts = {
        {"disabled", HWACCEL_DISABLED, .desc=NGLI_DOCSTRING("disable hardware acceleration")},
        {"auto",     HWACCEL_AUTO,     .desc=NGLI_DOCSTRING("enable hardware acceleration if available")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct media_opts, x)
static const struct node_param media_params[] = {
    {"filename", NGLI_PARAM_TYPE_STR, OFFSET(filename), {.str=NULL}, NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("path to input media file")},
    {"sxplayer_min_level", NGLI_PARAM_TYPE_SELECT, OFFSET(sxplayer_min_level), {.i32=SXPLAYER_LOG_WARNING},
                           .choices=&sxplayer_log_level_choices,
                           .desc=NGLI_DOCSTRING("sxplayer min logging level")},
    {"time_anim", NGLI_PARAM_TYPE_NODE, OFFSET(anim),
                  .node_types=(const int[]){NGL_NODE_ANIMATEDTIME, -1},
                  .desc=NGLI_DOCSTRING("time remapping animation (must use a `linear` interpolation)")},
    {"audio_tex", NGLI_PARAM_TYPE_BOOL, OFFSET(audio_tex),
                  .desc=NGLI_DOCSTRING("load the audio and expose it as a stereo waves and frequencies buffer")},
    {"max_nb_packets", NGLI_PARAM_TYPE_I32, OFFSET(max_nb_packets), {.i32=1},
                       .desc=NGLI_DOCSTRING("maximum number of packets in sxplayer demuxing queue")},
    {"max_nb_frames",  NGLI_PARAM_TYPE_I32, OFFSET(max_nb_frames),  {.i32=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in sxplayer decoding queue")},
    {"max_nb_sink",    NGLI_PARAM_TYPE_I32, OFFSET(max_nb_sink),    {.i32=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in sxplayer filtering queue")},
    {"max_pixels",     NGLI_PARAM_TYPE_I32, OFFSET(max_pixels),     {.i32=0},
                       .desc=NGLI_DOCSTRING("maximum number of pixels per frame")},
    {"stream_idx",     NGLI_PARAM_TYPE_I32, OFFSET(stream_idx),     {.i32=-1},
                       .desc=NGLI_DOCSTRING("force a stream number instead of picking the \"best\" one")},
    {"hwaccel",        NGLI_PARAM_TYPE_SELECT, OFFSET(hwaccel),     {.i32=HWACCEL_AUTO},
                       .choices=&sxplayer_hwaccel_choices,
                       .desc=NGLI_DOCSTRING("hardware acceleration")},
    {"filters",        NGLI_PARAM_TYPE_STR, OFFSET(filters),
                       .desc=NGLI_DOCSTRING("filters to apply on the media (sxplayer/libavfilter)")},
    {"vt_pix_fmt",     NGLI_PARAM_TYPE_STR, OFFSET(vt_pix_fmt),  {.str="auto"},
                       .desc=NGLI_DOCSTRING("auto or a comma or space separated list of VideoToolbox (Apple) allowed output pixel formats")},
    {NULL}
};

static const int log_levels[] = {
    [SXPLAYER_LOG_VERBOSE] = NGL_LOG_VERBOSE,
    [SXPLAYER_LOG_DEBUG]   = NGL_LOG_DEBUG,
    [SXPLAYER_LOG_INFO]    = NGL_LOG_INFO,
    [SXPLAYER_LOG_WARNING] = NGL_LOG_WARNING,
    [SXPLAYER_LOG_ERROR]   = NGL_LOG_ERROR,
};

static void callback_sxplayer_log(void *arg, int level, const char *filename, int ln,
                                  const char *fn, const char *fmt, va_list vl)
{
    if (level < 0 || level >= NGLI_ARRAY_NB(log_levels))
        return;

    const struct media_opts *o = arg;
    if (level < o->sxplayer_min_level)
        return;

    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, vl);
    if (buf[0])
        ngli_log_print(log_levels[level], __FILE__, __LINE__, __func__,
                       "[SXPLAYER %s:%d %s] %s", filename, ln, fn, buf);
}

#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
static const char *get_default_vt_pix_fmts(int backend)
{
    /* OpenGLES 3.0 (iOS) does not support 16-bit texture formats */
    if (backend == NGL_BACKEND_OPENGLES)
        return "nv12";

    return "nv12,p010";
}
#endif

static int media_init(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    const struct media_opts *o = node->opts;

    s->player = sxplayer_create(o->filename);
    if (!s->player)
        return NGL_ERROR_MEMORY;

    sxplayer_set_log_callback(s->player, node->opts, callback_sxplayer_log);

    struct ngl_node *anim_node = o->anim;
    if (anim_node) {
        const struct variable_opts *anim = anim_node->opts;

        // Set the media time boundaries using the time remapping animation
        if (anim->nb_animkf) {
            const struct animkeyframe_opts *kf0 = anim->animkf[0]->opts;
            const double initial_seek = kf0->scalar;

            sxplayer_set_option(s->player, "skip", initial_seek);

            if (anim->nb_animkf > 1) {
                const struct animkeyframe_opts *kfn = anim->animkf[anim->nb_animkf - 1]->opts;
                const double last_time = kfn->scalar;
                sxplayer_set_option(s->player, "trim_duration", last_time - initial_seek);
            }
        }
    }

    if (o->max_nb_packets) sxplayer_set_option(s->player, "max_nb_packets", o->max_nb_packets);
    if (o->max_nb_frames)  sxplayer_set_option(s->player, "max_nb_frames",  o->max_nb_frames);
    if (o->max_nb_sink)    sxplayer_set_option(s->player, "max_nb_sink",    o->max_nb_sink);
    if (o->max_pixels)     sxplayer_set_option(s->player, "max_pixels",     o->max_pixels);
    if (o->filters)        sxplayer_set_option(s->player, "filters",        o->filters);

    sxplayer_set_option(s->player, "stream_idx", o->stream_idx);
    sxplayer_set_option(s->player, "auto_hwaccel", o->hwaccel);

    sxplayer_set_option(s->player, "sw_pix_fmt", SXPLAYER_PIXFMT_AUTO);
#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    const struct ngl_ctx *ctx = node->ctx;
    const struct ngl_config *config = &ctx->config;
    const char *vt_pix_fmt = o->vt_pix_fmt;
    if (!strcmp(o->vt_pix_fmt, "auto"))
        vt_pix_fmt = get_default_vt_pix_fmts(config->backend);
    sxplayer_set_option(s->player, "vt_pix_fmt", vt_pix_fmt);
#endif

    if (o->audio_tex) {
        sxplayer_set_option(s->player, "avselect", SXPLAYER_SELECT_AUDIO);
        sxplayer_set_option(s->player, "audio_texture", 1);
        return 0;
    }

#if defined(TARGET_ANDROID)
    struct ngl_ctx *ctx = node->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    const struct ngl_config *config = &ctx->config;

    if (config->backend == NGL_BACKEND_OPENGLES) {
        void *android_surface = NULL;
        if (android_ctx->has_native_imagereader_api) {
            s->android_imagereader = ngli_android_imagereader_create(android_ctx, 1, 1,
                                                                     NGLI_ANDROID_IMAGE_FORMAT_YUV_420_888, 2);
            if (!s->android_imagereader)
                return NGL_ERROR_MEMORY;

            int ret = ngli_android_imagereader_get_window(s->android_imagereader, &android_surface);
            if (ret < 0)
                return ret;
        } else {
            s->android_handlerthread = ngli_android_handlerthread_new();
            if (!s->android_handlerthread)
                return NGL_ERROR_MEMORY;

            void *handler = ngli_android_handlerthread_get_native_handler(s->android_handlerthread);
            if (!handler)
                return NGL_ERROR_EXTERNAL;

            s->android_surface = ngli_android_surface_new(0, handler);
            if (!s->android_surface)
                return NGL_ERROR_MEMORY;

            android_surface = ngli_android_surface_get_surface(s->android_surface);
            if (!android_surface)
                return NGL_ERROR_EXTERNAL;
        }

        sxplayer_set_option(s->player, "opaque", &android_surface);
    }
#elif defined(HAVE_VAAPI)
    struct ngl_ctx *ctx = node->ctx;
    struct vaapi_ctx *vaapi_ctx = &ctx->vaapi_ctx;
    sxplayer_set_option(s->player, "opaque", &vaapi_ctx->va_display);
#endif

    return 0;
}

static int media_prefetch(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    sxplayer_start(s->player);
    return 0;
}

static const char * const pix_fmt_names[] = {
    [SXPLAYER_PIXFMT_RGBA]       = "rgba",
    [SXPLAYER_PIXFMT_BGRA]       = "bgra",
    [SXPLAYER_PIXFMT_VT]         = "vt",
    [SXPLAYER_PIXFMT_MEDIACODEC] = "mediacodec",
    [SXPLAYER_PIXFMT_VAAPI]      = "vaapi",
    [SXPLAYER_PIXFMT_NV12]       = "nv12",
    [SXPLAYER_PIXFMT_YUV420P]    = "yuv420p",
    [SXPLAYER_PIXFMT_YUV422P]    = "yuv422p",
    [SXPLAYER_PIXFMT_YUV444P]    = "yuv444p",
    [SXPLAYER_PIXFMT_P010LE]     = "p010le",
    [SXPLAYER_PIXFMT_YUV420P10LE] = "yuv420p10le",
    [SXPLAYER_PIXFMT_YUV422P10LE] = "yuv422p10le",
    [SXPLAYER_PIXFMT_YUV444P10LE] = "yuv444p10le",
};

static int media_update(struct ngl_node *node, double t)
{
    struct media_priv *s = node->priv_data;
    const struct media_opts *o = node->opts;
    struct ngl_node *anim_node = o->anim;
    double media_time = t;

    if (anim_node) {
        struct variable_priv *anim = anim_node->priv_data;
        const struct variable_opts *anim_o = anim_node->opts;
        const struct animkeyframe_opts *kf0 = anim_o->animkf[0]->opts;
        const double initial_seek = kf0->scalar;
        int ret = ngli_node_update(anim_node, t);
        if (ret < 0)
            return ret;
        const double dval = *(double *)anim->data;
        media_time = NGLI_MAX(0, dval - initial_seek);

        TRACE("remapped time f(%g)=%g", t, media_time);
    }

    sxplayer_release_frame(s->frame);

    TRACE("get frame from %s at t=%g", node->label, media_time);
    struct sxplayer_frame *frame = sxplayer_get_frame(s->player, media_time);
    if (frame) {
        const char *pix_fmt_str = frame->pix_fmt >= 0 &&
                                  frame->pix_fmt < NGLI_ARRAY_NB(pix_fmt_names) ? pix_fmt_names[frame->pix_fmt]
                                                                                : NULL;
        if (o->audio_tex) {
            if (frame->pix_fmt != SXPLAYER_SMPFMT_FLT) {
                LOG(ERROR, "unexpected %s (%d) sxplayer frame",
                    pix_fmt_str ? pix_fmt_str : "unknown", frame->pix_fmt);
                return NGL_ERROR_BUG;
            }
            pix_fmt_str = "audio";
        } else if (!pix_fmt_str) {
            LOG(ERROR, "invalid pixel format %d in sxplayer frame", frame->pix_fmt);
            return NGL_ERROR_BUG;
        }
        TRACE("got frame %dx%d %s with ts=%f", frame->width, frame->height,
              pix_fmt_str, frame->ts);
    }
    s->frame = frame;
    return 0;
}

static void media_release(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    sxplayer_release_frame(s->frame);
    s->frame = NULL;
    sxplayer_stop(s->player);
}

static void media_uninit(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    sxplayer_free(&s->player);

#if defined(TARGET_ANDROID)
    struct ngl_ctx *ctx = node->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    if (android_ctx->has_native_imagereader_api) {
        ngli_android_imagereader_freep(&s->android_imagereader);
    } else {
        ngli_android_surface_free(&s->android_surface);
        ngli_android_handlerthread_free(&s->android_handlerthread);
    }
#endif
}

const struct node_class ngli_media_class = {
    .id        = NGL_NODE_MEDIA,
    .name      = "Media",
    .init      = media_init,
    .prefetch  = media_prefetch,
    .update    = media_update,
    .release   = media_release,
    .uninit    = media_uninit,
    .opts_size = sizeof(struct media_opts),
    .priv_size = sizeof(struct media_priv),
    .params    = media_params,
    .file      = __FILE__,
};
