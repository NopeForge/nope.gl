/*
 * Copyright 2022 GoPro Inc.
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

#include "internal.h"
#include "log.h"
#include "params.h"

struct userselect_opts {
    struct ngl_node **branches;
    size_t nb_branches;
    struct livectl live;
};

static int branch_update_func(struct ngl_node *node)
{
    struct userselect_opts *o = node->opts;
    if (!o->live.id)
        return 0;
    if (o->live.val.i[0] < o->live.min.i[0]) {
        LOG(WARNING, "value (%d) is smaller than live_min (%d), clamping", o->live.val.i[0], o->live.min.i[0]);
        o->live.val.i[0] = o->live.min.i[0];
    }
    if (o->live.val.i[0] > o->live.max.i[0]) {
        LOG(WARNING, "value (%d) is larger than live_max (%d), clamping", o->live.val.i[0], o->live.max.i[0]);
        o->live.val.i[0] = o->live.max.i[0];
    }
    return 0;
}

#define OFFSET(x) offsetof(struct userselect_opts, x)
static const struct node_param userselect_params[] = {
    {"branches", NGLI_PARAM_TYPE_NODELIST, OFFSET(branches),
                 .desc=NGLI_DOCSTRING("a set of branches to pick from")},
    {"branch",   NGLI_PARAM_TYPE_I32, OFFSET(live.val.i), {.i32=0},
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=branch_update_func,
                 .desc=NGLI_DOCSTRING("controls which branch is taken")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {"live_min", NGLI_PARAM_TYPE_I32, OFFSET(live.min.i), {.i32=0},
                 .desc=NGLI_DOCSTRING("minimum value allowed during live change (only honored when live_id is set)")},
    {"live_max", NGLI_PARAM_TYPE_I32, OFFSET(live.max.i), {.i32=10},
                 .desc=NGLI_DOCSTRING("maximum value allowed during live change (only_honored when live_id is set)")},
    {NULL}
};

/*
 * This is similar to what's being done in the Group node: even if they are
 * updated and drawn exclusively, each branch may still have its own specific
 * rendering / graphics configuration, so we need to create a render path for
 * each of them.
 */
static int userselect_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    const struct userselect_opts *o = node->opts;

    int ret = 0;
    struct rnode *rnode_pos = ctx->rnode_pos;
    for (size_t i = 0; i < o->nb_branches; i++) {
        struct rnode *rnode = ngli_rnode_add_child(rnode_pos);
        if (!rnode)
            return NGL_ERROR_MEMORY;
        ctx->rnode_pos = rnode;

        ret = ngli_node_prepare(o->branches[i]);
        if (ret < 0)
            break;
    }

    ctx->rnode_pos = rnode_pos;
    return ret;
}

static int userselect_visit(struct ngl_node *node, bool is_active, double t)
{
    const struct userselect_opts *o = node->opts;

    const int branch_id = o->live.val.i[0];
    for (size_t i = 0; i < o->nb_branches; i++) {
        struct ngl_node *branch = o->branches[i];
        int ret = ngli_node_visit(branch, is_active && i == branch_id, t);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int userselect_update(struct ngl_node *node, double t)
{
    const struct userselect_opts *o = node->opts;

    const int branch_id = o->live.val.i[0];
    if (branch_id < 0 || branch_id >= o->nb_branches)
        return 0;
    return ngli_node_update(o->branches[branch_id], t);
}

static void userselect_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    const struct userselect_opts *o = node->opts;

    const int branch_id = o->live.val.i[0];
    if (branch_id < 0 || branch_id >= o->nb_branches)
        return;
    struct rnode *rnode_pos = ctx->rnode_pos;
    struct rnode *rnodes = ngli_darray_data(&rnode_pos->children);
    ctx->rnode_pos = &rnodes[branch_id];
    ngli_node_draw(o->branches[branch_id]);
    ctx->rnode_pos = rnode_pos;
}

const struct node_class ngli_userselect_class = {
    .id             = NGL_NODE_USERSELECT,
    .name           = "UserSelect",
    .prepare        = userselect_prepare,
    .visit          = userselect_visit,
    .update         = userselect_update,
    .draw           = userselect_draw,
    .opts_size      = sizeof(struct userselect_opts),
    .params         = userselect_params,
    .flags          = NGLI_NODE_FLAG_LIVECTL,
    .livectl_offset = OFFSET(live),
    .file           = __FILE__,
};

