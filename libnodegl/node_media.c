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

#ifdef __ANDROID__
#include <libavcodec/mediacodec.h>
#endif

#include "glincludes.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct media, x)
static const struct node_param media_params[] = {
    {"filename", PARAM_TYPE_STR, OFFSET(filename), {.str=NULL}, PARAM_FLAG_CONSTRUCTOR,
                 .desc=NGLI_DOCSTRING("path to input media file")},
    {"sxplayer_min_level", PARAM_TYPE_STR, OFFSET(sxplayer_min_level_str), {.str="warning"},
                           .desc=NGLI_DOCSTRING("sxplayer min logging level")},
    {"time_anim", PARAM_TYPE_NODE, OFFSET(anim),
                  .node_types=(const int[]){NGL_NODE_ANIMATEDFLOAT, -1},
                  .desc=NGLI_DOCSTRING("time remapping animation (must use a `linear` interpolation)")},
    {"audio_tex", PARAM_TYPE_BOOL, OFFSET(audio_tex),
                  .desc=NGLI_DOCSTRING("load the audio and expose it as a stereo waves and frequencies buffer")},
    {"max_nb_packets", PARAM_TYPE_INT, OFFSET(max_nb_packets), {.i64=1},
                       .desc=NGLI_DOCSTRING("maximum number of packets in sxplayer demuxing queue")},
    {"max_nb_frames",  PARAM_TYPE_INT, OFFSET(max_nb_frames),  {.i64=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in sxplayer decoding queue")},
    {"max_nb_sink",    PARAM_TYPE_INT, OFFSET(max_nb_sink),    {.i64=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in sxplayer filtering queue")},
    {"max_pixels",     PARAM_TYPE_INT, OFFSET(max_pixels),     {.i64=0},
                       .desc=NGLI_DOCSTRING("maximum number of pixels per frame")},
    {NULL}
};

static const struct {
    const char *str;
    int ngl_id;
} log_levels[] = {
    [SXPLAYER_LOG_VERBOSE] = {"verbose", NGL_LOG_VERBOSE},
    [SXPLAYER_LOG_DEBUG]   = {"debug",   NGL_LOG_DEBUG},
    [SXPLAYER_LOG_INFO]    = {"info",    NGL_LOG_INFO},
    [SXPLAYER_LOG_WARNING] = {"warning", NGL_LOG_WARNING},
    [SXPLAYER_LOG_ERROR]   = {"error",   NGL_LOG_ERROR},
};

static void callback_sxplayer_log(void *arg, int level, const char *filename, int ln,
                                  const char *fn, const char *fmt, va_list vl)
{
    if (level < 0 || level >= NGLI_ARRAY_NB(log_levels))
        return;

    struct media *s = arg;
    if (level < s->sxplayer_min_level)
        return;

    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, vl);
    if (buf[0])
        ngli_log_print(log_levels[level].ngl_id, __FILE__, __LINE__, __FUNCTION__,
                       "[SXPLAYER %s:%d %s] %s", filename, ln, fn, buf);
}

static int media_init(struct ngl_node *node)
{
    int i;
    struct media *s = node->priv_data;

    s->player = sxplayer_create(s->filename);
    if (!s->player)
        return -1;

    sxplayer_set_log_callback(s->player, s, callback_sxplayer_log);

    for (i = 0; i < NGLI_ARRAY_NB(log_levels); i++) {
        if (log_levels[i].str && !strcmp(log_levels[i].str, s->sxplayer_min_level_str)) {
            s->sxplayer_min_level = i;
            break;
        }
    }
    if (i == NGLI_ARRAY_NB(log_levels)) {
        LOG(ERROR, "unrecognized sxplayer log level '%s'", s->sxplayer_min_level_str);
        return -1;
    }

    struct ngl_node *anim_node = s->anim;
    if (anim_node) {
        struct animation *anim = anim_node->priv_data;

        // Sanity checks for time animation keyframe
        double prev_media_time = 0;
        for (i = 0; i < anim->nb_animkf; i++) {
            const struct animkeyframe *kf = anim->animkf[i]->priv_data;
            if (kf->easing != EASING_LINEAR) {
                LOG(ERROR, "only linear interpolation is allowed for time remapping");
                return -1;
            }
            if (kf->scalar < prev_media_time) {
                LOG(ERROR, "media times must be positive and monotically increasing: %g < %g",
                    kf->scalar, prev_media_time);
                return -1;
            }
            prev_media_time = kf->scalar;
        }

        // Set the media time boundaries using the time remapping animation
        if (anim->nb_animkf) {
            const struct animkeyframe *kf0 = anim->animkf[0]->priv_data;
            const double initial_seek = kf0->scalar;

            sxplayer_set_option(s->player, "skip", initial_seek);

            if (anim->nb_animkf > 1) {
                const struct animkeyframe *kfn = anim->animkf[anim->nb_animkf - 1]->priv_data;
                const double last_time = kfn->scalar;
                sxplayer_set_option(s->player, "trim_duration", last_time - initial_seek);
            }
        }
    }

    if (s->max_nb_packets) sxplayer_set_option(s->player, "max_nb_packets", s->max_nb_packets);
    if (s->max_nb_frames)  sxplayer_set_option(s->player, "max_nb_frames",  s->max_nb_frames);
    if (s->max_nb_sink)    sxplayer_set_option(s->player, "max_nb_sink",    s->max_nb_sink);
    if (s->max_pixels)     sxplayer_set_option(s->player, "max_pixels",     s->max_pixels);

    sxplayer_set_option(s->player, "sw_pix_fmt", SXPLAYER_PIXFMT_RGBA);
#if defined(TARGET_IPHONE)
    sxplayer_set_option(s->player, "vt_pix_fmt", "nv12");
#endif

    if (s->audio_tex) {
        sxplayer_set_option(s->player, "avselect", SXPLAYER_SELECT_AUDIO);
        sxplayer_set_option(s->player, "audio_texture", 1);
        return 0;
    }

#ifdef __ANDROID__
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    ngli_glGenTextures(gl, 1, &s->android_texture_id);
    s->android_texture_target = GL_TEXTURE_EXTERNAL_OES;
    ngli_glBindTexture(gl, s->android_texture_target, s->android_texture_id);
    ngli_glTexParameteri(gl, s->android_texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ngli_glTexParameteri(gl, s->android_texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ngli_glTexParameteri(gl, s->android_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ngli_glTexParameteri(gl, s->android_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ngli_glBindTexture(gl, s->android_texture_target, 0);

    s->android_handlerthread = ngli_android_handlerthread_new();
    if (!s->android_handlerthread)
        return -1;

    void *handler = ngli_android_handlerthread_get_native_handler(s->android_handlerthread);
    if (!handler)
        return -1;

    s->android_surface = ngli_android_surface_new(s->android_texture_id, handler);
    if (!s->android_surface)
        return -1;

    void *android_surface = ngli_android_surface_get_surface(s->android_surface);
    if (!android_surface)
        return -1;

    sxplayer_set_option(s->player, "opaque", &android_surface);
#endif

    return 0;
}

static int media_prefetch(struct ngl_node *node)
{
    struct media *s = node->priv_data;
    sxplayer_start(s->player);
    return 0;
}

static const char * const pix_fmt_names[] = {
    [SXPLAYER_PIXFMT_RGBA]       = "rgba",
    [SXPLAYER_PIXFMT_BGRA]       = "bgra",
    [SXPLAYER_PIXFMT_VT]         = "vt",
    [SXPLAYER_PIXFMT_MEDIACODEC] = "mediacodec",
};

static int media_update(struct ngl_node *node, double t)
{
    struct media *s = node->priv_data;
    struct ngl_node *anim_node = s->anim;
    double media_time = t;

    if (anim_node) {
        struct animation *anim = anim_node->priv_data;

        if (anim->nb_animkf >= 1) {
            const struct animkeyframe *kf0 = anim->animkf[0]->priv_data;
            const double initial_seek = kf0->scalar;

            if (anim->nb_animkf == 1) {
                media_time = NGLI_MAX(0, t - kf0->time);
            } else {
                int ret = ngli_node_update(anim_node, t);
                if (ret < 0)
                    return ret;
                media_time = anim->scalar - initial_seek;
            }

            LOG(VERBOSE, "remapped time f(%g)=%g", t, media_time);
            if (media_time < 0) {
                LOG(ERROR, "invalid remapped time %g", media_time);
                return -1;
            }
        }
    }

    sxplayer_release_frame(s->frame);

    LOG(VERBOSE, "get frame from %s at t=%g", node->name, media_time);
    struct sxplayer_frame *frame = sxplayer_get_frame(s->player, media_time);
    if (frame) {
        const char *pix_fmt_str = frame->pix_fmt >= 0 &&
                                  frame->pix_fmt < NGLI_ARRAY_NB(pix_fmt_names) ? pix_fmt_names[frame->pix_fmt]
                                                                                : NULL;
        if (s->audio_tex) {
            if (frame->pix_fmt != SXPLAYER_SMPFMT_FLT) {
                LOG(ERROR, "unexpected %s (%d) sxplayer frame",
                    pix_fmt_str ? pix_fmt_str : "unknown", frame->pix_fmt);
                return -1;
            }
            pix_fmt_str = "audio";
        } else if (!pix_fmt_str) {
            LOG(ERROR, "invalid pixel format %d in sxplayer frame", frame->pix_fmt);
            return -1;
        }
        LOG(VERBOSE, "got frame %dx%d %s with ts=%f", frame->width, frame->height,
            pix_fmt_str, frame->ts);
    }
    s->frame = frame;
    return 0;
}

static void media_release(struct ngl_node *node)
{
    struct media *s = node->priv_data;
    sxplayer_release_frame(s->frame);
    s->frame = NULL;
    sxplayer_stop(s->player);
}

static void media_uninit(struct ngl_node *node)
{
    struct media *s = node->priv_data;
    sxplayer_free(&s->player);

#ifdef __ANDROID__
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *glcontext = ctx->glcontext;
    const struct glfunctions *gl = &glcontext->funcs;

    ngli_android_surface_free(&s->android_surface);
    ngli_glDeleteTextures(gl, 1, &s->android_texture_id);
    ngli_android_handlerthread_free(&s->android_handlerthread);
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
    .priv_size = sizeof(struct media),
    .params    = media_params,
    .file      = __FILE__,
};
