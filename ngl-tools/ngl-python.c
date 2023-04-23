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

#include <stdio.h>
#include <stdlib.h>

#include <nopegl.h>

#include "common.h"
#include "opts.h"
#include "player.h"
#include "python_utils.h"

struct ctx {
    /* options */
    int log_level;
    struct ngl_config cfg;
    int player_ui;
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-l", "--loglevel",      OPT_TYPE_LOGLEVEL, .offset=OFFSET(log_level)},
    {"-b", "--backend",       OPT_TYPE_BACKEND,  .offset=OFFSET(cfg.backend)},
    {"-s", "--size",          OPT_TYPE_RATIONAL, .offset=OFFSET(cfg.width)},
    {"-z", "--swap_interval", OPT_TYPE_INT,      .offset=OFFSET(cfg.swap_interval)},
    {"-c", "--clear_color",   OPT_TYPE_COLOR,    .offset=OFFSET(cfg.clear_color)},
    {"-m", "--samples",       OPT_TYPE_INT,      .offset=OFFSET(cfg.samples)},
    {"-u", "--disable-ui",    OPT_TYPE_TOGGLE,   .offset=OFFSET(player_ui)},
};

int main(int argc, char *argv[])
{
    struct ctx s = {
        .log_level  = NGL_LOG_INFO,
        .cfg.width  = DEFAULT_WIDTH,
        .cfg.height = DEFAULT_HEIGHT,
        .cfg.swap_interval  = -1,
        .cfg.clear_color[3] = 1.f,
        .player_ui          = 1,
    };

    int ret = opts_parse(argc, argc - 2, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP || argc < 3) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), " <module> <scene_func>");
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    ngl_log_set_min_level(s.log_level);

    const char *scene_func = argv[argc - 1];
    const char *module     = argv[argc - 2];
    struct ngl_scene *scene = python_get_scene(module, scene_func);
    if (!scene)
        return -1;

    get_viewport(s.cfg.width, s.cfg.height, scene->aspect_ratio, s.cfg.viewport);

    struct player p;
    ret = player_init(&p, "ngl-python", scene, &s.cfg, s.player_ui);
    ngl_scene_freep(&scene);
    if (ret < 0)
        goto end;

    player_main_loop(&p);

end:
    player_uninit(&p);

    return ret;
}
