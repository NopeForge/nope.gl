/*
 * Copyright 2017 GoPro Inc.
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

#include <stdlib.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"

struct timerangefilter {
    struct ngl_node *child;
    struct ngl_node **ranges;
    int nb_ranges;
    int current_range;
    double prefetch_time;
    double max_idle_time;

    int drawme;
};

#define RANGES_TYPES_LIST (const int[]){NGL_NODE_TIMERANGEMODEONCE,     \
                                        NGL_NODE_TIMERANGEMODENOOP,     \
                                        NGL_NODE_TIMERANGEMODECONT,     \
                                        -1}

#define OFFSET(x) offsetof(struct timerangefilter, x)
static const struct node_param timerangefilter_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"ranges", PARAM_TYPE_NODELIST, OFFSET(ranges),
               .node_types=RANGES_TYPES_LIST,
               .flags=PARAM_FLAG_DOT_DISPLAY_PACKED},
    {"prefetch_time", PARAM_TYPE_DBL, OFFSET(prefetch_time), {.dbl=1.0}},
    {"max_idle_time", PARAM_TYPE_DBL, OFFSET(max_idle_time), {.dbl=4.0}},
    {NULL}
};

static int compare_range(const void *p1, const void *p2)
{
    const struct ngl_node *n1 = *(const struct ngl_node **)p1;
    const struct ngl_node *n2 = *(const struct ngl_node **)p2;
    const struct timerangemode *r1 = n1->priv_data;
    const struct timerangemode *r2 = n2->priv_data;
    return r1->start_time > r2->start_time;
}

static int timerangefilter_init(struct ngl_node *node)
{
    struct timerangefilter *s = node->priv_data;

    if (s->prefetch_time < 0) {
        LOG(ERROR, "prefetch time must be positive");
        return -1;
    }

    if (s->max_idle_time <= s->prefetch_time) {
        LOG(ERROR, "max idle time must be superior to prefetch time");
        return -1;
    }

    // Sort the ranges by start time. We also skip the ngli_node_init as, for
    // now, render ranges don't have any
    // TODO: merge successive continuous and norender ones?
    qsort(s->ranges, s->nb_ranges, sizeof(*s->ranges), compare_range);

    return 0;
}

static int get_rr_id(const struct timerangefilter *s, int start, double t)
{
    int ret = -1;

    for (int i = start; i < s->nb_ranges; i++) {
        const struct timerangemode *rr = s->ranges[i]->priv_data;
        if (rr->start_time > t)
            break;
        ret = i;
    }
    return ret;
}

static int update_rr_state(struct timerangefilter *s, double t)
{
    if (!s->nb_ranges)
        return -1;

    int rr_id = get_rr_id(s, s->current_range, t);
    if (rr_id < 0) {
        // range not found, probably backward in time so look from the
        // start
        // TODO: look for rr using bsearch?
        rr_id = get_rr_id(s, 0, t);
    }

    if (rr_id >= 0) {
        if (s->current_range != rr_id) {
            // We leave our current render range, so we reset the "Once" flag
            // for next time we may come in again (seek back)
            struct ngl_node *cur_rr = s->ranges[s->current_range];
            if (cur_rr->class->id == NGL_NODE_TIMERANGEMODEONCE) {
                struct timerangemode *rro = cur_rr->priv_data;
                rro->updated = 0;
            }
        }

        s->current_range = rr_id;
    }

    return rr_id;
}

// TODO: render once
static void timerangefilter_visit(struct ngl_node *node, const struct ngl_node *from, double t)
{
    struct timerangefilter *s = node->priv_data;

    /*
     * The life of the parent takes over the life of its children: if the
     * parent is dead, the children are likely dead as well. However, a living
     * children from a dead parent can be revealed by another living branch.
     */
    int is_active = from ? from->is_active : 1;
    if (is_active) {
        const int rr_id = update_rr_state(s, t);

        if (rr_id >= 0) {
            struct ngl_node *rr = s->ranges[rr_id];

            s->current_range = rr_id;

            if (rr->class->id == NGL_NODE_TIMERANGEMODENOOP) {
                is_active = 0;

                if (rr_id < s->nb_ranges - 1) {
                    // We assume here the next range requires the node started
                    // as the current one doesn't.
                    const struct timerangemode *next = s->ranges[rr_id + 1]->priv_data;
                    const double next_use_in = next->start_time - t;

                    if (next_use_in < s->prefetch_time) {
                        // The node will actually be needed soon, so we need to
                        // start it if necessary.
                        is_active = 1;
                    } else if (next_use_in < s->max_idle_time && node->state == STATE_READY) {
                        // The node will be needed in a slight amount of time;
                        // a bit longer than a prefetch period so we don't need
                        // to start it, but in the case where it's actually
                        // already active it's not worth releasing it to start
                        // it again soon after, so we keep it active.
                        is_active = 1;
                    }
                }
            }
        }
    }

    /*
     * If a node is inactive and is already in a dead state, there is no need
     * to check for resources below as we can assume they were already released
     * as well (unless they're shared with another branch) by
     * honor_release_prefetch().
     *
     * On the other hand, we cannot do the same if the node is active, because
     * we have to mark every node below for activity to prevent an early
     * release from another branch.
     */
    if (!is_active && node->state == STATE_IDLE)
        return;

    if (node->active_time != t) {
        // If we never passed through this node for that given time, the new
        // active state takes over to replace the one from a previous update.
        node->is_active = is_active;
        node->active_time = t;
    } else {
        // This is not the first time we come across that node, so if it's
        // needed in that part of the branch we mark it as active so it doesn't
        // get released.
        node->is_active |= is_active;
    }

    return ngli_node_visit(s->child, node, t);
}

static void timerangefilter_update(struct ngl_node *node, double t)
{
    struct timerangefilter *s = node->priv_data;

    s->drawme = 0;

    const int rr_id = update_rr_state(s, t);
    if (rr_id >= 0) {
        struct ngl_node *rr = s->ranges[rr_id];

        if (rr->class->id == NGL_NODE_TIMERANGEMODENOOP)
            return;

        if (rr->class->id == NGL_NODE_TIMERANGEMODEONCE) {
            struct timerangemode *rro = rr->priv_data;
            if (rro->updated)
                return;
            t = rro->render_time;
            rro->updated = 1;
        }
    }

    s->drawme = 1;
    ngli_node_update(s->child, t);
}

static void timerangefilter_draw(struct ngl_node *node)
{
    struct timerangefilter *s = node->priv_data;

    if (!s->drawme) {
        LOG(VERBOSE, "%s @ %p not marked for drawing, skip it", node->name, node);
        return;
    }

    ngli_node_draw(s->child);
}

const struct node_class ngli_timerangefilter_class = {
    .id        = NGL_NODE_TIMERANGEFILTER,
    .name      = "TimeRangeFilter",
    .init      = timerangefilter_init,
    .visit     = timerangefilter_visit,
    .update    = timerangefilter_update,
    .draw      = timerangefilter_draw,
    .priv_size = sizeof(struct timerangefilter),
    .params    = timerangefilter_params,
};
