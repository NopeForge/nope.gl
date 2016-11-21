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
    {"capability", PARAM_TYPE_INT, OFFSET(capability),    {.i64=GL_BLEND}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"enabled",    PARAM_TYPE_INT, OFFSET(enabled[0]),    {.i64=GL_FALSE}, .flags=PARAM_FLAG_CONSTRUCTOR},
    {"src_rgb",    PARAM_TYPE_INT, OFFSET(src_rgb[0]),    {.i64=GL_ONE}},
    {"dst_rgb",    PARAM_TYPE_INT, OFFSET(dst_rgb[0]),    {.i64=GL_ZERO}},
    {"src_alpha",  PARAM_TYPE_INT, OFFSET(src_alpha[0]),  {.i64=GL_ONE}},
    {"dst_alpha",  PARAM_TYPE_INT, OFFSET(dst_alpha[0]),  {.i64=GL_ZERO}},
    {"mode_rgb",   PARAM_TYPE_INT, OFFSET(mode_rgb[0]),   {.i64=GL_FUNC_ADD}},
    {"mode_alpha", PARAM_TYPE_INT, OFFSET(mode_alpha[0]), {.i64=GL_FUNC_ADD}},
    {NULL}
};

#define DECLARE_GLENUM(glenum) case ((glenum)): name = #glenum + 3; break

static const char *get_enum_name(GLenum glenum)
{
    const char *name = NULL;

    switch(glenum) {
    DECLARE_GLENUM(GL_ZERO);
    DECLARE_GLENUM(GL_ONE);
    DECLARE_GLENUM(GL_SRC_COLOR);
    DECLARE_GLENUM(GL_ONE_MINUS_SRC_COLOR);
    DECLARE_GLENUM(GL_DST_COLOR);
    DECLARE_GLENUM(GL_ONE_MINUS_DST_COLOR);
    DECLARE_GLENUM(GL_SRC_ALPHA);
    DECLARE_GLENUM(GL_ONE_MINUS_SRC_ALPHA);
    DECLARE_GLENUM(GL_DST_ALPHA);
    DECLARE_GLENUM(GL_ONE_MINUS_DST_ALPHA);
    DECLARE_GLENUM(GL_CONSTANT_COLOR);
    DECLARE_GLENUM(GL_ONE_MINUS_CONSTANT_COLOR);
    DECLARE_GLENUM(GL_CONSTANT_ALPHA);
    DECLARE_GLENUM(GL_ONE_MINUS_CONSTANT_ALPHA);
    DECLARE_GLENUM(GL_SRC_ALPHA_SATURATE);
    DECLARE_GLENUM(GL_FUNC_ADD);
    DECLARE_GLENUM(GL_FUNC_SUBTRACT);
    DECLARE_GLENUM(GL_FUNC_REVERSE_SUBTRACT);
    DECLARE_GLENUM(GL_MIN);
    DECLARE_GLENUM(GL_MAX);
    default:
        LOG(ERROR, "unsupported GLenum %x", glenum);
    }

    return name;
}

static char *glblendstate_info_str(const struct ngl_node *node)
{
    const struct glstate *s = node->priv_data;
    if (s->enabled[0]) {
        return ngli_asprintf("BLEND "
                             "src_rgb=%s dst_rgb=%s "
                             "src_alpha=%s dst_alpha=%s "
                             "mode_rgb=%s mode_alpha=%s",
                             get_enum_name(s->src_rgb[0]),   get_enum_name(s->dst_rgb[0]),
                             get_enum_name(s->src_alpha[0]), get_enum_name(s->dst_alpha[0]),
                             get_enum_name(s->mode_rgb[0]),  get_enum_name(s->mode_alpha[0]));
    } else {
        return ngli_asprintf("BLEND disabled");
    }
}

const struct node_class ngli_glblendstate_class = {
    .id        = NGL_NODE_GLBLENDSTATE,
    .name      = "GLBlendState",
    .info_str  = glblendstate_info_str,
    .priv_size = sizeof(struct glstate),
    .params    = glblendstate_params,
};
