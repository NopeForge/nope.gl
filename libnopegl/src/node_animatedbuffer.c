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
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/type.h"
#include "node_animkeyframe.h"
#include "node_buffer.h"
#include "nopegl.h"
#include "utils/memory.h"

struct animatedbuffer_opts {
    struct ngl_node **animkf;
    size_t nb_animkf;
};

struct animatedbuffer_priv {
    struct buffer_info buf;
    struct animation anim;
};

NGLI_STATIC_ASSERT(offsetof(struct animatedbuffer_priv, buf) == 0, "buffer_info is first");

#define OFFSET(x) offsetof(struct animatedbuffer_opts, x)
static const struct node_param animatedbuffer_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(animkf),
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMKEYFRAMEBUFFER, NGLI_NODE_NONE},
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
    const struct buffer_info *info = &s->buf;
    const float *d0 = (const float *)kf0->data;
    const float *d1 = (const float *)kf1->data;
    const struct buffer_layout *layout = &info->layout;
    const size_t comp = layout->comp;
    for (size_t k = 0; k < layout->count; k++)
        for (size_t i = 0; i < comp; i++)
            dstf[k*comp + i] = NGLI_MIX_F32(d0[k*comp + i], d1[k*comp + i], (float)ratio);
}

static void cpy_buffer(void *user_arg, void *dst,
                       const struct animkeyframe_opts *kf)
{
    const struct animatedbuffer_priv *s = user_arg;
    const struct buffer_info *info = &s->buf;
    memcpy(dst, kf->data, info->data_size);
}

static int animatedbuffer_update(struct ngl_node *node, double t)
{
    struct animatedbuffer_priv *s = node->priv_data;
    struct buffer_info *info = &s->buf;
    int ret = ngli_animation_evaluate(&s->anim, info->data, t);
    if (ret < 0)
        return ret;

    if (!(info->flags & NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD))
        return 0;

    return ngpu_buffer_upload(info->buffer, info->data, 0, info->data_size);
}

static int animatedbuffer_init(struct ngl_node *node)
{
    struct animatedbuffer_priv *s = node->priv_data;
    const struct animatedbuffer_opts *o = node->opts;
    struct buffer_info *info = &s->buf;
    struct buffer_layout *layout = &info->layout;

    info->flags |= NGLI_BUFFER_INFO_FLAG_DYNAMIC;
    info->usage = NGPU_BUFFER_USAGE_DYNAMIC_BIT | NGPU_BUFFER_USAGE_TRANSFER_DST_BIT;
    layout->comp = ngpu_format_get_nb_comp(layout->format);
    layout->stride = ngpu_format_get_bytes_per_pixel(layout->format);

    int ret = ngli_animation_init(&s->anim, s,
                                  o->animkf, o->nb_animkf,
                                  mix_buffer, cpy_buffer);
    if (ret < 0)
        return ret;

    for (size_t i = 0; i < o->nb_animkf; i++) {
        const struct animkeyframe_opts *kf = o->animkf[i]->opts;
        const size_t data_count = kf->data_size / layout->stride;
        const size_t data_pad   = kf->data_size % layout->stride;

        if (layout->count && layout->count != data_count) {
            static const char *types[] = {"float", "vec2", "vec3", "vec4"};
            LOG(ERROR, "the number of %s in buffer key frame %zu "
                "does not match the previous ones (%zu vs %zu)",
                types[layout->comp - 1], i, data_count, layout->count);
            return NGL_ERROR_INVALID_ARG;
        }

        if (data_pad)
            LOG(WARNING, "the data buffer has %zu trailing bytes", data_pad);

        layout->count = data_count;
    }

    if (!layout->count)
        return NGL_ERROR_INVALID_ARG;

    info->data = ngli_calloc(layout->count, layout->stride);
    if (!info->data)
        return NGL_ERROR_MEMORY;
    info->data_size = layout->count * layout->stride;

    info->buffer = ngpu_buffer_create(node->ctx->gpu_ctx);
    if (!info->buffer)
        return NGL_ERROR_MEMORY;

    return 0;
}

static int animatedbuffer_prepare(struct ngl_node *node)
{
    struct animatedbuffer_priv *s = node->priv_data;
    struct buffer_info *info = &s->buf;

    if (!(info->flags & NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD))
        return ngli_node_prepare_children(node);

    if (info->buffer->size)
        return 0;

    int ret = ngpu_buffer_init(info->buffer, info->data_size, info->usage);
    if (ret < 0)
        return ret;

    return ngli_node_prepare_children(node);
}

static void animatedbuffer_uninit(struct ngl_node *node)
{
    struct animatedbuffer_priv *s = node->priv_data;
    struct buffer_info *info = &s->buf;

    ngpu_buffer_freep(&info->buffer);
    ngli_freep(&info->data);
}

#define DEFINE_ABUFFER_CLASS(class_id, class_name, type_name, class_data_type, class_data_format)  \
static int animatedbuffer##type_name##_init(struct ngl_node *node)                 \
{                                                                                  \
    struct animatedbuffer_priv *s = node->priv_data;                               \
    struct buffer_info *info = &s->buf;                                            \
    info->layout.format = class_data_format;                                       \
    info->layout.type   = class_data_type;                                         \
    return animatedbuffer_init(node);                                              \
}                                                                                  \
                                                                                   \
const struct node_class ngli_animatedbuffer##type_name##_class = {                 \
    .id        = class_id,                                                         \
    .category  = NGLI_NODE_CATEGORY_BUFFER,                                        \
    .name      = class_name,                                                       \
    .init      = animatedbuffer##type_name##_init,                                 \
    .prepare   = animatedbuffer_prepare,                                           \
    .update    = animatedbuffer_update,                                            \
    .uninit    = animatedbuffer_uninit,                                            \
    .opts_size = sizeof(struct animatedbuffer_opts),                               \
    .priv_size = sizeof(struct animatedbuffer_priv),                               \
    .params    = animatedbuffer_params,                                            \
    .params_id = "AnimatedBuffer",                                                 \
    .file      = __FILE__,                                                         \
};                                                                                 \

DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERFLOAT, "AnimatedBufferFloat", float, NGPU_TYPE_F32, NGPU_FORMAT_R32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC2, "AnimatedBufferVec2", vec2, NGPU_TYPE_VEC2, NGPU_FORMAT_R32G32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC3, "AnimatedBufferVec3", vec3, NGPU_TYPE_VEC3, NGPU_FORMAT_R32G32B32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC4, "AnimatedBufferVec4", vec4, NGPU_TYPE_VEC4, NGPU_FORMAT_R32G32B32A32_SFLOAT)
