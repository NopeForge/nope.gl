/*
 * Copyright 2016-2022 GoPro Inc.
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
#include <nopemd.h>

#include "config.h"

#if defined(TARGET_ANDROID)
#include "android_imagereader.h"
#endif

#include "internal.h"
#include "log.h"
#include "node_animkeyframe.h"
#include "node_media.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "utils/memory.h"

#if defined(TARGET_ANDROID)
#include "ngpu/ctx.h"
#include "ngpu/opengl/texture_gl.h"
#endif

struct media_opts {
    const char *filename;
    int nopemd_min_level;
    struct ngl_node *anim;
    int audio_tex;
    int32_t max_nb_packets;
    int32_t max_nb_frames;
    int32_t max_nb_sink;
    int32_t max_pixels;
    int32_t stream_idx;
    int hwaccel;
    char *filters;
    char *vt_pix_fmt;
};

static const struct param_choices nopemd_log_level_choices = {
    .name = "nopemd_log_level",
    .consts = {
        {"verbose", NMD_LOG_VERBOSE, .desc=NGLI_DOCSTRING("verbose messages")},
        {"debug",   NMD_LOG_DEBUG,   .desc=NGLI_DOCSTRING("debugging messages")},
        {"info",    NMD_LOG_INFO,    .desc=NGLI_DOCSTRING("informational messages")},
        {"warning", NMD_LOG_WARNING, .desc=NGLI_DOCSTRING("warning messages")},
        {"error",   NMD_LOG_ERROR,   .desc=NGLI_DOCSTRING("error messages")},
        {NULL}
    }
};

#define HWACCEL_DISABLED 0
#define HWACCEL_AUTO     1

static const struct param_choices nopemd_hwaccel_choices = {
    .name = "nopemd_hwaccel",
    .consts = {
        {"disabled", HWACCEL_DISABLED, .desc=NGLI_DOCSTRING("disable hardware acceleration")},
        {"auto",     HWACCEL_AUTO,     .desc=NGLI_DOCSTRING("enable hardware acceleration if available")},
        {NULL}
    }
};

static int filename_changed(struct ngl_node *node);

#define OFFSET(x) offsetof(struct media_opts, x)
static const struct node_param media_params[] = {
    {"filename", NGLI_PARAM_TYPE_STR, OFFSET(filename), {.str=NULL},
                 .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_FILEPATH | NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=filename_changed,
                 .desc=NGLI_DOCSTRING("path to input media file")},
    {"nopemd_min_level", NGLI_PARAM_TYPE_SELECT, OFFSET(nopemd_min_level), {.i32=NMD_LOG_WARNING},
                         .choices=&nopemd_log_level_choices,
                         .desc=NGLI_DOCSTRING("nope.media min logging level")},
    {"time_anim", NGLI_PARAM_TYPE_NODE, OFFSET(anim),
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMATEDTIME, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("time remapping animation (must use a `linear` interpolation)")},
    {"audio_tex", NGLI_PARAM_TYPE_BOOL, OFFSET(audio_tex),
                  .desc=NGLI_DOCSTRING("load the audio and expose it as a stereo waves and frequencies buffer")},
    {"max_nb_packets", NGLI_PARAM_TYPE_I32, OFFSET(max_nb_packets), {.i32=1},
                       .desc=NGLI_DOCSTRING("maximum number of packets in nope.media demuxing queue")},
    {"max_nb_frames",  NGLI_PARAM_TYPE_I32, OFFSET(max_nb_frames),  {.i32=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in nope.media decoding queue")},
    {"max_nb_sink",    NGLI_PARAM_TYPE_I32, OFFSET(max_nb_sink),    {.i32=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in nope.media filtering queue")},
    {"max_pixels",     NGLI_PARAM_TYPE_I32, OFFSET(max_pixels),     {.i32=0},
                       .desc=NGLI_DOCSTRING("maximum number of pixels per frame")},
    {"stream_idx",     NGLI_PARAM_TYPE_I32, OFFSET(stream_idx),     {.i32=-1},
                       .desc=NGLI_DOCSTRING("force a stream number instead of picking the \"best\" one")},
    {"hwaccel",        NGLI_PARAM_TYPE_SELECT, OFFSET(hwaccel),     {.i32=HWACCEL_AUTO},
                       .choices=&nopemd_hwaccel_choices,
                       .desc=NGLI_DOCSTRING("hardware acceleration")},
    {"filters",        NGLI_PARAM_TYPE_STR, OFFSET(filters),
                       .desc=NGLI_DOCSTRING("filters to apply on the media (nope.media/libavfilter)")},
    {"vt_pix_fmt",     NGLI_PARAM_TYPE_STR, OFFSET(vt_pix_fmt),  {.str="auto"},
                       .desc=NGLI_DOCSTRING("auto or a comma or space separated list of VideoToolbox (Apple) allowed output pixel formats")},
    {NULL}
};

static const enum ngl_log_level log_levels[] = {
    [NMD_LOG_VERBOSE] = NGL_LOG_VERBOSE,
    [NMD_LOG_DEBUG]   = NGL_LOG_DEBUG,
    [NMD_LOG_INFO]    = NGL_LOG_INFO,
    [NMD_LOG_WARNING] = NGL_LOG_WARNING,
    [NMD_LOG_ERROR]   = NGL_LOG_ERROR,
};

ngli_printf_format(6, 0)
static void callback_nopemd_log(void *arg, int level, const char *filename, int ln,
                                const char *fn, const char *fmt, va_list vl)
{
    if (level < 0 || level >= NGLI_ARRAY_NB(log_levels))
        return;

    const struct media_opts *o = arg;
    if (level < o->nopemd_min_level)
        return;

    char logline[128];
    char *logbuf = NULL;
    const char *logp = logline;

    /* we need a copy because it may be re-used a 2nd time */
    va_list vl_copy;
    va_copy(vl_copy, vl);

    int len = vsnprintf(logline, sizeof(logline), fmt, vl);

    /* handle the case where the line doesn't fit the stack buffer */
    if (len >= sizeof(logline)) {
        const size_t logbuf_len = (size_t)len + 1;
        logbuf = ngli_malloc(logbuf_len);
        if (!logbuf) {
            va_end(vl_copy);
            return;
        }
        vsnprintf(logbuf, logbuf_len, fmt, vl_copy);
        logp = logbuf;
    }

    if (logp[0])
        ngli_log_print(log_levels[level], __FILE__, __LINE__, __func__,
                       "[NOPE.MEDIA %s:%d %s] %s", filename, ln, fn, logp);

    ngli_free(logbuf);
    va_end(vl_copy);
}

#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
static const char *get_default_vt_pix_fmts(enum ngl_backend_type backend)
{
    /* OpenGLES 3.0 (iOS) does not support 16-bit texture formats */
    if (backend == NGL_BACKEND_OPENGLES)
        return "nv12";

    return "nv12,p010";
}
#endif

#if defined(TARGET_ANDROID)
static int init_android_surface(struct android_ctx *ctx, struct android_surface_compat *surface)
{
    surface->imagereader = ngli_android_imagereader_create(ctx, 1, 1,
                                                           NGLI_ANDROID_IMAGE_FORMAT_PRIVATE, 2);
    if (!surface->imagereader)
        return NGL_ERROR_MEMORY;

    int ret = ngli_android_imagereader_get_window(surface->imagereader, &surface->surface_handle);
    if (ret < 0)
        return ret;

    return 0;
}

static void reset_android_surface(struct android_surface_compat *surface)
{
    ngli_android_imagereader_freep(&surface->imagereader);

    memset(surface, 0, sizeof(*surface));
}
#endif

static int media_init(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    const struct media_opts *o = node->opts;

    s->player = nmd_create(o->filename);
    if (!s->player)
        return NGL_ERROR_MEMORY;

    nmd_set_log_callback(s->player, node->opts, callback_nopemd_log);

    struct ngl_node *anim_node = o->anim;
    if (anim_node) {
        const struct variable_opts *anim = anim_node->opts;

        // Set the media time boundaries using the time remapping animation
        if (anim->nb_animkf) {
            const struct animkeyframe_opts *kf0 = anim->animkf[0]->opts;
            const double initial_seek = kf0->scalar;

            nmd_set_option(s->player, "start_time", initial_seek);

            if (anim->nb_animkf > 1) {
                const struct animkeyframe_opts *kfn = anim->animkf[anim->nb_animkf - 1]->opts;
                const double last_time = kfn->scalar;
                nmd_set_option(s->player, "end_time", last_time);
            }
        }
    }

    if (o->max_nb_packets) nmd_set_option(s->player, "max_nb_packets", o->max_nb_packets);
    if (o->max_nb_frames)  nmd_set_option(s->player, "max_nb_frames",  o->max_nb_frames);
    if (o->max_nb_sink)    nmd_set_option(s->player, "max_nb_sink",    o->max_nb_sink);
    if (o->max_pixels)     nmd_set_option(s->player, "max_pixels",     o->max_pixels);
    if (o->filters)        nmd_set_option(s->player, "filters",        o->filters);

    nmd_set_option(s->player, "stream_idx", o->stream_idx);
    nmd_set_option(s->player, "auto_hwaccel", o->hwaccel);

    nmd_set_option(s->player, "sw_pix_fmt", NMD_PIXFMT_AUTO);
    if (o->audio_tex) {
        nmd_set_option(s->player, "avselect", NMD_SELECT_AUDIO);
        nmd_set_option(s->player, "audio_texture", 1);
        return 0;
    }

#if defined(TARGET_ANDROID)
    struct ngl_ctx *ctx = node->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    int ret = init_android_surface(android_ctx, &s->android_surface);
    if (ret < 0)
        return ret;
    nmd_set_option(s->player, "opaque", &s->android_surface.surface_handle);
#elif defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    const struct ngl_ctx *ctx = node->ctx;
    const struct ngl_config *config = &ctx->config;
    const char *vt_pix_fmt = o->vt_pix_fmt;
    if (!strcmp(o->vt_pix_fmt, "auto"))
        vt_pix_fmt = get_default_vt_pix_fmts(config->backend);
    nmd_set_option(s->player, "vt_pix_fmt", vt_pix_fmt);
#elif defined(HAVE_VAAPI)
    struct ngl_ctx *ctx = node->ctx;
    struct vaapi_ctx *vaapi_ctx = &ctx->vaapi_ctx;
    nmd_set_option(s->player, "opaque", &vaapi_ctx->va_display);
#endif

    return 0;
}

static int media_prefetch(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    nmd_start(s->player);
    s->prefetched = 1;
    return 0;
}

static const char * const pix_fmt_names[] = {
    [NMD_PIXFMT_RGBA]        = "rgba",
    [NMD_PIXFMT_BGRA]        = "bgra",
    [NMD_PIXFMT_VT]          = "vt",
    [NMD_PIXFMT_MEDIACODEC]  = "mediacodec",
    [NMD_PIXFMT_VAAPI]       = "vaapi",
    [NMD_PIXFMT_NV12]        = "nv12",
    [NMD_PIXFMT_YUV420P]     = "yuv420p",
    [NMD_PIXFMT_YUV422P]     = "yuv422p",
    [NMD_PIXFMT_YUV444P]     = "yuv444p",
    [NMD_PIXFMT_P010LE]      = "p010le",
    [NMD_PIXFMT_YUV420P10LE] = "yuv420p10le",
    [NMD_PIXFMT_YUV422P10LE] = "yuv422p10le",
    [NMD_PIXFMT_YUV444P10LE] = "yuv444p10le",
};

static const char *get_pix_fmt_name(int pix_fmt)
{
    if (pix_fmt < 0 || pix_fmt >= NGLI_ARRAY_NB(pix_fmt_names))
        return NULL;

    return pix_fmt_names[pix_fmt];
}

static const char *get_nmd_ret_name(int nmd_err)
{
    switch (nmd_err) {
    case NMD_ERR_MEMORY:    return "memory error";
    case NMD_ERR_EOF:       return "reached EOF";
    case NMD_ERR_GENERIC:   return "generic error";
    case NMD_RET_SUCCESS:   return "success";
    case NMD_RET_UNCHANGED: return "unchanged";
    default:                return "unknown";
    }
}

static int media_update(struct ngl_node *node, double t)
{
    struct media_priv *s = node->priv_data;
    const struct media_opts *o = node->opts;
    struct ngl_node *anim_node = o->anim;
    double media_time = t;

    if (anim_node) {
        struct variable_info *anim = anim_node->priv_data;
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

    nmd_frame_releasep(&s->frame);

    TRACE("get frame from %s at t=%g", node->label, media_time);
    struct nmd_frame *frame = NULL;
    int ret = nmd_get_frame(s->player, media_time, &frame);
    if (ret == NMD_RET_NEWFRAME) {
        const char *pix_fmt_str = get_pix_fmt_name(frame->pix_fmt);
        if (o->audio_tex) {
            if (frame->pix_fmt != NMD_SMPFMT_FLT) {
                LOG(ERROR, "unexpected %s (%d) nope.media frame",
                    pix_fmt_str ? pix_fmt_str : "unknown", frame->pix_fmt);
                return NGL_ERROR_BUG;
            }
            pix_fmt_str = "audio";
        } else if (!pix_fmt_str) {
            LOG(ERROR, "invalid pixel format %d in nope.media frame", frame->pix_fmt);
            return NGL_ERROR_BUG;
        }
        TRACE("got frame %dx%d %s with ts=%f", frame->width, frame->height,
              pix_fmt_str, frame->ts);
    } else if (ret < 0 && ret != NMD_ERR_EOF) {
        LOG(ERROR, "failed to get frame: %s", get_nmd_ret_name(ret));
    }
    s->frame = frame;
    return 0;
}

static void media_release(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    nmd_frame_releasep(&s->frame);
    nmd_stop(s->player);
    s->prefetched = 0;
}

static void media_uninit(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    nmd_freep(&s->player);

#if defined(TARGET_ANDROID)
    reset_android_surface(node);
#endif
}

static int filename_changed(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    const int prefetched = s->prefetched;

    if (prefetched)
        media_release(node);
    media_uninit(node);

    int ret = media_init(node);
    if (ret < 0)
        return ret;
    if (prefetched) {
        ret = media_prefetch(node);
        if (ret < 0)
            return ret;
    }

    return 0;
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
