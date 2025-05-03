/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <stddef.h>
#include <string.h>

#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "ngpu/graphics_state.h"
#include "node_graphicconfig.h"
#include "nopegl.h"
#include "utils/utils.h"

struct graphicconfig_opts {
    struct ngl_node *child;

    int blend;
    enum ngpu_blend_factor blend_src_factor;
    enum ngpu_blend_factor blend_dst_factor;
    enum ngpu_blend_factor blend_src_factor_a;
    enum ngpu_blend_factor blend_dst_factor_a;
    enum ngpu_blend_op blend_op;
    enum ngpu_blend_op blend_op_a;

    enum ngpu_color_component color_write_mask;

    int depth_test;
    uint32_t depth_write_mask;
    enum ngpu_compare_op depth_func;

    int stencil_test;
    uint32_t stencil_write_mask;
    enum ngpu_compare_op stencil_func;
    uint32_t stencil_ref;
    uint32_t stencil_read_mask;
    enum ngpu_stencil_op stencil_fail;
    enum ngpu_stencil_op stencil_depth_fail;
    enum ngpu_stencil_op stencil_depth_pass;

    enum ngpu_cull_mode cull_mode;

    int32_t scissor[4];
};

struct graphicconfig_priv {
    int use_scissor;
};

#define DEFAULT_SCISSOR {-1, -1, -1, -1}

static const struct param_choices blend_factor_choices = {
    .name = "blend_factor",
    .consts = {
        {"unset",               -1,                     .desc=NGLI_DOCSTRING("unset")},
        {"zero",                NGPU_BLEND_FACTOR_ZERO,                .desc=NGLI_DOCSTRING("`0`")},
        {"one",                 NGPU_BLEND_FACTOR_ONE,                 .desc=NGLI_DOCSTRING("`1`")},
        {"src_color",           NGPU_BLEND_FACTOR_SRC_COLOR,           .desc=NGLI_DOCSTRING("`src_color`")},
        {"one_minus_src_color", NGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, .desc=NGLI_DOCSTRING("`1 - src_color`")},
        {"dst_color",           NGPU_BLEND_FACTOR_DST_COLOR,           .desc=NGLI_DOCSTRING("`dst_color`")},
        {"one_minus_dst_color", NGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR, .desc=NGLI_DOCSTRING("`1 - dst_color`")},
        {"src_alpha",           NGPU_BLEND_FACTOR_SRC_ALPHA,           .desc=NGLI_DOCSTRING("`src_alpha`")},
        {"one_minus_src_alpha", NGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .desc=NGLI_DOCSTRING("`1 - src_alpha`")},
        {"dst_alpha",           NGPU_BLEND_FACTOR_DST_ALPHA,           .desc=NGLI_DOCSTRING("`dst_alpha`")},
        {"one_minus_dst_alpha", NGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, .desc=NGLI_DOCSTRING("`1 - dst_alpha`")},
        {NULL}
    }
};

static const struct param_choices blend_op_choices = {
    .name = "blend_operation",
    .consts = {
        {"unset",  -1,                             .desc=NGLI_DOCSTRING("unset")},
        {"add",    NGPU_BLEND_OP_ADD,              .desc=NGLI_DOCSTRING("`src + dst`")},
        {"sub",    NGPU_BLEND_OP_SUBTRACT,         .desc=NGLI_DOCSTRING("`src - dst`")},
        {"revsub", NGPU_BLEND_OP_REVERSE_SUBTRACT, .desc=NGLI_DOCSTRING("`dst - src`")},
        {"min",    NGPU_BLEND_OP_MIN,              .desc=NGLI_DOCSTRING("`min(src, dst)`")},
        {"max",    NGPU_BLEND_OP_MAX,              .desc=NGLI_DOCSTRING("`max(src, dst)`")},
        {NULL}
    }
};

static const struct param_choices component_choices = {
    .name = "component",
    .consts = {
        {"r", NGPU_COLOR_COMPONENT_R_BIT, .desc=NGLI_DOCSTRING("red")},
        {"g", NGPU_COLOR_COMPONENT_G_BIT, .desc=NGLI_DOCSTRING("green")},
        {"b", NGPU_COLOR_COMPONENT_B_BIT, .desc=NGLI_DOCSTRING("blue")},
        {"a", NGPU_COLOR_COMPONENT_A_BIT, .desc=NGLI_DOCSTRING("alpha")},
        {NULL}
    }
};

static const struct param_choices func_choices = {
    .name = "function",
    .consts = {
        {"unset",    -1,                               .desc=NGLI_DOCSTRING("unset")},
        {"never",    NGPU_COMPARE_OP_NEVER,            .desc=NGLI_DOCSTRING("`f(a,b) = 0`")},
        {"less",     NGPU_COMPARE_OP_LESS,             .desc=NGLI_DOCSTRING("`f(a,b) = a < b`")},
        {"equal",    NGPU_COMPARE_OP_EQUAL,            .desc=NGLI_DOCSTRING("`f(a,b) = a == b`")},
        {"lequal",   NGPU_COMPARE_OP_LESS_OR_EQUAL,    .desc=NGLI_DOCSTRING("`f(a,b) = a ≤ b`")},
        {"greater",  NGPU_COMPARE_OP_GREATER,          .desc=NGLI_DOCSTRING("`f(a,b) = a > b`")},
        {"notequal", NGPU_COMPARE_OP_NOT_EQUAL,        .desc=NGLI_DOCSTRING("`f(a,b) = a ≠ b`")},
        {"gequal",   NGPU_COMPARE_OP_GREATER_OR_EQUAL, .desc=NGLI_DOCSTRING("`f(a,b) = a ≥ b`")},
        {"always",   NGPU_COMPARE_OP_ALWAYS,           .desc=NGLI_DOCSTRING("`f(a,b) = 1`")},
        {NULL}
    }
};

static const struct param_choices stencil_op_choices = {
    .name = "stencil_operation",
    .consts = {
        {"unset",       -1,                                  .desc=NGLI_DOCSTRING("unset")},
        {"keep",        NGPU_STENCIL_OP_KEEP,                .desc=NGLI_DOCSTRING("keeps the current value")},
        {"zero",        NGPU_STENCIL_OP_ZERO,                .desc=NGLI_DOCSTRING("sets the stencil buffer value to 0")},
        {"replace",     NGPU_STENCIL_OP_REPLACE,             .desc=NGLI_DOCSTRING("sets the stencil buffer value to ref, as specified by the stencil function")},
        {"incr",        NGPU_STENCIL_OP_INCREMENT_AND_CLAMP, .desc=NGLI_DOCSTRING("increments the current stencil buffer value and clamps it")},
        {"incr_wrap",   NGPU_STENCIL_OP_INCREMENT_AND_WRAP,  .desc=NGLI_DOCSTRING("increments the current stencil buffer value and wraps it")},
        {"decr",        NGPU_STENCIL_OP_DECREMENT_AND_CLAMP, .desc=NGLI_DOCSTRING("decrements the current stencil buffer value and clamps it")},
        {"decr_wrap",   NGPU_STENCIL_OP_DECREMENT_AND_WRAP,  .desc=NGLI_DOCSTRING("decrements the current stencil buffer value and wraps it")},
        {"decr_invert", NGPU_STENCIL_OP_INVERT,              .desc=NGLI_DOCSTRING("bitwise inverts the current stencil buffer value")},
        {NULL}
    }
};

static const struct param_choices cull_mode_choices = {
    .name = "cull_mode",
    .consts = {
        {"unset", -1,                       .desc=NGLI_DOCSTRING("unset")},
        {"none",  NGPU_CULL_MODE_NONE,      .desc=NGLI_DOCSTRING("no facets are discarded")},
        {"front", NGPU_CULL_MODE_FRONT_BIT, .desc=NGLI_DOCSTRING("cull front-facing facets")},
        {"back",  NGPU_CULL_MODE_BACK_BIT,  .desc=NGLI_DOCSTRING("cull back-facing facets")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct graphicconfig_opts, x)
static const struct node_param graphicconfig_params[] = {
    {"child",              NGLI_PARAM_TYPE_NODE,   OFFSET(child),              .flags=NGLI_PARAM_FLAG_NON_NULL,
                           .desc=NGLI_DOCSTRING("scene to which the graphic configuration will be applied")},
    {"blend",              NGLI_PARAM_TYPE_BOOL,   OFFSET(blend),              {.i32=-1},
                           .desc=NGLI_DOCSTRING("enable blending")},
    {"blend_src_factor",   NGLI_PARAM_TYPE_SELECT, OFFSET(blend_src_factor),   {.i32=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("blend source factor")},
    {"blend_dst_factor",   NGLI_PARAM_TYPE_SELECT, OFFSET(blend_dst_factor),   {.i32=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("blend destination factor")},
    {"blend_src_factor_a", NGLI_PARAM_TYPE_SELECT, OFFSET(blend_src_factor_a), {.i32=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("alpha blend source factor")},
    {"blend_dst_factor_a", NGLI_PARAM_TYPE_SELECT, OFFSET(blend_dst_factor_a), {.i32=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("alpha blend destination factor")},
    {"blend_op",           NGLI_PARAM_TYPE_SELECT, OFFSET(blend_op),           {.i32=-1},
                           .choices=&blend_op_choices,
                           .desc=NGLI_DOCSTRING("blend operation")},
    {"blend_op_a",         NGLI_PARAM_TYPE_SELECT, OFFSET(blend_op_a),         {.i32=-1},
                           .choices=&blend_op_choices,
                           .desc=NGLI_DOCSTRING("alpha blend operation")},
    {"color_write_mask",   NGLI_PARAM_TYPE_FLAGS,  OFFSET(color_write_mask),   {.i32=-1},
                           .choices=&component_choices,
                           .desc=NGLI_DOCSTRING("color write mask")},
    {"depth_test",         NGLI_PARAM_TYPE_BOOL,   OFFSET(depth_test),         {.i32=-1},
                           .desc=NGLI_DOCSTRING("enable depth testing")},
    {"depth_write_mask",   NGLI_PARAM_TYPE_BOOL,   OFFSET(depth_write_mask),   {.i32=-1},
                           .desc=NGLI_DOCSTRING("depth write mask")},
    {"depth_func",         NGLI_PARAM_TYPE_SELECT, OFFSET(depth_func),         {.i32=-1},
                           .desc=NGLI_DOCSTRING("passes if `<function>(depth, stored_depth)`"),
                           .choices=&func_choices},
    {"stencil_test",       NGLI_PARAM_TYPE_BOOL,   OFFSET(stencil_test),       {.i32=-1},
                           .desc=NGLI_DOCSTRING("enable stencil testing")},
    {"stencil_write_mask", NGLI_PARAM_TYPE_I32,    OFFSET(stencil_write_mask), {.i32=-1},
                           .desc=NGLI_DOCSTRING("stencil write mask, must be in the range [0, 0xff]")},
    {"stencil_func",       NGLI_PARAM_TYPE_SELECT, OFFSET(stencil_func),       {.i32=-1},
                           .desc=NGLI_DOCSTRING("passes if `<function>(stencil_ref & stencil_read_mask, stencil & stencil_read_mask)`"),
                           .choices=&func_choices},
    {"stencil_ref",        NGLI_PARAM_TYPE_I32,    OFFSET(stencil_ref),        {.i32=-1},
                           .desc=NGLI_DOCSTRING("stencil reference value to compare against")},
    {"stencil_read_mask",  NGLI_PARAM_TYPE_I32,    OFFSET(stencil_read_mask),  {.i32=-1},
                           .desc=NGLI_DOCSTRING("stencil read mask, must be in the range [0, 0xff]")},
    {"stencil_fail",       NGLI_PARAM_TYPE_SELECT, OFFSET(stencil_fail),       {.i32=-1},
                           .choices=&stencil_op_choices,
                           .desc=NGLI_DOCSTRING("operation to execute if stencil test fails")},
    {"stencil_depth_fail", NGLI_PARAM_TYPE_SELECT, OFFSET(stencil_depth_fail), {.i32=-1},
                           .choices=&stencil_op_choices,
                           .desc=NGLI_DOCSTRING("operation to execute if depth test fails")},
    {"stencil_depth_pass", NGLI_PARAM_TYPE_SELECT, OFFSET(stencil_depth_pass), {.i32=-1},
                           .choices=&stencil_op_choices,
                           .desc=NGLI_DOCSTRING("operation to execute if stencil and depth test pass")},
    {"cull_mode",          NGLI_PARAM_TYPE_SELECT, OFFSET(cull_mode),          {.i32=-1},
                           .choices=&cull_mode_choices,
                           .desc=NGLI_DOCSTRING("face culling mode")},
    {"scissor",            NGLI_PARAM_TYPE_IVEC4, OFFSET(scissor), {.ivec=DEFAULT_SCISSOR},
                           .desc=NGLI_DOCSTRING("define an area where all pixels outside are discarded")},
    {NULL}
};

#define COPY_PARAM(name) do {        \
    if (o->name != -1) {             \
        state->name = o->name;       \
    }                                \
} while (0)                          \

#define COPY_STENCIL_PARAM(name) do {                   \
    if (o->stencil_##name != -1) {                      \
        state->stencil_front.name = o->stencil_##name;  \
        state->stencil_back.name  = o->stencil_##name;  \
    }                                                   \
} while (0)                                             \

void ngli_node_graphicconfig_get_state(const struct ngl_node *node, struct ngpu_graphics_state *state)
{
    struct ngl_ctx *ctx = node->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct graphicconfig_opts *o = node->opts;

    COPY_PARAM(blend);
    COPY_PARAM(blend_dst_factor);
    COPY_PARAM(blend_src_factor);
    COPY_PARAM(blend_dst_factor_a);
    COPY_PARAM(blend_src_factor_a);
    COPY_PARAM(blend_op);
    COPY_PARAM(blend_op_a);

    COPY_PARAM(color_write_mask);

    COPY_PARAM(depth_test);
    COPY_PARAM(depth_write_mask);
    COPY_PARAM(depth_func);

    COPY_PARAM(stencil_test);
    COPY_STENCIL_PARAM(write_mask);
    COPY_STENCIL_PARAM(func);
    COPY_STENCIL_PARAM(ref);
    COPY_STENCIL_PARAM(read_mask);
    COPY_STENCIL_PARAM(fail);
    COPY_STENCIL_PARAM(depth_fail);
    COPY_STENCIL_PARAM(depth_pass);

    if (o->cull_mode != -1)
        state->cull_mode = ngpu_ctx_transform_cull_mode(gpu_ctx, o->cull_mode);
}

static int graphicconfig_init(struct ngl_node *node)
{
    struct graphicconfig_priv *s = node->priv_data;
    const struct graphicconfig_opts *o = node->opts;

    if (o->stencil_write_mask != -1 && o->stencil_write_mask > 0xff) {
        LOG(ERROR, "stencil write mask (0x%x) must be in the range [0, 0xff]", o->stencil_write_mask);
        return NGL_ERROR_INVALID_USAGE;
    }

    if (o->stencil_read_mask != -1 && o->stencil_read_mask > 0xff) {
        LOG(ERROR, "stencil read mask (0x%x) must be in the range [0, 0xff]", o->stencil_read_mask);
        return NGL_ERROR_INVALID_USAGE;
    }

    static const int default_scissor[4] = DEFAULT_SCISSOR;
    s->use_scissor = memcmp(o->scissor, default_scissor, sizeof(o->scissor));

    return 0;
}

static int graphicconfig_prepare(struct ngl_node *node)
{
    const struct graphicconfig_opts *o = node->opts;

    struct rnode *rnode = node->ctx->rnode_pos;
    ngli_node_graphicconfig_get_state(node, &rnode->graphics_state);

    return ngli_node_prepare(o->child);
}

static void graphicconfig_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct graphicconfig_priv *s = node->priv_data;
    const struct graphicconfig_opts *o = node->opts;

    struct ngpu_scissor prev_scissor;
    if (s->use_scissor) {
        prev_scissor = ctx->scissor;
        ctx->scissor = (struct ngpu_scissor){NGLI_ARG_VEC4(o->scissor)};
    }

    ngli_node_draw(o->child);

    if (s->use_scissor)
        ctx->scissor = prev_scissor;
}

const struct node_class ngli_graphicconfig_class = {
    .id        = NGL_NODE_GRAPHICCONFIG,
    .name      = "GraphicConfig",
    .init      = graphicconfig_init,
    .prepare   = graphicconfig_prepare,
    .update    = ngli_node_update_children,
    .draw      = graphicconfig_draw,
    .opts_size = sizeof(struct graphicconfig_opts),
    .priv_size = sizeof(struct graphicconfig_priv),
    .params    = graphicconfig_params,
    .file      = __FILE__,
};
