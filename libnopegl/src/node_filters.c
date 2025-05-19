/*
 * Copyright 2022 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Forge
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

#include "filterschain.h"
#include "internal.h"
#include "log.h"
#include "ngpu/type.h"
#include "node_colorkey.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "ngpu/pgcraft.h"
#include "utils/bstr.h"
#include "utils/darray.h"
#include "utils/memory.h"

/* GLSL filters as string */
#include "filter_alpha.h"
#include "filter_contrast.h"
#include "filter_exposure.h"
#include "filter_inversealpha.h"
#include "filter_linear2srgb.h"
#include "filter_opacity.h"
#include "filter_premult.h"
#include "filter_saturation.h"
#include "filter_selector.h"
#include "filter_srgb2linear.h"

struct filteralpha_opts {
    struct ngl_node *alpha_node;
    float alpha;
};

struct filteralpha_priv {
    struct filter filter;
};

struct filtercolormap_opts {
    struct ngl_node **colorkeys;
    size_t nb_colorkeys;
};

struct filtercolormap_priv {
    struct filter filter;
};

struct filtercontrast_opts {
    struct ngl_node *contrast_node;
    float contrast;
    struct ngl_node *pivot_node;
    float pivot;
};

struct filtercontrast_priv {
    struct filter filter;
};

struct filterexposure_opts {
    struct ngl_node *exposure_node;
    float exposure;
};

struct filterexposure_priv {
    struct filter filter;
};

struct filterinversealpha_priv {
    struct filter filter;
};

struct filterlinear2srgb_priv {
    struct filter filter;
};

struct filteropacity_opts {
    struct ngl_node *opacity_node;
    float opacity;
};

struct filteropacity_priv {
    struct filter filter;
};

struct filterpremult_priv {
    struct filter filter;
};

struct filtersaturation_opts {
    struct ngl_node *saturation_node;
    float saturation;
};

struct filtersaturation_priv {
    struct filter filter;
};

struct filterselector_opts {
    struct ngl_node *range_node;
    float range[2];
    int component;
    int drop_mode;
    int output_mode;
    struct ngl_node *smoothedges_node;
    int smoothedges;
};

struct filterselector_priv {
    struct filter filter;
};

struct filtersrgb2linear_priv {
    struct filter filter;
};

/* struct filter must be on top of each context because that's how the private
 * data is read externally and in the params below */
NGLI_STATIC_ASSERT(filter_on_top_of_alpha_priv,         offsetof(struct filteralpha_priv,         filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_colormap_priv,      offsetof(struct filtercolormap_priv,      filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_contrast_priv,      offsetof(struct filtercontrast_priv,      filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_exposure_priv,      offsetof(struct filterexposure_priv,      filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_inversealpha_priv,  offsetof(struct filterinversealpha_priv,  filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_linear2srgb_priv,   offsetof(struct filterlinear2srgb_priv,   filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_opacity_priv,       offsetof(struct filteropacity_priv,       filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_premult_priv,       offsetof(struct filterpremult_priv,       filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_saturation_priv,    offsetof(struct filtersaturation_priv,    filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_selector_priv,      offsetof(struct filterselector_priv,      filter) == 0);
NGLI_STATIC_ASSERT(filter_on_top_of_srgb2linear_priv,   offsetof(struct filtersrgb2linear_priv,   filter) == 0);

#define OFFSET(x) offsetof(struct filteralpha_opts, x)
static const struct node_param filteralpha_params[] = {
    {"alpha", NGLI_PARAM_TYPE_F32, OFFSET(alpha_node), {.f32=1.f},
              .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
              .desc=NGLI_DOCSTRING("alpha channel value")},
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct filtercolormap_opts, x)
static const struct node_param filtercolormap_params[] = {
    {"colorkeys", NGLI_PARAM_TYPE_NODELIST, OFFSET(colorkeys),
                  .node_types=(const uint32_t[]){NGL_NODE_COLORKEY, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("color keys to interpolate from")},
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct filtercontrast_opts, x)
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

#define OFFSET(x) offsetof(struct filterexposure_opts, x)
static const struct node_param filterexposure_params[] = {
    {"exposure", NGLI_PARAM_TYPE_F32, OFFSET(exposure_node),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                 .desc=NGLI_DOCSTRING("exposure")},
    {NULL}
};
#undef OFFSET

#define filterinversealpha_params NULL
#define filterlinear2srgb_params NULL

#define OFFSET(x) offsetof(struct filteropacity_opts, x)
static const struct node_param filteropacity_params[] = {
    {"opacity", NGLI_PARAM_TYPE_F32, OFFSET(opacity_node), {.f32=1.f},
              .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
              .desc=NGLI_DOCSTRING("opacity value (color gets premultiplied by this value)")},
    {NULL}
};
#undef OFFSET

#define filterpremult_params NULL

#define OFFSET(x) offsetof(struct filtersaturation_opts, x)
static const struct node_param filtersaturation_params[] = {
    {"saturation", NGLI_PARAM_TYPE_F32, OFFSET(saturation_node), {.f32=1.f},
                   .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                   .desc=NGLI_DOCSTRING("saturation")},
    {NULL}
};
#undef OFFSET

enum { /* values are explicit to help shader readibility */
    SELECTOR_COMPONENT_LIGHTNESS = 0,
    SELECTOR_COMPONENT_CHROMA    = 1,
    SELECTOR_COMPONENT_HUE       = 2,
};

static const struct param_choices selector_component_choices = {
    .name = "selector_component",
    .consts = {
        {"lightness", SELECTOR_COMPONENT_LIGHTNESS, .desc=NGLI_DOCSTRING("lightness component from OkLCH (within [0,1])")},
        {"chroma",    SELECTOR_COMPONENT_CHROMA,    .desc=NGLI_DOCSTRING("chroma component from OkLCH (infinite upper boundary, but in practice within [0,0.4])")},
        {"hue",       SELECTOR_COMPONENT_HUE,       .desc=NGLI_DOCSTRING("hue component from OkLCH (circular value in radian)")},
        {NULL}
    }
};

enum { /* values are explicit to help shader readibility */
    SELECTOR_DROP_OUTSIDE = 0,
    SELECTOR_DROP_INSIDE  = 1,
};

static const struct param_choices selector_drop_choices = {
    .name = "selector_drop",
    .consts = {
        {"outside", SELECTOR_DROP_OUTSIDE, .desc=NGLI_DOCSTRING("drop if value is outside the range")},
        {"inside",  SELECTOR_DROP_INSIDE,  .desc=NGLI_DOCSTRING("drop if value is inside the range")},
        {NULL}
    }
};

enum { /* values are explicit to help shader readibility */
    SELECTOR_OUTPUT_COLORHOLES = 0,
    SELECTOR_OUTPUT_BINARY     = 1,
};

static const struct param_choices selector_output_choices = {
    .name = "selector_output",
    .consts = {
        {"colorholes", SELECTOR_OUTPUT_COLORHOLES, .desc=NGLI_DOCSTRING("replace the selected colors with `(0,0,0,0)`")},
        {"binary",     SELECTOR_OUTPUT_BINARY,     .desc=NGLI_DOCSTRING("same as `colorholes` but non-selected colors become `(1,1,1,1)`")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct filterselector_opts, x)
static const struct node_param filterselector_params[] = {
    {"range",        NGLI_PARAM_TYPE_VEC2, OFFSET(range_node), {.vec={0.f, 1.f}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("values within this range are selected")},
    {"component",    NGLI_PARAM_TYPE_SELECT, OFFSET(component), {.i32=SELECTOR_COMPONENT_LIGHTNESS},
                     .choices=&selector_component_choices,
                     .desc=NGLI_DOCSTRING("reference component for the selector comparison")},
    {"drop_mode",    NGLI_PARAM_TYPE_SELECT, OFFSET(drop_mode), {.i32=SELECTOR_DROP_OUTSIDE},
                     .choices=&selector_drop_choices,
                     .desc=NGLI_DOCSTRING("define how to interpret the `range` selector")},
    {"output_mode",  NGLI_PARAM_TYPE_SELECT, OFFSET(output_mode), {.i32=SELECTOR_OUTPUT_COLORHOLES},
                     .choices=&selector_output_choices,
                     .desc=NGLI_DOCSTRING("define the output color")},
    {"smoothedges",  NGLI_PARAM_TYPE_BOOL, OFFSET(smoothedges_node), {.i32=0},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                     .desc=NGLI_DOCSTRING("make edges less sharp")},
    {NULL}
};
#undef OFFSET

#define filtersrgb2linear_params NULL

static int register_resource(struct darray *resources, const char *name,
                             const struct ngl_node *pnode, void *data, enum ngpu_type data_type)
{
    struct ngpu_pgcraft_uniform res = {
        .type  = data_type,
        .stage = NGPU_PROGRAM_SHADER_FRAG,
        .data  = ngli_node_get_data_ptr(pnode, data),
    };
    snprintf(res.name, sizeof(res.name), "%s", name);
    if (!ngli_darray_push(resources, &res))
        return NGL_ERROR_MEMORY;
    return 0;
}

static int filter_init(struct ngl_node *node)
{
    struct filter *s = node->priv_data;
    ngli_darray_init(&s->resources, sizeof(struct ngpu_pgcraft_uniform), 0);
    return 0;
}

static int filteralpha_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filteralpha_priv *s = node->priv_data;
    struct filteralpha_opts *o = node->opts;
    s->filter.name = "alpha";
    s->filter.code = filter_alpha_glsl;
    return register_resource(&s->filter.resources, "alpha", o->alpha_node, &o->alpha, NGPU_TYPE_F32);
}

static int filtercolormap_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;

    struct filtercolormap_priv *s = node->priv_data;
    struct filtercolormap_opts *o = node->opts;

    if (o->nb_colorkeys < 2) {
        LOG(ERROR, "a minimum of 2 color keys is required to make a color map");
        return NGL_ERROR_INVALID_ARG;
    }

    s->filter.name = "colormap";
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS | NGLI_FILTER_HELPER_SRGB;

    /* Craft the shader dynamically according to the number of color keys */
    struct bstr *str = ngli_bstr_create();
    if (!str) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    #define I "    "

    /* Prototype and initial declarations */
    ngli_bstr_print(str, "vec4 filter_colormap(vec4 color, vec2 coords");
    for (size_t i = 0; i < o->nb_colorkeys; i++)
        ngli_bstr_printf(str, ", float pos%zu, vec3 color%zu, float opacity%zu", i, i, i);
    ngli_bstr_print(str, ")\n{\n"
                         I "float t_prv = 1.0, t_nxt = 0.0;\n"
                         I "vec4 v_prv, v_nxt;\n"
                         "\n");

    /* Convert input color to grayscale to obtain the interpolation index */
    ngli_bstr_print(str, I "float t = dot(color.rgb, ngli_luma_weights);\n\n");

    /* Switch colors to linear space and saturate pos within [0,1] */
    for (size_t i = 0; i < o->nb_colorkeys; i++) {
        ngli_bstr_printf(str, I "pos%zu = ngli_sat(pos%zu);\n", i, i);
        ngli_bstr_printf(str, I "color%zu = ngli_srgb2linear(color%zu);\n", i, i);
    }

    /* Identify left-most and right-most knots */
    for (size_t i = 0; i < o->nb_colorkeys; i++) {
        /* The '=' are here to make sure we enter in the loop at least once */
        ngli_bstr_printf(str, I "if (pos%zu <= t_prv) { t_prv = pos%zu; v_prv = vec4(color%zu, opacity%zu); }\n", i, i, i, i);
        ngli_bstr_printf(str, I "if (pos%zu >= t_nxt) { t_nxt = pos%zu; v_nxt = vec4(color%zu, opacity%zu); }\n", i, i, i, i);
    }

    /* Identify the two closest surrounding knots (if any) */
    for (size_t i = 0; i < o->nb_colorkeys; i++) {
        /* The '=' are here to make sure we honor the knot if we are exactly on it */
        ngli_bstr_printf(str, I "if (pos%zu <= t && pos%zu > t_prv) { t_prv = pos%zu; v_prv = vec4(color%zu, opacity%zu); }\n", i, i, i, i, i);
        ngli_bstr_printf(str, I "if (pos%zu >= t && pos%zu < t_nxt) { t_nxt = pos%zu; v_nxt = vec4(color%zu, opacity%zu); }\n", i, i, i, i, i);
    }

    /* Final interpolation: simple hermite spline */
    ngli_bstr_print(str, I "vec4 ret = mix(v_prv, v_nxt, smoothstep(t_prv, t_nxt, t));\n"
                         I "return vec4(ngli_linear2srgb(ret.rgb), 1.0) * ret.a;\n"
                         "}\n");

    s->filter.code = ngli_bstr_strdup(str);
    if (!s->filter.code) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    char pos_name[64], color_name[64], opacity_name[64];

    for (size_t i = 0; i < o->nb_colorkeys; i++) {
        struct ngl_node *colorkey = o->colorkeys[i];
        struct colorkey_opts *key_o = colorkey->opts;

        snprintf(pos_name,     sizeof(pos_name),     "pos%zu", i);
        snprintf(color_name,   sizeof(color_name),   "color%zu",    i);
        snprintf(opacity_name, sizeof(opacity_name), "opacity%zu",  i);

        if ((ret = register_resource(&s->filter.resources, pos_name,     key_o->position_node, &key_o->position, NGPU_TYPE_F32))  < 0 ||
            (ret = register_resource(&s->filter.resources, color_name,   key_o->color_node,    key_o->color,     NGPU_TYPE_VEC3)) < 0 ||
            (ret = register_resource(&s->filter.resources, opacity_name, key_o->opacity_node,  &key_o->opacity,  NGPU_TYPE_F32))  < 0)
            goto end;
    }

end:
    ngli_bstr_freep(&str);

    return ret;
}

static int filtercontrast_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filtercontrast_priv *s = node->priv_data;
    struct filtercontrast_opts *o = node->opts;
    s->filter.name = "contrast";
    s->filter.code = filter_contrast_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS;
    if ((ret = register_resource(&s->filter.resources, "contrast", o->contrast_node, &o->contrast, NGPU_TYPE_F32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "pivot", o->pivot_node, &o->pivot, NGPU_TYPE_F32)) < 0)
        return ret;
    return 0;
}

static int filterexposure_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filterexposure_priv *s = node->priv_data;
    struct filterexposure_opts *o = node->opts;
    s->filter.name = "exposure";
    s->filter.code = filter_exposure_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS;
    return register_resource(&s->filter.resources, "exposure", o->exposure_node, &o->exposure, NGPU_TYPE_F32);
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
    s->filter.helpers = NGLI_FILTER_HELPER_SRGB;
    return 0;
}

static int filteropacity_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filteropacity_priv *s = node->priv_data;
    struct filteropacity_opts *o = node->opts;
    s->filter.name = "opacity";
    s->filter.code = filter_opacity_glsl;
    return register_resource(&s->filter.resources, "opacity", o->opacity_node, &o->opacity, NGPU_TYPE_F32);
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
    struct filtersaturation_opts *o = node->opts;
    s->filter.name = "saturation";
    s->filter.code = filter_saturation_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS | NGLI_FILTER_HELPER_SRGB;
    return register_resource(&s->filter.resources, "saturation", o->saturation_node, &o->saturation, NGPU_TYPE_F32);
}

static int filterselector_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filterselector_priv *s = node->priv_data;
    struct filterselector_opts *o = node->opts;
    s->filter.name = "selector";
    s->filter.code = filter_selector_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_MISC_UTILS | NGLI_FILTER_HELPER_SRGB | NGLI_FILTER_HELPER_OKLAB;
    if ((ret = register_resource(&s->filter.resources, "range", o->range_node, &o->range, NGPU_TYPE_VEC2)) < 0 ||
        (ret = register_resource(&s->filter.resources, "component", NULL, &o->component, NGPU_TYPE_I32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "drop_mode", NULL, &o->drop_mode, NGPU_TYPE_I32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "output_mode", NULL, &o->output_mode, NGPU_TYPE_I32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "smoothedges", o->smoothedges_node, &o->smoothedges, NGPU_TYPE_I32)) < 0)
        return ret;
    return 0;
}

static int filtersrgb2linear_init(struct ngl_node *node)
{
    int ret = filter_init(node);
    if (ret < 0)
        return ret;
    struct filtersrgb2linear_priv *s = node->priv_data;
    s->filter.name = "srgb2linear";
    s->filter.code = filter_srgb2linear_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_SRGB;
    return 0;
}

static void filter_uninit(struct ngl_node *node)
{
    struct filter *s = node->priv_data;
    ngli_darray_reset(&s->resources);
}

static void filtercolormap_uninit(struct ngl_node *node)
{
    struct filter *s = node->priv_data;
    ngli_freep(&s->code);
    filter_uninit(node);
}

const struct node_class ngli_filtercolormap_class = {   \
    .id        = NGL_NODE_FILTERCOLORMAP,               \
    .name      = "FilterColorMap",                      \
    .init      = filtercolormap_init,                   \
    .update    = ngli_node_update_children,             \
    .uninit    = filtercolormap_uninit,                 \
    .opts_size = sizeof(struct filtercolormap_opts),    \
    .priv_size = sizeof(struct filtercolormap_priv),    \
    .params    = filtercolormap_params,                 \
    .file      = __FILE__,                              \
};

#define DECLARE_FILTER(type, cls_id, cls_opts_size, cls_name) \
const struct node_class ngli_filter##type##_class = {   \
    .id        = cls_id,                                \
    .name      = cls_name,                              \
    .init      = filter##type##_init,                   \
    .update    = ngli_node_update_children,             \
    .uninit    = filter_uninit,                         \
    .opts_size = cls_opts_size,                         \
    .priv_size = sizeof(struct filter##type##_priv),    \
    .params    = filter##type##_params,                 \
    .file      = __FILE__,                              \
};

DECLARE_FILTER(alpha,        NGL_NODE_FILTERALPHA,        sizeof(struct filteralpha_opts),      "FilterAlpha")
DECLARE_FILTER(contrast,     NGL_NODE_FILTERCONTRAST,     sizeof(struct filtercontrast_opts),   "FilterContrast")
DECLARE_FILTER(exposure,     NGL_NODE_FILTEREXPOSURE,     sizeof(struct filterexposure_opts),   "FilterExposure")
DECLARE_FILTER(inversealpha, NGL_NODE_FILTERINVERSEALPHA, 0,                                    "FilterInverseAlpha")
DECLARE_FILTER(linear2srgb,  NGL_NODE_FILTERLINEAR2SRGB,  0,                                    "FilterLinear2sRGB")
DECLARE_FILTER(opacity,      NGL_NODE_FILTEROPACITY,      sizeof(struct filteropacity_opts),    "FilterOpacity")
DECLARE_FILTER(premult,      NGL_NODE_FILTERPREMULT,      0,                                    "FilterPremult")
DECLARE_FILTER(saturation,   NGL_NODE_FILTERSATURATION,   sizeof(struct filtersaturation_opts), "FilterSaturation")
DECLARE_FILTER(selector,     NGL_NODE_FILTERSELECTOR,     sizeof(struct filterselector_opts),   "FilterSelector")
DECLARE_FILTER(srgb2linear,  NGL_NODE_FILTERSRGB2LINEAR,  0,                                    "FilterSRGB2Linear")
