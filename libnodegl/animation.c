/*
 * Copyright 2017-2019 GoPro Inc.
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
#include "animation.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "internal.h"

static int get_kf_id(struct ngl_node * const *animkf, int nb_animkf, int start, double t)
{
    int ret = -1;

    for (int i = start; i < nb_animkf; i++) {
        const struct animkeyframe_opts *kf = animkf[i]->opts;
        if (kf->time > t)
            break;
        ret = i;
    }
    return ret;
}

int ngli_animation_evaluate(struct animation *s, void *dst, double t)
{
    struct ngl_node * const *animkf = s->kfs;
    const int nb_animkf = s->nb_kfs;
    int kf_id = get_kf_id(animkf, nb_animkf, s->current_kf, t);
    if (kf_id < 0)
        kf_id = get_kf_id(animkf, nb_animkf, 0, t);
    if (kf_id >= 0 && kf_id < nb_animkf - 1) {
        const struct animkeyframe_priv *kf1_priv = animkf[kf_id + 1]->priv_data;
        const struct animkeyframe_opts *kf0 = animkf[kf_id    ]->opts;
        const struct animkeyframe_opts *kf1 = animkf[kf_id + 1]->opts;
        const double t0 = kf0->time;
        const double t1 = kf1->time;

        double tnorm = NGLI_LINEAR_INTERP(t0, t1, t);
        if (kf1_priv->scale_boundaries)
            tnorm = NGLI_MIX(kf1->offsets[0], kf1->offsets[1], tnorm);
        double ratio = kf1_priv->function(tnorm, kf1->nb_args, kf1->args);
        if (kf1_priv->scale_boundaries)
            ratio = NGLI_LINEAR_INTERP(kf1_priv->boundaries[0], kf1_priv->boundaries[1], ratio);

        s->current_kf = kf_id;
        s->mix_func(s->user_arg, dst, kf0, kf1, ratio);
    } else {
        const struct animkeyframe_opts *kf0 = animkf[            0]->opts;
        const struct animkeyframe_opts *kfn = animkf[nb_animkf - 1]->opts;
        const struct animkeyframe_opts *kf  = t < kf0->time ? kf0 : kfn;
        s->cpy_func(s->user_arg, dst, kf);
    }
    return 0;
}

int ngli_animation_derivate(struct animation *s, void *dst, double t)
{
    struct ngl_node * const *animkf = s->kfs;
    const int nb_animkf = s->nb_kfs;
    int kf_id = get_kf_id(animkf, nb_animkf, s->current_kf, t);
    if (kf_id < 0)
        kf_id = get_kf_id(animkf, nb_animkf, 0, t);
    if (kf_id >= 0 && kf_id < nb_animkf - 1) {
        const struct animkeyframe_priv *kf1_priv = animkf[kf_id + 1]->priv_data;
        const struct animkeyframe_opts *kf0 = animkf[kf_id    ]->opts;
        const struct animkeyframe_opts *kf1 = animkf[kf_id + 1]->opts;
        const double t0 = kf0->time;
        const double t1 = kf1->time;

        double tnorm = NGLI_LINEAR_INTERP(t0, t1, t);
        if (kf1_priv->scale_boundaries)
            tnorm = NGLI_MIX(kf1->offsets[0], kf1->offsets[1], tnorm);
        double ratio = kf1_priv->derivative(tnorm, kf1->nb_args, kf1->args);
        if (kf1_priv->scale_boundaries)
            ratio *= kf1_priv->derivative_scale;

        s->current_kf = kf_id;
        s->mix_func(s->user_arg, dst, kf0, kf1, ratio);
    } else {
        const struct animkeyframe_opts *kf0 = animkf[            0]->opts;
        const struct animkeyframe_opts *kfn = animkf[nb_animkf - 1]->opts;
        const struct animkeyframe_opts *kf  = t < kf0->time ? kf0 : kfn;
        s->cpy_func(s->user_arg, dst, kf);
    }
    return 0;
}

int ngli_animation_init(struct animation *s, void *user_arg,
                        struct ngl_node * const *kfs, int nb_kfs,
                        ngli_animation_mix_func_type mix_func,
                        ngli_animation_cpy_func_type cpy_func)
{
    if (nb_kfs < 1) {
        LOG(ERROR, "invalid number of animated key frames: %d", nb_kfs);
        return NGL_ERROR_INVALID_ARG;
    }

    s->user_arg = user_arg;

    ngli_assert(mix_func && cpy_func);
    s->mix_func = mix_func;
    s->cpy_func = cpy_func;

    double prev_time = -DBL_MAX;
    for (int i = 0; i < nb_kfs; i++) {
        const struct animkeyframe_opts *kf = kfs[i]->opts;

        if (kf->time < prev_time) {
            LOG(ERROR, "key frames must be monotonically increasing: %g < %g",
                kf->time, prev_time);
            return NGL_ERROR_INVALID_ARG;
        }
        prev_time = kf->time;
    }

    s->kfs = kfs;
    s->nb_kfs = nb_kfs;

    return 0;
}
