/*
 * Copyright 2023 Nope Forge
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

#include "log.h"
#include "nopegl.h"
#include "internal.h"
#include "params.h"

struct timerangefilter_opts {
    struct ngl_node *child;
    double start_time;
    double end_time;
    double render_time;
    double prefetch_time;
};

struct timerangefilter_priv {
    int updated;
    int drawme;
};

#define OFFSET(x) offsetof(struct timerangefilter_opts, x)
static const struct node_param timerangefilter_params[] = {
    {"child", NGLI_PARAM_TYPE_NODE, OFFSET(child), .flags=NGLI_PARAM_FLAG_NON_NULL,
              .desc=NGLI_DOCSTRING("time filtered scene")},
    {"start",         NGLI_PARAM_TYPE_F64, OFFSET(start_time),
                      .desc=NGLI_DOCSTRING("start time (included) for the scene to be drawn")},
    {"end",           NGLI_PARAM_TYPE_F64, OFFSET(end_time), {.f64=-1.0},
                      .desc=NGLI_DOCSTRING("end time (excluded) for the scene to be drawn, a negative value implies forever")},
    {"render_time",   NGLI_PARAM_TYPE_F64, OFFSET(render_time), {.f64=-1.0},
                      .desc=NGLI_DOCSTRING("chosen time to draw for a \"once\" mode, negative to ignore")},
    {"prefetch_time", NGLI_PARAM_TYPE_F64, OFFSET(prefetch_time), {.f64=1.0},
                      .desc=NGLI_DOCSTRING("`child` is prefetched `prefetch_time` seconds in advance")},
    {NULL}
};

static int timerangefilter_init(struct ngl_node *node)
{
    const struct timerangefilter_opts *o = node->opts;

    if (o->end_time >= 0.0 && o->end_time < o->start_time) {
        LOG(ERROR, "end time must be after start time");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->start_time < 0.0) {
        LOG(ERROR, "start time cannot be negative");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->prefetch_time < 0) {
        LOG(ERROR, "prefetch time must be positive");
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

static int timerangefilter_visit(struct ngl_node *node, bool is_active, double t)
{
    struct timerangefilter_priv *s = node->priv_data;
    const struct timerangefilter_opts *o = node->opts;
    struct ngl_node *child = o->child;

    /*
     * The life of the parent takes over the life of its children: if the
     * parent is dead, the children are likely dead as well. However, a living
     * children from a dead parent can be revealed by another living branch.
     */
    if (is_active) {
        if (t < o->start_time - o->prefetch_time || (o->end_time >= 0.0 && t >= o->end_time))
            is_active = false;

        // If the child of the current once range is inactive, meaning
        // it has been previously released, we need to force an update
        // otherwise the child will stay uninitialized.
        if (!child->is_active)
            s->updated = 0;
    }

    return ngli_node_visit(child, is_active, t);
}

static int timerangefilter_update(struct ngl_node *node, double t)
{
    struct timerangefilter_priv *s = node->priv_data;
    const struct timerangefilter_opts *o = node->opts;

    s->drawme = 0;

    if (t < o->start_time || (o->end_time >= 0.0 && t >= o->end_time))
        return 0;

    if (o->render_time >= 0.0) {
        if (s->updated)
            return 0;
        t = o->render_time;
        s->updated = 1;
    }

    s->drawme = 1;

    return ngli_node_update(o->child, t);
}

static void timerangefilter_draw(struct ngl_node *node)
{
    struct timerangefilter_priv *s = node->priv_data;
    const struct timerangefilter_opts *o = node->opts;

    if (!s->drawme) {
        TRACE("%s @ %p with range [%f,%f) not marked for drawing, skip it", node->label, node, o->start_time, o->end_time);
        return;
    }

    ngli_node_draw(o->child);
}

const struct node_class ngli_timerangefilter_class = {
    .id        = NGL_NODE_TIMERANGEFILTER,
    .name      = "TimeRangeFilter",
    .init      = timerangefilter_init,
    .visit     = timerangefilter_visit,
    .update    = timerangefilter_update,
    .draw      = timerangefilter_draw,
    .opts_size = sizeof(struct timerangefilter_opts),
    .priv_size = sizeof(struct timerangefilter_priv),
    .params    = timerangefilter_params,
    .file      = __FILE__,
};
