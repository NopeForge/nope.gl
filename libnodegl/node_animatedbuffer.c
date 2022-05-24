/*
 * Copyright 2017-2022 GoPro Inc.
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

#include <float.h>
#include <stddef.h>
#include <string.h>
#include "animation.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"
#include "type.h"

struct animatedbuffer_opts {
    struct ngl_node **animkf;
    int nb_animkf;
};

struct animatedbuffer_priv {
    struct buffer_info buf;
    struct animation anim;
};

NGLI_STATIC_ASSERT(buffer_info_is_first, offsetof(struct animatedbuffer_priv, buf) == 0);

#define OFFSET(x) offsetof(struct animatedbuffer_opts, x)
static const struct node_param animatedbuffer_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf),
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEBUFFER, -1},
                  .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .desc=NGLI_DOCSTRING("key frame buffers to interpolate from")},
    {NULL}
};

static void mix_buffer(void *user_arg, void *dst,
                       const struct animkeyframe_opts *kf0,
                       const struct animkeyframe_opts *kf1,
                       double ratio)
{
    float *dstf = dst;
    const struct animatedbuffer_priv *s = user_arg;
    const float *d0 = (const float *)kf0->data;
    const float *d1 = (const float *)kf1->data;
    const struct buffer_layout *layout = &s->buf.layout;
    const int comp = layout->comp;
    for (int k = 0; k < layout->count; k++)
        for (int i = 0; i < comp; i++)
            dstf[k*comp + i] = NGLI_MIX(d0[k*comp + i], d1[k*comp + i], ratio);
}

static void cpy_buffer(void *user_arg, void *dst,
                       const struct animkeyframe_opts *kf)
{
    const struct animatedbuffer_priv *s = user_arg;
    memcpy(dst, kf->data, s->buf.data_size);
}

static int animatedbuffer_update(struct ngl_node *node, double t)
{
    struct animatedbuffer_priv *s = node->priv_data;
    return ngli_animation_evaluate(&s->anim, s->buf.data, t);
}

static int animatedbuffer_init(struct ngl_node *node)
{
    struct animatedbuffer_priv *s = node->priv_data;
    const struct animatedbuffer_opts *o = node->opts;
    struct buffer_layout *layout = &s->buf.layout;

    s->buf.flags |= NGLI_BUFFER_INFO_FLAG_DYNAMIC;
    s->buf.usage = NGLI_BUFFER_USAGE_DYNAMIC_BIT | NGLI_BUFFER_USAGE_TRANSFER_DST_BIT;
    layout->comp = ngli_format_get_nb_comp(layout->format);
    layout->stride = ngli_format_get_bytes_per_pixel(layout->format);

    int ret = ngli_animation_init(&s->anim, s,
                                  o->animkf, o->nb_animkf,
                                  mix_buffer, cpy_buffer);
    if (ret < 0)
        return ret;

    for (int i = 0; i < o->nb_animkf; i++) {
        const struct animkeyframe_opts *kf = o->animkf[i]->opts;
        const int data_count = kf->data_size / layout->stride;
        const int data_pad   = kf->data_size % layout->stride;

        if (layout->count && layout->count != data_count) {
            static const char *types[] = {"float", "vec2", "vec3", "vec4"};
            LOG(ERROR, "the number of %s in buffer key frame %d "
                "does not match the previous ones (%d vs %d)",
                types[layout->comp - 1], i, data_count, layout->count);
            return NGL_ERROR_INVALID_ARG;
        }

        if (data_pad)
            LOG(WARNING, "the data buffer has %d trailing bytes", data_pad);

        layout->count = data_count;
    }

    if (!layout->count)
        return NGL_ERROR_INVALID_ARG;

    s->buf.data = ngli_calloc(layout->count, layout->stride);
    if (!s->buf.data)
        return NGL_ERROR_MEMORY;
    s->buf.data_size = layout->count * layout->stride;

    return 0;
}

static void animatedbuffer_uninit(struct ngl_node *node)
{
    struct animatedbuffer_priv *s = node->priv_data;

    ngli_freep(&s->buf.data);
}

#define DEFINE_ABUFFER_CLASS(class_id, class_name, type_name, class_data_type, class_data_format)  \
static int animatedbuffer##type_name##_init(struct ngl_node *node)                 \
{                                                                                  \
    struct animatedbuffer_priv *s = node->priv_data;                               \
    s->buf.layout.format = class_data_format;                                      \
    s->buf.layout.type   = class_data_type;                                        \
    return animatedbuffer_init(node);                                              \
}                                                                                  \
                                                                                   \
const struct node_class ngli_animatedbuffer##type_name##_class = {                 \
    .id        = class_id,                                                         \
    .category  = NGLI_NODE_CATEGORY_BUFFER,                                        \
    .name      = class_name,                                                       \
    .init      = animatedbuffer##type_name##_init,                                 \
    .update    = animatedbuffer_update,                                            \
    .uninit    = animatedbuffer_uninit,                                            \
    .opts_size = sizeof(struct animatedbuffer_opts),                               \
    .priv_size = sizeof(struct animatedbuffer_priv),                               \
    .params    = animatedbuffer_params,                                            \
    .params_id = "AnimatedBuffer",                                                 \
    .file      = __FILE__,                                                         \
};                                                                                 \

DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERFLOAT, "AnimatedBufferFloat", float, NGLI_TYPE_FLOAT, NGLI_FORMAT_R32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC2,  "AnimatedBufferVec2",  vec2,  NGLI_TYPE_VEC2,  NGLI_FORMAT_R32G32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC3,  "AnimatedBufferVec3",  vec3,  NGLI_TYPE_VEC3,  NGLI_FORMAT_R32G32B32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC4,  "AnimatedBufferVec4",  vec4,  NGLI_TYPE_VEC4,  NGLI_FORMAT_R32G32B32A32_SFLOAT)
