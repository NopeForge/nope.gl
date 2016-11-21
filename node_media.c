/*
 * Copyright 2016 Clément Bœsch <cboesch@gopro.com>
 * Copyright 2016 Matthieu Bouron <mbouron@gopro.com>
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
#include <sxplayer.h>

#ifdef __ANDROID__
#include <libavcodec/mediacodec.h>
#endif

#include "gl_utils.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct media, x)
static const struct node_param media_params[] = {
    {"filename", PARAM_TYPE_STR, OFFSET(filename), {.str=NULL}, PARAM_FLAG_CONSTRUCTOR},
    {"start",    PARAM_TYPE_DBL, OFFSET(start)},
    {"initial_seek", PARAM_TYPE_DBL, OFFSET(initial_seek)},
    {NULL}
};

static int media_init(struct ngl_node *node)
{
    struct media *s = node->priv_data;
    LOG(VERBOSE, "media '%s' @ %f", s->filename, s->start);

    s->player = sxplayer_create(s->filename);
    if (!s->player)
        return -1;

    sxplayer_set_option(s->player, "max_nb_packets", 1);
    sxplayer_set_option(s->player, "max_nb_frames", 1);
    sxplayer_set_option(s->player, "max_nb_sink", 1);
    sxplayer_set_option(s->player, "sw_pix_fmt", SXPLAYER_PIXFMT_RGBA);
    sxplayer_set_option(s->player, "skip", s->initial_seek);

#ifdef __ANDROID__
    glGenTextures(1, &s->android_texture_id);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, s->android_texture_id);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    s->android_texture_target = GL_TEXTURE_EXTERNAL_OES;

    s->android_surface = av_android_surface_new(NULL, s->android_texture_id);
    void *android_surface = av_android_surface_get_surface(s->android_surface);
    sxplayer_set_option(s->player, "opaque", &android_surface);
#endif

    return 0;
}

static void media_prefetch(struct ngl_node *node)
{
    struct media *s = node->priv_data;
    sxplayer_start(s->player);
}

static void media_update(struct ngl_node *node, double t)
{
    struct media *s = node->priv_data;

    t = t - s->start;
    if (t < 0)
        return;

    sxplayer_release_frame(s->frame);
    LOG(VERBOSE, "get frame from %s at t=%f", node->name, t);
    s->frame = sxplayer_get_frame(s->player, t);
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
    av_android_surface_free((AVAndroidSurface **)&s->android_surface);
    glDeleteTextures(1, &s->android_texture_id);
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
};
