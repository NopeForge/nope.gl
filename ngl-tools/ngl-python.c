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

#include <stdio.h>
#include <stdlib.h>

#include <nodegl.h>

#include "common.h"
#include "player.h"
#include "python_utils.h"

struct ctx {
    int log_level;
    int aspect[2];
    struct ngl_config cfg;
    double duration;
};

int main(int argc, char *argv[])
{
    int ret;
    struct ctx s = {
        .log_level  = NGL_LOG_INFO,
        .cfg.width  = 1280,
        .cfg.height = 800,
        .aspect[0]  = 1,
        .aspect[1]  = 1,
        .cfg.swap_interval  = -1,
        .cfg.clear_color[3] = 1.f,
    };

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <module|script.py> <scene_func>\n", argv[0]);
        return -1;
    }

    ngl_log_set_min_level(s.log_level);

    struct ngl_node *scene = python_get_scene(argv[1], argv[2], &s.duration, s.aspect);
    if (!scene)
        return -1;

    get_viewport(s.cfg.width, s.cfg.height, s.aspect, s.cfg.viewport);

    struct player p;
    ret = player_init(&p, "ngl-python", scene, &s.cfg, s.duration, 1);
    if (ret < 0)
        goto end;
    ngl_node_unrefp(&scene);

    player_main_loop();

end:
    player_uninit();

    return ret;
}
