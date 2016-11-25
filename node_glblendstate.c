/*
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
#include <string.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct glstate, x)
static const struct node_param glblendstate_params[] = {
    {"enabled",    PARAM_TYPE_INT, OFFSET(enabled[0]),    {.i64=GL_FALSE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"src_rgb",    PARAM_TYPE_INT, OFFSET(src_rgb[0]),    {.i64=GL_ONE}},
    {"dst_rgb",    PARAM_TYPE_INT, OFFSET(dst_rgb[0]),    {.i64=GL_ZERO}},
    {"src_alpha",  PARAM_TYPE_INT, OFFSET(src_alpha[0]),  {.i64=GL_ONE}},
    {"dst_alpha",  PARAM_TYPE_INT, OFFSET(dst_alpha[0]),  {.i64=GL_ZERO}},
    {"mode_rgb",   PARAM_TYPE_INT, OFFSET(mode_rgb[0]),   {.i64=GL_FUNC_ADD}},
    {"mode_alpha", PARAM_TYPE_INT, OFFSET(mode_alpha[0]), {.i64=GL_FUNC_ADD}},
    {NULL}
};

static char *get_blend_str(GLenum parameter, const char *comp, const int rgb)
{
    const char *comp_str = rgb ? "" : "A";
    switch (parameter) {
        case GL_ZERO:                       return NULL;
        case GL_ONE:                        return ngli_asprintf("%s", comp);
        case GL_SRC_COLOR:                  return ngli_asprintf(   "%s*src%s/k%s",  comp, comp_str, comp_str);
        case GL_ONE_MINUS_SRC_COLOR:        return ngli_asprintf("%s*(1-src%s/k%s)", comp, comp_str, comp_str);
        case GL_DST_COLOR:                  return ngli_asprintf(   "%s*dst%s/k%s",  comp, comp_str, comp_str);
        case GL_ONE_MINUS_DST_COLOR:        return ngli_asprintf("%s*(1-dst%s/k%s)", comp, comp_str, comp_str);
        case GL_SRC_ALPHA:                  return ngli_asprintf(   "%s*srcA/kA",  comp);
        case GL_ONE_MINUS_SRC_ALPHA:        return ngli_asprintf("%s*(1-srcA/kA)", comp);
        case GL_DST_ALPHA:                  return ngli_asprintf(   "%s*dstA/kA",  comp);
        case GL_ONE_MINUS_DST_ALPHA:        return ngli_asprintf("%s*(1-dstA/kA)", comp);
        case GL_CONSTANT_COLOR:             return ngli_asprintf(   "%s*src%s",  comp, comp_str);
        case GL_ONE_MINUS_CONSTANT_COLOR:   return ngli_asprintf("%s*(1-src%s)", comp, comp_str);
        case GL_CONSTANT_ALPHA:             return ngli_asprintf(   "%s*srcA",  comp);
        case GL_ONE_MINUS_CONSTANT_ALPHA:   return ngli_asprintf("%s*(1-srcA)", comp);
        case GL_SRC_ALPHA_SATURATE:         return ngli_asprintf(rgb ? "%s*min(srcA,1-dstA)" : "%s", comp);
        default:
            LOG(WARNING, "unsupported blend parameter 0x%x", parameter);
            return ngli_asprintf("%s*[?]", comp);
    }
}

static char *get_func_str(GLenum mode, GLenum src, GLenum dst, const int rgb)
{
    const char op = mode == GL_FUNC_ADD ? '+' : '-';
    const char *lcomp = rgb ? "src" : "srcA";
    const char *rcomp = rgb ? "dst" : "dstA";

    char *lblend = get_blend_str(src, lcomp, rgb);
    char *rblend = get_blend_str(dst, rcomp, rgb);

    if (mode == GL_FUNC_REVERSE_SUBTRACT) {
        NGLI_SWAP(const char *, lcomp,  rcomp);
        NGLI_SWAP(      char *, lblend, rblend);
    }

    char *ret;

    if (!lblend && !rblend) {
        ret = ngli_asprintf("0");
    } else if (!lblend) {
        ret = ngli_asprintf("%s%s", op == '-' ? "-" : "", rblend);
        free(rblend);
    } else if (!rblend) {
        ret = lblend;
    } else {
        ret = ngli_asprintf("%s %c %s", lblend, op, rblend);
        free(lblend);
        free(rblend);
    }

    return ret;
}

static char *glblendstate_info_str(const struct ngl_node *node)
{
    char *info_str;
    const struct glstate *s = node->priv_data;
    if (s->enabled[0]) {
        char *rgb_blend   = get_func_str(s->mode_rgb[0],   s->src_rgb[0],   s->dst_rgb[0],   1);
        char *alpha_blend = get_func_str(s->mode_alpha[0], s->src_alpha[0], s->dst_alpha[0], 0);
        info_str = ngli_asprintf("BLEND dst=%s  dstA=%s", rgb_blend, alpha_blend);
        free(rgb_blend);
        free(alpha_blend);
    } else {
        info_str = ngli_asprintf("BLEND disabled");
    }
    return info_str;
}

static int glblendstate_init(struct ngl_node *node)
{
    struct glstate *s = node->priv_data;

    s->capability = GL_BLEND;

    return 0;
}

const struct node_class ngli_glblendstate_class = {
    .id        = NGL_NODE_GLBLENDSTATE,
    .name      = "GLBlendState",
    .info_str  = glblendstate_info_str,
    .init      = glblendstate_init,
    .priv_size = sizeof(struct glstate),
    .params    = glblendstate_params,
};
