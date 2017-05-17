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

#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

#include <GLFW/glfw3.h>
#include <nodegl.h>

struct player {

    GLFWwindow *window;
    struct {
        double x;
        double y;
        double width;
        double height;
    } view;

    int width, height;
    double duration;

    struct ngl_ctx *ngl;
    int64_t clock_off;
    int64_t frame_ts;
    int paused;
    int64_t lasthover;
    void (*tick_callback)(struct player *p);
};

int player_init(struct player *p, const char *win_title, struct ngl_node *scene,
                int width, int height, double duration);

void player_uninit(void);

void player_main_loop(void);

#endif
