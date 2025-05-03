/*
 * Copyright 2020-2022 GoPro Inc.
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

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "log.h"
#include "node_buffer.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "internal.h"
#include "ngpu/type.h"

struct streamedbuffer_opts {
    uint32_t count;
    struct ngl_node *timestamps;
    struct ngl_node *buffer_node;
    int32_t timebase[2];
    struct ngl_node *time_anim;
};

struct streamedbuffer_priv {
    struct buffer_info buf;
    size_t last_index;
};

NGLI_STATIC_ASSERT(buffer_info_is_first, offsetof(struct streamedbuffer_priv, buf) == 0);

#define OFFSET(x) offsetof(struct streamedbuffer_opts, x)

#define DECLARE_STREAMED_PARAMS(name, allowed_node)                                                       \
static const struct node_param streamedbuffer##name##_params[] = {                                        \
    {"count",      NGLI_PARAM_TYPE_U32, OFFSET(count),                                                    \
                   .desc=NGLI_DOCSTRING("number of elements for each chunk of data to stream")},          \
    {"timestamps", NGLI_PARAM_TYPE_NODE, OFFSET(timestamps), .flags=NGLI_PARAM_FLAG_NON_NULL,             \
                   .node_types=(const uint32_t[]){NGL_NODE_BUFFERINT64, NGLI_NODE_NONE},                  \
                   .desc=NGLI_DOCSTRING("timestamps associated with each chunk of data to stream")},      \
    {"buffer",     NGLI_PARAM_TYPE_NODE, OFFSET(buffer_node), .flags=NGLI_PARAM_FLAG_NON_NULL,            \
                   .node_types=(const uint32_t[]){allowed_node, NGLI_NODE_NONE},                          \
                   .desc=NGLI_DOCSTRING("buffer containing the data to stream")},                         \
    {"timebase",   NGLI_PARAM_TYPE_RATIONAL, OFFSET(timebase), {.r={1, 1000000}},                         \
                   .desc=NGLI_DOCSTRING("time base in which the `timestamps` are represented")},          \
    {"time_anim",  NGLI_PARAM_TYPE_NODE, OFFSET(time_anim),                                               \
                   .node_types=(const uint32_t[]){NGL_NODE_ANIMATEDTIME, NGLI_NODE_NONE},                 \
                   .desc=NGLI_DOCSTRING("time remapping animation (must use a `linear` interpolation)")}, \
    {NULL}                                                                                                \
};

DECLARE_STREAMED_PARAMS(int,    NGL_NODE_BUFFERINT)
DECLARE_STREAMED_PARAMS(ivec2,  NGL_NODE_BUFFERIVEC2)
DECLARE_STREAMED_PARAMS(ivec3,  NGL_NODE_BUFFERIVEC3)
DECLARE_STREAMED_PARAMS(ivec4,  NGL_NODE_BUFFERIVEC4)
DECLARE_STREAMED_PARAMS(uint,   NGL_NODE_BUFFERUINT)
DECLARE_STREAMED_PARAMS(uivec2, NGL_NODE_BUFFERUIVEC2)
DECLARE_STREAMED_PARAMS(uivec3, NGL_NODE_BUFFERUIVEC3)
DECLARE_STREAMED_PARAMS(uivec4, NGL_NODE_BUFFERUIVEC4)
DECLARE_STREAMED_PARAMS(float,  NGL_NODE_BUFFERFLOAT)
DECLARE_STREAMED_PARAMS(vec2,   NGL_NODE_BUFFERVEC2)
DECLARE_STREAMED_PARAMS(vec3,   NGL_NODE_BUFFERVEC3)
DECLARE_STREAMED_PARAMS(vec4,   NGL_NODE_BUFFERVEC4)
DECLARE_STREAMED_PARAMS(mat4,   NGL_NODE_BUFFERMAT4)

static size_t get_data_index(const struct ngl_node *node, size_t start, int64_t t64)
{
    const struct streamedbuffer_opts *o = node->opts;
    const struct buffer_info *timestamps_priv = o->timestamps->priv_data;
    const int64_t *timestamps = (int64_t *)timestamps_priv->data;
    const size_t nb_timestamps = timestamps_priv->layout.count;

    size_t ret = SIZE_MAX;
    for (size_t i = start; i < nb_timestamps; i++) {
        const int64_t ts = timestamps[i];
        if (ts > t64)
            break;
        ret = i;
    }
    return ret;
}

static int streamedbuffer_update(struct ngl_node *node, double t)
{
    struct streamedbuffer_priv *s = node->priv_data;
    struct buffer_info *info = &s->buf;
    const struct streamedbuffer_opts *o = node->opts;
    struct ngl_node *time_anim = o->time_anim;

    double rt = t;
    if (time_anim) {
        struct variable_info *anim = time_anim->priv_data;

        int ret = ngli_node_update(time_anim, t);
        if (ret < 0)
            return ret;
        rt = *(double *)anim->data;

        TRACE("remapped time f(%g)=%g", t, rt);
        if (rt < 0) {
            LOG(ERROR, "invalid remapped time %g", rt);
            return NGL_ERROR_INVALID_ARG;
        }
    }

    const int64_t t64 = llrint(rt * o->timebase[1] / (double)o->timebase[0]);
    size_t index = get_data_index(node, s->last_index, t64);
    if (index == SIZE_MAX) {
        index = get_data_index(node, 0, t64);
        if (index == SIZE_MAX) // the requested time `t` is before the first user timestamp
            index = 0;
    }
    s->last_index = index;

    const struct buffer_info *buffer_info = o->buffer_node->priv_data;
    const struct buffer_layout *layout = &info->layout;
    info->data = buffer_info->data + layout->stride * layout->count * index;

    if (!(info->flags & NGLI_BUFFER_INFO_FLAG_GPU_UPLOAD))
        return 0;

    return ngpu_buffer_upload(info->buffer, info->data, 0, info->data_size);
}

static int check_timestamps_buffer(const struct ngl_node *node)
{
    const struct streamedbuffer_priv *s = node->priv_data;
    const struct streamedbuffer_opts *o = node->opts;
    const struct buffer_info *info = &s->buf;
    const struct buffer_info *timestamps_priv = o->timestamps->priv_data;
    const int64_t *timestamps = (int64_t *)timestamps_priv->data;
    const size_t nb_timestamps = timestamps_priv->layout.count;

    if (!nb_timestamps) {
        LOG(ERROR, "timestamps buffer must not be empty");
        return NGL_ERROR_INVALID_ARG;
    }

    const struct buffer_info *buffer_info = o->buffer_node->priv_data;
    const size_t count = buffer_info->layout.count / info->layout.count;
    if (nb_timestamps != count) {
        LOG(ERROR, "timestamps count must match buffer chunk count: %zu != %zu", nb_timestamps, count);
        return NGL_ERROR_INVALID_ARG;
    }

    int64_t last_ts = timestamps[0];
    for (size_t i = 1; i < nb_timestamps; i++) {
        const int64_t ts = timestamps[i];
        if (ts < 0) {
            LOG(ERROR, "timestamps must be positive: %" PRId64, ts);
            return NGL_ERROR_INVALID_ARG;
        }
        if (ts < last_ts) {
            LOG(ERROR, "timestamps must be monotonically increasing: %" PRId64 " < %" PRId64, ts, last_ts);
            return NGL_ERROR_INVALID_ARG;
        }
        last_ts = ts;
    }

    return 0;
}

static int streamedbuffer_init(struct ngl_node *node)
{
    struct streamedbuffer_priv *s = node->priv_data;
    const struct streamedbuffer_opts *o = node->opts;
    struct buffer_info *info = &s->buf;
    struct buffer_info *buffer_info = o->buffer_node->priv_data;
    struct buffer_layout *layout = &info->layout;

    *layout = buffer_info->layout;
    layout->count = o->count;

    if (layout->count == 0) {
        LOG(ERROR, "invalid number of elements (0)");
        return NGL_ERROR_INVALID_ARG;
    }

    if (buffer_info->layout.count % layout->count) {
        LOG(ERROR, "buffer count (%zu) is not a multiple of streamed buffer count (%zu)",
            buffer_info->layout.count, layout->count);
        return NGL_ERROR_INVALID_ARG;
    }

    info->data = buffer_info->data;
    info->data_size = buffer_info->data_size / layout->count;
    info->usage = buffer_info->usage;
    info->flags |= NGLI_BUFFER_INFO_FLAG_DYNAMIC;

    if (!o->timebase[1]) {
        LOG(ERROR, "invalid timebase: %d/%d", o->timebase[0], o->timebase[1]);
        return NGL_ERROR_INVALID_ARG;
    }

    int ret = check_timestamps_buffer(node);
    if (ret < 0)
        return ret;

    info->buffer = ngpu_buffer_create(node->ctx->gpu_ctx);
    if (!info->buffer)
        return NGL_ERROR_MEMORY;

    return 0;
}

static int streamedbuffer_prepare(struct ngl_node *node)
{
    struct streamedbuffer_priv *s = node->priv_data;
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

static void streamedbuffer_uninit(struct ngl_node *node)
{
    struct streamedbuffer_priv *s = node->priv_data;
    struct buffer_info *info = &s->buf;

    ngpu_buffer_freep(&info->buffer);
}

#define DECLARE_STREAMED_CLASS(class_id, class_name, class_suffix)          \
const struct node_class ngli_streamedbuffer##class_suffix##_class = {       \
    .id        = class_id,                                                  \
    .category  = NGLI_NODE_CATEGORY_BUFFER,                                 \
    .name      = class_name,                                                \
    .init      = streamedbuffer_init,                                       \
    .prepare   = streamedbuffer_prepare,                                    \
    .update    = streamedbuffer_update,                                     \
    .uninit    = streamedbuffer_uninit,                                     \
    .opts_size = sizeof(struct streamedbuffer_opts),                        \
    .priv_size = sizeof(struct streamedbuffer_priv),                        \
    .params    = streamedbuffer##class_suffix##_params,                     \
    .file      = __FILE__,                                                  \
};                                                                          \

DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERINT,    "StreamedBufferInt",    int)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERIVEC2,  "StreamedBufferIVec2",  ivec2)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERIVEC3,  "StreamedBufferIVec3",  ivec3)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERIVEC4,  "StreamedBufferIVec4",  ivec4)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERUINT,   "StreamedBufferUInt",   uint)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERUIVEC2, "StreamedBufferUIVec2", uivec2)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERUIVEC3, "StreamedBufferUIVec3", uivec3)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERUIVEC4, "StreamedBufferUIVec4", uivec4)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERFLOAT,  "StreamedBufferFloat",  float)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERVEC2,   "StreamedBufferVec2",   vec2)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERVEC3,   "StreamedBufferVec3",   vec3)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERVEC4,   "StreamedBufferVec4",   vec4)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDBUFFERMAT4,   "StreamedBufferMat4",   mat4)
