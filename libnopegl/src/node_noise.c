/*
 * Copyright 2021-2022 GoPro Inc.
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
#include <limits.h>

#include "internal.h"
#include "node_uniform.h"
#include "noise.h"
#include "nopegl.h"
#include "ngpu/type.h"

struct noise_opts {
    float frequency;
    struct noise_params generator_params;
};

struct noise_priv {
    struct variable_info var;
    float vector[4];
    struct noise generator[4];
};

const struct param_choices noise_func_choices = {
    .name = "interp_noise",
    .consts = {
        {"linear",  NGLI_NOISE_LINEAR,  .desc=NGLI_DOCSTRING("linear interpolation (not recommended), f(t)=t")},
        {"cubic",   NGLI_NOISE_CUBIC,   .desc=NGLI_DOCSTRING("cubic hermite curve, f(t)=3t²-2t³")},
        {"quintic", NGLI_NOISE_QUINTIC, .desc=NGLI_DOCSTRING("quintic curve, f(t)=6t⁵-15t⁴+10t³")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct noise_opts, x)
static const struct node_param noise_params[] = {
    {"frequency",   NGLI_PARAM_TYPE_F32, OFFSET(frequency), {.f32=1.f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("oscillation per second")},
    {"amplitude",   NGLI_PARAM_TYPE_F32, OFFSET(generator_params.amplitude), {.f32=1.f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("by how much it oscillates")},
    {"octaves",     NGLI_PARAM_TYPE_I32, OFFSET(generator_params.octaves), {.i32=3},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("number of accumulated noise layers (controls the level of details)")},
    {"lacunarity",  NGLI_PARAM_TYPE_F32, OFFSET(generator_params.lacunarity), {.f32=2.f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("frequency multiplier per octave")},
    {"gain",        NGLI_PARAM_TYPE_F32, OFFSET(generator_params.gain), {.f32=0.5f},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("amplitude multiplier per octave (also known as persistence)")},
    {"seed",        NGLI_PARAM_TYPE_U32, OFFSET(generator_params.seed), {.u32=0},
                    .desc=NGLI_DOCSTRING("random base seed (acts as an offsetting to the time)")},
    {"interpolant", NGLI_PARAM_TYPE_SELECT, OFFSET(generator_params.function), {.i32=NGLI_NOISE_QUINTIC},
                    .choices=&noise_func_choices,
                    .desc=NGLI_DOCSTRING("interpolation function to use between noise points")},
    {NULL}
};

NGLI_STATIC_ASSERT(variable_info_is_first, offsetof(struct noise_priv, var) == 0);

static int noisevec_update(struct ngl_node *node, double t, int n)
{
    struct noise_priv *s = node->priv_data;
    const struct noise_opts *o = node->opts;
    const float v = (float)(t * o->frequency);
    for (int i = 0; i < n; i++)
        s->vector[i] = ngli_noise_get(&s->generator[i], v);
    return 0;
}

static int noisefloat_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 1);
}

static int noisevec2_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 2);
}

static int noisevec3_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 3);
}

static int noisevec4_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 4);
}

static int init_noise_generators(struct noise_priv *s, const struct noise_opts *o, uint32_t n)
{
    /*
     * Every generator is instanciated the same, except for the seed: the seed
     * offset is defined to create a large gap between every components to keep
     * the overlap to the minimum possible
     */
    const uint32_t seed_offset = UINT32_MAX / n;
    uint32_t seed = o->generator_params.seed;
    for (int i = 0; i < n; i++) {
        struct noise_params np = o->generator_params;
        np.seed = seed;
        int ret = ngli_noise_init(&s->generator[i], &np);
        if (ret < 0)
            return ret;
        seed += seed_offset;
    }
    return 0;
}

#define DEFINE_NOISE_CLASS(class_id, class_name, type, dtype, count)        \
static int noise##type##_init(struct ngl_node *node)                        \
{                                                                           \
    struct noise_priv *s = node->priv_data;                                 \
    const struct noise_opts *o = node->opts;                                \
    s->var.data = s->vector;                                                \
    s->var.data_size = count * sizeof(float);                               \
    s->var.data_type = dtype;                                               \
    s->var.dynamic = 1;                                                     \
    return init_noise_generators(s, o, count);                              \
}                                                                           \
                                                                            \
const struct node_class ngli_noise##type##_class = {                        \
    .id        = class_id,                                                  \
    .category  = NGLI_NODE_CATEGORY_VARIABLE,                               \
    .name      = class_name,                                                \
    .init      = noise##type##_init,                                        \
    .update    = noise##type##_update,                                      \
    .opts_size = sizeof(struct noise_opts),                                 \
    .priv_size = sizeof(struct noise_priv),                                 \
    .params    = noise_params,                                              \
    .params_id = "Noise",                                                   \
    .file      = __FILE__,                                                  \
};

DEFINE_NOISE_CLASS(NGL_NODE_NOISEFLOAT, "NoiseFloat", float, NGPU_TYPE_F32,   1)
DEFINE_NOISE_CLASS(NGL_NODE_NOISEVEC2,  "NoiseVec2",  vec2,  NGPU_TYPE_VEC2,  2)
DEFINE_NOISE_CLASS(NGL_NODE_NOISEVEC3,  "NoiseVec3",  vec3,  NGPU_TYPE_VEC3,  3)
DEFINE_NOISE_CLASS(NGL_NODE_NOISEVEC4,  "NoiseVec4",  vec4,  NGPU_TYPE_VEC4,  4)
