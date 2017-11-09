/*
 * Copyright 2016 GoPro Inc.
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
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct timerangemode, x)
static const struct node_param continuous_params[] = {
    {"start_time", PARAM_TYPE_DBL, OFFSET(start_time), .flags = PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static const struct node_param norender_params[] = {
    {"start_time", PARAM_TYPE_DBL, OFFSET(start_time), .flags = PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static const struct node_param once_params[] = {
    {"start_time",  PARAM_TYPE_DBL, OFFSET(start_time),  .flags = PARAM_FLAG_CONSTRUCTOR},
    {"render_time", PARAM_TYPE_DBL, OFFSET(render_time), .flags = PARAM_FLAG_CONSTRUCTOR},
    {NULL}
};

static char *timerangemode_info_str_continous(const struct ngl_node *node)
{
    const struct timerangemode *s = node->priv_data;
    return ngli_asprintf("continuous at %g", s->start_time);
}

static char *timerangemode_info_str_norender(const struct ngl_node *node)
{
    const struct timerangemode *s = node->priv_data;
    return ngli_asprintf("norender at %g", s->start_time);
}

static char *timerangemode_info_str_once(const struct ngl_node *node)
{
    const struct timerangemode *s = node->priv_data;
    return ngli_asprintf("once at %g (with t=%g)", s->start_time, s->render_time);
}

const struct node_class ngli_timerangemodecont_class = {
    .id        = NGL_NODE_TIMERANGEMODECONT,
    .name      = "TimeRangeModeCont",
    .info_str  = timerangemode_info_str_continous,
    .priv_size = sizeof(struct timerangemode),
    .params    = continuous_params,
};

const struct node_class ngli_timerangemodenoop_class = {
    .id        = NGL_NODE_TIMERANGEMODENOOP,
    .name      = "TimeRangeModeNoop",
    .info_str  = timerangemode_info_str_norender,
    .priv_size = sizeof(struct timerangemode),
    .params    = norender_params,
};

const struct node_class ngli_timerangemodeonce_class = {
    .id        = NGL_NODE_TIMERANGEMODEONCE,
    .info_str  = timerangemode_info_str_once,
    .name      = "TimeRangeModeOnce",
    .priv_size = sizeof(struct timerangemode),
    .params    = once_params,
};
