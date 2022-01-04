/*
 * Copyright 2021 GoPro Inc.
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

#include "darray.h"
#include "filterschain.h"
#include "internal.h"
#include "type.h"

/* GLSL filters as string */
#include "filter_alpha.h"
#include "filter_contrast.h"
#include "filter_exposure.h"
#include "filter_inversealpha.h"
#include "filter_linear2srgb.h"
#include "filter_opacity.h"
#include "filter_premult.h"
#include "filter_saturation.h"
#include "filter_srgb2linear.h"

struct filteralpha_priv {
    struct filter filter;
    struct ngl_node *alpha_node;
    float alpha;
};

struct filtercontrast_priv {
    struct filter filter;
    struct ngl_node *contrast_node;
    float contrast;
    struct ngl_node *pivot_node;
    float pivot;
};

struct filterexposure_priv {
    struct filter filter;
    struct ngl_node *exposure_node;
    float exposure;
};

struct filterinversealpha_priv {
    struct filter filter;
};

struct filterlinear2srgb_priv {
    struct filter filter;
};

struct filteropacity_priv {
    struct filter filter;
    struct ngl_node *opacity_node;
    float opacity;
};

struct filterpremult_priv {
    struct filter filter;
};

struct filtersaturation_priv {
    struct filter filter;
    struct ngl_node *saturation_node;
    float saturation;
};

struct filtersrgb2linear_priv {
    struct filter filter;
};

/* struct filter must be on top of each context because that's how the private
 * data is read externally and in the params below */
NGLI_STATIC_ASSERT(filter_on_top_of_alpha_priv,         offsetof(struct filteralpha_priv,         filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_contrast_priv,      offsetof(struct filtercontrast_priv,      filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_exposure_priv,      offsetof(struct filterexposure_priv,      filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_inversealpha_priv,  offsetof(struct filterinversealpha_priv,  filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_linear2srgb_priv,   offsetof(struct filterlinear2srgb_priv,   filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_opacity_priv,       offsetof(struct filteropacity_priv,       filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_premult_priv,       offsetof(struct filterpremult_priv,       filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_saturation_priv,    offsetof(struct filtersaturation_priv,    filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_srgb2linear_priv,   offsetof(struct filtersrgb2linear_priv,   filter) == 0);

#define OFFSET(x) offsetof(struct filteralpha_priv, x)
static const struct node_param filteralpha_params[] = {
    {"alpha", NGLI_PARAM_TYPE_F32, OFFSET(alpha_node), {.f32=1.f},
              .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
              .desc=NGLI_DOCSTRING("alpha channel value")},
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct filtercontrast_priv, x)
static const struct node_param filtercontrast_params[] = {
    {"contrast",  NGLI_PARAM_TYPE_F32, OFFSET(contrast_node), {.f32=1.f},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("perceptual contrast value")},
    {"pivot",     NGLI_PARAM_TYPE_F32, OFFSET(pivot_node), {.f32=.5f},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("pivot point between light and dark")},
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct filterexposure_priv, x)
static const struct node_param filterexposure_params[] = {
    {"exposure", NGLI_PARAM_TYPE_F32, OFFSET(exposure_node),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("exposure")},
    {NULL}
};
#undef OFFSET

#define filterinversealpha_params NULL
#define filterlinear2srgb_params NULL

#define OFFSET(x) offsetof(struct filteropacity_priv, x)
static const struct node_param filteropacity_params[] = {
    {"opacity", NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=1.f},
              .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
              .desc=NGLI_DOCSTRING("opacity value (color gets premultiplied by this value)")},
    {NULL}
};
#undef OFFSET

#define filterpremult_params NULL

#define OFFSET(x) offsetof(struct filtersaturation_priv, x)
static const struct node_param filtersaturation_params[] = {
    {"saturation", NGLI_PARAM_TYPE_F32, OFFSET(saturation_node), {.f32=1.f},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("saturation")},
    {NULL}
};
#undef OFFSET

#define filtersrgb2linear_params NULL

static int register_resource(struct darray *resources, const char *name,
                             struct ngl_node *pnode, void *data, int data_type)
{
    if (pnode) {
        struct variable_priv *var = pnode->priv_data;
        ngli_assert(var->data_type == data_type);
        data = var->data;
    }
    struct pgcraft_uniform res = {.type=data_type, .stage=NGLI_PROGRAM_SHADER_FRAG, .data=data};
    snprintf(res.name, sizeof(res.name), "%s", name);
    if (!ngli_darray_push(resources, &res))
        return NGL_ERROR_MEMORY;
    return 0;
}

static int filter_init(struct ngl_node *node)
{
    struct filter *s = node->priv_data;
    ngli_darray_init(&s->resources, sizeof(struct pgcraft_uniform), 0);
    return 0;
}

static int filteralpha_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filteralpha_priv *s = node->priv_data;
    s->filter.name = "alpha";
    s->filter.code = filter_alpha_glsl;
    return register_resource(&s->filter.resources, "alpha", s->alpha_node, &s->alpha, NGLI_TYPE_FLOAT);
}

static int filtercontrast_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filtercontrast_priv *s = node->priv_data;
    s->filter.name = "contrast";
    s->filter.code = filter_contrast_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS;
    if ((ret = register_resource(&s->filter.resources, "contrast", s->contrast_node, &s->contrast, NGLI_TYPE_FLOAT)) < 0 ||
        (ret = register_resource(&s->filter.resources, "pivot", s->pivot_node, &s->pivot, NGLI_TYPE_FLOAT)) < 0)
        return ret;
    return 0;
}

static int filterexposure_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filterexposure_priv *s = node->priv_data;
    s->filter.name = "exposure";
    s->filter.code = filter_exposure_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS;
    return register_resource(&s->filter.resources, "exposure", s->exposure_node, &s->exposure, NGLI_TYPE_FLOAT);
}

static int filterinversealpha_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filterinversealpha_priv *s = node->priv_data;
    s->filter.name = "inversealpha";
    s->filter.code = filter_inversealpha_glsl;
    return 0;
}

static int filterlinear2srgb_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filterlinear2srgb_priv *s = node->priv_data;
    s->filter.name = "linear2srgb";
    s->filter.code = filter_linear2srgb_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_LINEAR2SRGB;
    return 0;
}

static int filteropacity_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filteropacity_priv *s = node->priv_data;
    s->filter.name = "opacity";
    s->filter.code = filter_opacity_glsl;
    return register_resource(&s->filter.resources, "opacity", s->opacity_node, &s->opacity, NGLI_TYPE_FLOAT);
}

static int filterpremult_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filterpremult_priv *s = node->priv_data;
    s->filter.name = "premult";
    s->filter.code = filter_premult_glsl;
    return 0;
}

static int filtersaturation_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filtersaturation_priv *s = node->priv_data;
    s->filter.name = "saturation";
    s->filter.code = filter_saturation_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS;
    return register_resource(&s->filter.resources, "saturation", s->saturation_node, &s->saturation, NGLI_TYPE_FLOAT);
}

static int filtersrgb2linear_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filtersrgb2linear_priv *s = node->priv_data;
    s->filter.name = "srgb2linear";
    s->filter.code = filter_srgb2linear_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_SRGB2LINEAR;
    return 0;
}

static void filter_uninit(struct ngl_node *node)
{
    struct filter *s = node->priv_data;
    ngli_darray_reset(&s->resources);
}

#define DECLARE_FILTER(type, cls_id, cls_name)          \
const struct node_class ngli_filter##type##_class = {   \
    .id        = cls_id,                                \
    .name      = cls_name,                              \
    .init      = filter##type##_init,                   \
    .update    = ngli_node_update_children,             \
    .uninit    = filter_uninit,                         \
    .priv_size = sizeof(struct filter##type##_priv),    \
    .params    = filter##type##_params,                 \
    .file      = __FILE__,                              \
};

DECLARE_FILTER(alpha,         NGL_NODE_FILTERALPHA,         "FilterAlpha")
DECLARE_FILTER(contrast,      NGL_NODE_FILTERCONTRAST,      "FilterContrast")
DECLARE_FILTER(exposure,      NGL_NODE_FILTEREXPOSURE,      "FilterExposure")
DECLARE_FILTER(inversealpha,  NGL_NODE_FILTERINVERSEALPHA,  "FilterInverseAlpha")
DECLARE_FILTER(linear2srgb,   NGL_NODE_FILTERLINEAR2SRGB,   "FilterLinear2sRGB")
DECLARE_FILTER(opacity,       NGL_NODE_FILTEROPACITY,       "FilterOpacity")
DECLARE_FILTER(premult,       NGL_NODE_FILTERPREMULT,       "FilterPremult")
DECLARE_FILTER(saturation,    NGL_NODE_FILTERSATURATION,    "FilterSaturation")
DECLARE_FILTER(srgb2linear,   NGL_NODE_FILTERSRGB2LINEAR,   "FilterSRGB2Linear")
