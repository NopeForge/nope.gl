/*
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

#include <float.h>
#include <stddef.h>
#include <string.h>

#include "log.h"
#include "nopegl.h"
#include "internal.h"
#include "params.h"

struct timerangefilter_opts {
    struct ngl_node *child;
    struct ngl_node **ranges;
    int nb_ranges;
    double prefetch_time;
    double max_idle_time;
};

struct timerangefilter_priv {
    int current_range;
    int drawme;
};

#define RANGES_TYPES_LIST (const uint32_t[]){NGL_NODE_TIMERANGEMODEONCE,     \
                                             NGL_NODE_TIMERANGEMODENOOP,     \
                                             NGL_NODE_TIMERANGEMODECONT,     \
                                             NGLI_NODE_NONE}

#define OFFSET(x) offsetof(struct timerangefilter_opts, x)
static const struct node_param timerangefilter_params[] = {
    {"child", NGLI_PARAM_TYPE_NODE, OFFSET(child), .flags=NGLI_PARAM_FLAG_NON_NULL,
              .desc=NGLI_DOCSTRING("time filtered scene")},
    {"ranges", NGLI_PARAM_TYPE_NODELIST, OFFSET(ranges),
               .node_types=RANGES_TYPES_LIST,
               .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
               .desc=NGLI_DOCSTRING("key frame time filtering events")},
    {"prefetch_time", NGLI_PARAM_TYPE_F64, OFFSET(prefetch_time), {.f64=1.0},
                      .desc=NGLI_DOCSTRING("`child` is prefetched `prefetch_time` seconds in advance")},
    {"max_idle_time", NGLI_PARAM_TYPE_F64, OFFSET(max_idle_time), {.f64=4.0},
                      .desc=NGLI_DOCSTRING("`child` will not be released if it is required in the next incoming `max_idle_time` seconds")},
    {NULL}
};

static int timerangefilter_init(struct ngl_node *node)
{
    const struct timerangefilter_opts *o = node->opts;

    double prev_start_time = -DBL_MAX;
    for (int i = 0; i < o->nb_ranges; i++) {
        const struct timerangemode_opts *trm = o->ranges[i]->opts;

        if (trm->start_time < prev_start_time) {
            LOG(ERROR, "time ranges must be monotonically increasing: %g < %g",
                trm->start_time, prev_start_time);
            return NGL_ERROR_INVALID_ARG;
        }
        prev_start_time = trm->start_time;
    }

    if (o->prefetch_time < 0) {
        LOG(ERROR, "prefetch time must be positive");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->max_idle_time <= o->prefetch_time) {
        LOG(ERROR, "max idle time must be superior to prefetch time");
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

static int get_rr_id(const struct timerangefilter_opts *o, int start, double t)
{
    int ret = -1;

    for (int i = start; i < o->nb_ranges; i++) {
        const struct timerangemode_opts *rr = o->ranges[i]->opts;
        if (rr->start_time > t)
            break;
        ret = i;
    }
    return ret;
}

static int update_rr_state(struct timerangefilter_priv *s, const struct timerangefilter_opts *o, double t)
{
    if (!o->nb_ranges)
        return NGL_ERROR_INVALID_ARG;

    int rr_id = get_rr_id(o, s->current_range, t);
    if (rr_id < 0) {
        // range not found, probably backward in time so look from the
        // start
        // TODO: look for rr using bsearch?
        rr_id = get_rr_id(o, 0, t);
    }

    if (rr_id >= 0) {
        if (s->current_range != rr_id) {
            // We leave our current render range, so we reset the "Once" flag
            // for next time we may come in again (seek back)
            struct ngl_node *cur_rr = o->ranges[s->current_range];
            if (cur_rr->cls->id == NGL_NODE_TIMERANGEMODEONCE) {
                struct timerangemode_priv *rro = cur_rr->priv_data;
                rro->updated = 0;
            }
        }

        s->current_range = rr_id;
    }

    return rr_id;
}

static int timerangefilter_visit(struct ngl_node *node, int is_active, double t)
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
        const int rr_id = update_rr_state(s, o, t);

        if (rr_id >= 0) {
            struct ngl_node *rr = o->ranges[rr_id];

            s->current_range = rr_id;

            if (rr->cls->id == NGL_NODE_TIMERANGEMODENOOP) {
                is_active = 0;

                if (rr_id < o->nb_ranges - 1) {
                    // We assume here the next range requires the node started
                    // as the current one doesn't.
                    const struct timerangemode_opts *next = o->ranges[rr_id + 1]->opts;
                    const double next_use_in = next->start_time - t;

                    if (next_use_in <= o->prefetch_time) {
                        TRACE("next use of %s in %g (< %g), mark as active",
                              child->label, next_use_in, o->prefetch_time);

                        // The node will actually be needed soon, so we need to
                        // start it if necessary.
                        is_active = 1;
                    } else if (next_use_in <= o->max_idle_time && child->is_active) {
                        TRACE("%s not currently needed but will be soon %g (< %g), keep as active",
                              child->label, next_use_in, o->max_idle_time);

                        // The node will be needed in a slight amount of time;
                        // a bit longer than a prefetch period so we don't need
                        // to start it, but in the case where it's actually
                        // already active it's not worth releasing it to start
                        // it again soon after, so we keep it active.
                        is_active = 1;
                    }
                }
            } else if (rr->cls->id == NGL_NODE_TIMERANGEMODEONCE) {
                // If the child of the current once range is inactive, meaning
                // it has been previously released, we need to force an update
                // otherwise the child will stay uninitialized.
                if (!child->is_active) {
                    struct timerangemode_priv *rro = rr->priv_data;
                    rro->updated = 0;
                }
            }
        }
    }

    return ngli_node_visit(child, is_active, t);
}

static int timerangefilter_update(struct ngl_node *node, double t)
{
    struct timerangefilter_priv *s = node->priv_data;
    const struct timerangefilter_opts *o = node->opts;

    s->drawme = 0;

    const int rr_id = update_rr_state(s, o, t);
    if (rr_id >= 0) {
        struct ngl_node *rr = o->ranges[rr_id];

        if (rr->cls->id == NGL_NODE_TIMERANGEMODENOOP)
            return 0;

        if (rr->cls->id == NGL_NODE_TIMERANGEMODEONCE) {
            struct timerangemode_priv *rro = rr->priv_data;
            if (rro->updated)
                return 0;
            const struct timerangemode_opts *rro_opts = rr->opts;
            t = rro_opts->render_time;
            rro->updated = 1;
        }
    }

    s->drawme = 1;

    return ngli_node_update(o->child, t);
}

static void timerangefilter_draw(struct ngl_node *node)
{
    struct timerangefilter_priv *s = node->priv_data;
    const struct timerangefilter_opts *o = node->opts;

    if (!s->drawme) {
        TRACE("%s @ %p not marked for drawing, skip it", node->label, node);
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
