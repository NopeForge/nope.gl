/*
 * Copyright 2017 GoPro Inc.
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
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct animation_priv, x)
static const struct node_param animatedfloat_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEFLOAT, -1},
                  .desc=NGLI_DOCSTRING("float key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedvec2_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC2, -1},
                  .desc=NGLI_DOCSTRING("vec2 key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedvec3_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1},
                  .desc=NGLI_DOCSTRING("vec3 key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedvec4_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC4, -1},
                  .desc=NGLI_DOCSTRING("vec4 key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedquat_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEQUAT, -1},
                  .desc=NGLI_DOCSTRING("quaternion key frames to interpolate from")},
    {NULL}
};

static int get_kf_id(struct ngl_node **animkf, int nb_animkf, int start, double t)
{
    int ret = -1;

    for (int i = start; i < nb_animkf; i++) {
        const struct animkeyframe_priv *kf = animkf[i]->priv_data;
        if (kf->time > t)
            break;
        ret = i;
    }
    return ret;
}

#define MIX(x, y, a) ((x)*(1.-(a)) + (y)*(a))

static inline int animation_update(const struct animation_priv *s, double t, int len,
                                   void *dst, int *cache)
{
    struct ngl_node **animkf = s->animkf;
    const int nb_animkf = s->nb_animkf;
    if (!nb_animkf)
        return 0;
    int kf_id = get_kf_id(animkf, nb_animkf, *cache, t);
    if (kf_id < 0)
        kf_id = get_kf_id(animkf, nb_animkf, 0, t);
    if (kf_id >= 0 && kf_id < nb_animkf - 1) {
        const struct animkeyframe_priv *kf0 = animkf[kf_id    ]->priv_data;
        const struct animkeyframe_priv *kf1 = animkf[kf_id + 1]->priv_data;
        const double t0 = kf0->time;
        const double t1 = kf1->time;

        double tnorm = (t - t0) / (t1 - t0);
        if (kf1->scale_boundaries)
            tnorm = (kf1->offsets[1] - kf1->offsets[0]) * tnorm + kf1->offsets[0];
        double ratio = kf1->function(tnorm, kf1->nb_args, kf1->args);
        if (kf1->scale_boundaries)
            ratio = (ratio - kf1->boundaries[0]) / (kf1->boundaries[1] - kf1->boundaries[0]);

        *cache = kf_id;
        if (len == 1) { /* scalar */
            ((double *)dst)[0] = MIX(kf0->scalar, kf1->scalar, ratio);
        } else if (len == 5) { /* quaternion */
            ngli_quat_slerp(dst, kf0->value, kf1->value, ratio);
        } else { /* vector */
            for (int i = 0; i < len; i++)
                ((float *)dst)[i] = MIX(kf0->value[i], kf1->value[i], ratio);
        }
    } else {
        const struct animkeyframe_priv *kf0 = animkf[            0]->priv_data;
        const struct animkeyframe_priv *kfn = animkf[nb_animkf - 1]->priv_data;
        const struct animkeyframe_priv *kf  = t < kf0->time ? kf0 : kfn;
        if (len == 1) /* scalar */
            memcpy(dst, &kf->scalar, sizeof(double));
        else if (len == 5) /* quaternion */
            memcpy(dst, kf->value, 4 * sizeof(float));
        else /* vector */
            memcpy(dst, kf->value, len * sizeof(float));
    }
    return 0;
}

int ngl_anim_evaluate(struct ngl_node *node, void *dst, double t)
{
    struct animation_priv *s = node->priv_data;
    if (!s->nb_animkf)
        return -1;

    struct animkeyframe_priv *kf0 = s->animkf[0]->priv_data;
    if (!kf0->function) {
        for (int i = 0; i < s->nb_animkf; i++) {
            int ret = s->animkf[i]->class->init(s->animkf[i]);
            if (ret < 0)
                return ret;
        }
    }

    const int len = node->class->id - NGL_NODE_ANIMATEDFLOAT + 1;
    return animation_update(s, t, len, dst, &s->eval_current_kf);
}

static int animation_init(struct ngl_node *node)
{
    struct animation_priv *s = node->priv_data;
    double prev_time = -DBL_MAX;

    for (int i = 0; i < s->nb_animkf; i++) {
        const struct animkeyframe_priv *kf = s->animkf[i]->priv_data;

        if (kf->time < prev_time) {
            LOG(ERROR, "key frames must be monotically increasing: %g < %g",
                kf->time, prev_time);
            return -1;
        }
        prev_time = kf->time;
    }

    return 0;
}

static int animatedfloat_update(struct ngl_node *node, double t)
{
    struct animation_priv *s = node->priv_data;
    return animation_update(s, t, 1, &s->scalar, &s->current_kf);
}

#define UPDATE_FUNC(type, len)                                          \
static int animated##type##_update(struct ngl_node *node, double t)     \
{                                                                       \
    struct animation_priv *s = node->priv_data;                         \
    return animation_update(s, t, len, s->values, &s->current_kf);      \
}

UPDATE_FUNC(vec2,   2);
UPDATE_FUNC(vec3,   3);
UPDATE_FUNC(vec4,   4);
UPDATE_FUNC(quat,   5); /* quaternion (4) + slerp (1) */

const struct node_class ngli_animatedfloat_class = {
    .id        = NGL_NODE_ANIMATEDFLOAT,
    .name      = "AnimatedFloat",
    .init      = animation_init,
    .update    = animatedfloat_update,
    .priv_size = sizeof(struct animation_priv),
    .params    = animatedfloat_params,
    .file      = __FILE__,
};

const struct node_class ngli_animatedvec2_class = {
    .id        = NGL_NODE_ANIMATEDVEC2,
    .name      = "AnimatedVec2",
    .init      = animation_init,
    .update    = animatedvec2_update,
    .priv_size = sizeof(struct animation_priv),
    .params    = animatedvec2_params,
    .file      = __FILE__,
};

const struct node_class ngli_animatedvec3_class = {
    .id        = NGL_NODE_ANIMATEDVEC3,
    .name      = "AnimatedVec3",
    .init      = animation_init,
    .update    = animatedvec3_update,
    .priv_size = sizeof(struct animation_priv),
    .params    = animatedvec3_params,
    .file      = __FILE__,
};

const struct node_class ngli_animatedvec4_class = {
    .id        = NGL_NODE_ANIMATEDVEC4,
    .name      = "AnimatedVec4",
    .init      = animation_init,
    .update    = animatedvec4_update,
    .priv_size = sizeof(struct animation_priv),
    .params    = animatedvec4_params,
    .file      = __FILE__,
};

const struct node_class ngli_animatedquat_class = {
    .id        = NGL_NODE_ANIMATEDQUAT,
    .name      = "AnimatedQuat",
    .init      = animation_init,
    .update    = animatedquat_update,
    .priv_size = sizeof(struct animation_priv),
    .params    = animatedquat_params,
    .file      = __FILE__,
};
