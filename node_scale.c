/*
 * Copyright 2016 Clément Bœsch <cboesch@gopro.com>
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
#include "math_utils.h"

#define OFFSET(x) offsetof(struct scale, x)
static const struct node_param scale_params[] = {
    {"child",   PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR},
    {"factors", PARAM_TYPE_VEC3, OFFSET(factors)},
    {"anchor",  PARAM_TYPE_VEC3, OFFSET(anchor)},
    {"animkf",  PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1}},
    {NULL}
};

static int scale_init(struct ngl_node *node)
{
    struct scale *s = node->priv_data;
    LOG(VERBOSE, "scale %s by (%f,%f,%f) with anchor (%f,%f,%f)",
        s->child->class->name,
        s->factors[0], s->factors[1], s->factors[2],
        s->anchor[0],  s->anchor[1],  s->anchor[2]);
    ngli_node_init(s->child);
    for (int i = 0; i < s->nb_animkf; i++)
        ngli_node_init(s->animkf[i]);
    return 0;
}

static const float *get_factors(struct scale *s, double t)
{
    if (s->nb_animkf)
        ngli_animkf_interpolate(s->factors, s->animkf, s->nb_animkf, &s->current_kf, t);
    return s->factors;
}

static void scale_update(struct ngl_node *node, double t)
{
    struct scale *s = node->priv_data;
    struct ngl_node *child = s->child;
    const float *f = get_factors(s, t);
    const float sm[4*4] = { // TODO: anchor
        f[0], 0.0f, 0.0f, 0.0f,
        0.0f, f[1], 0.0f, 0.0f,
        0.0f, 0.0f, f[2], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    ngli_mat4_mul(child->modelview_matrix, node->modelview_matrix, sm);
    memcpy(child->projection_matrix, node->projection_matrix, sizeof(node->projection_matrix));
    ngli_node_update(child, t);
}

static void scale_draw(struct ngl_node *node)
{
    struct scale *s = node->priv_data;
    ngli_node_draw(s->child);
}

const struct node_class ngli_scale_class = {
    .id        = NGL_NODE_SCALE,
    .name      = "Scale",
    .init      = scale_init,
    .update    = scale_update,
    .draw      = scale_draw,
    .priv_size = sizeof(struct scale),
    .params    = scale_params,
};
