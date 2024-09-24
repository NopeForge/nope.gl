/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef NODE_TEXTEFFECT_H
#define NODE_TEXTEFFECT_H

#include <stdint.h>

struct texteffect_opts {
    double start_time;
    double end_time;
    int target;
    int random;
    uint32_t random_seed;

    /* if animated, expressed in effect time (0 to 1) */
    struct ngl_node *start_pos_node;
    float start_pos;
    struct ngl_node *end_pos_node;
    float end_pos;
    struct ngl_node *overlap_node;
    float overlap;

    /* if animated, expressed in target time (0 to 1) */
    struct ngl_node *transform_chain;
    float anchor[2];
    int anchor_ref;
    struct ngl_node *color_node;
    float color[3];
    struct ngl_node *opacity_node;
    float opacity;
    struct ngl_node *outline_node;
    float outline;
    struct ngl_node *outline_color_node;
    float outline_color[3];
    struct ngl_node *glow_node;
    float glow;
    struct ngl_node *glow_color_node;
    float glow_color[3];
    struct ngl_node *blur_node;
    float blur;
};

#endif
