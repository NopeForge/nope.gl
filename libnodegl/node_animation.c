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

#include <string.h>
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct animation, x)
static const struct node_param animationscalar_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMESCALAR, -1}},
    {NULL}
};

static const struct node_param animationvec2_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC2, -1}},
    {NULL}
};

static const struct node_param animationvec3_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1}},
    {NULL}
};

static const struct node_param animationvec4_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC4, -1}},
    {NULL}
};

static int get_kf_id(struct ngl_node **animkf, int nb_animkf, int start, double t)
{
    int ret = -1;

    for (int i = start; i < nb_animkf; i++) {
        const struct animkeyframe *kf = animkf[i]->priv_data;
        if (kf->time >= t)
            break;
        ret = i;
    }
    return ret;
}

#define MIX(x, y, a) ((x)*(1.-(a)) + (y)*(a))

static inline void animation_update(struct animation *s, double t, int len)
{
    struct ngl_node **animkf = s->animkf;
    const int nb_animkf = s->nb_animkf;
    if (!nb_animkf)
        return;
    int kf_id = get_kf_id(animkf, nb_animkf, s->current_kf, t);
    float *dst = s->values;
    if (kf_id < 0)
        kf_id = get_kf_id(animkf, nb_animkf, 0, t);
    if (kf_id >= 0 && kf_id < nb_animkf - 1) {
        const struct animkeyframe *kf0 = animkf[kf_id    ]->priv_data;
        const struct animkeyframe *kf1 = animkf[kf_id + 1]->priv_data;
        const double t0 = kf0->time;
        const double t1 = kf1->time;
        const double tnorm = (t - t0) / (t1 - t0);
        const double ratio = kf1->function(tnorm, kf1->nb_args, kf1->args);
        s->current_kf = kf_id;
        if (len == 1)
            dst[0] = MIX(kf0->scalar, kf1->scalar, ratio);
        else
            for (int i = 0; i < len; i++)
                dst[i] = MIX(kf0->value[i], kf1->value[i], ratio);
    } else {
        const struct animkeyframe *kf0 = animkf[            0]->priv_data;
        const struct animkeyframe *kfn = animkf[nb_animkf - 1]->priv_data;
        const struct animkeyframe *kf  = t <= kf0->time ? kf0 : kfn;
        if (len == 1)
            dst[0] = kf->scalar;
        else
            memcpy(dst, kf->value, len * sizeof(*dst));
    }
}

#define UPDATE_FUNC(type, len)                                          \
static void animation##type##_update(struct ngl_node *node, double t)   \
{                                                                       \
    animation_update(node->priv_data, t, len);                          \
}

UPDATE_FUNC(scalar, 1);
UPDATE_FUNC(vec2,   2);
UPDATE_FUNC(vec3,   3);
UPDATE_FUNC(vec4,   4);

const struct node_class ngli_animationscalar_class = {
    .id        = NGL_NODE_ANIMATIONSCALAR,
    .name      = "AnimationScalar",
    .update    = animationscalar_update,
    .priv_size = sizeof(struct animation),
    .params    = animationscalar_params,
};

const struct node_class ngli_animationvec2_class = {
    .id        = NGL_NODE_ANIMATIONVEC2,
    .name      = "AnimationVec2",
    .update    = animationvec2_update,
    .priv_size = sizeof(struct animation),
    .params    = animationvec2_params,
};

const struct node_class ngli_animationvec3_class = {
    .id        = NGL_NODE_ANIMATIONVEC3,
    .name      = "AnimationVec3",
    .update    = animationvec3_update,
    .priv_size = sizeof(struct animation),
    .params    = animationvec3_params,
};

const struct node_class ngli_animationvec4_class = {
    .id        = NGL_NODE_ANIMATIONVEC4,
    .name      = "AnimationVec4",
    .update    = animationvec4_update,
    .priv_size = sizeof(struct animation),
    .params    = animationvec4_params,
};
