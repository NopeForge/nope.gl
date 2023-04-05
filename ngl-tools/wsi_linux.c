/*
 * Copyright 2018-2022 GoPro Inc.
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

#include <SDL.h>
#include <SDL_config.h>
#include <SDL_syswm.h>
#include <nopegl.h>

#include "wsi.h"

int wsi_set_ngl_config(struct ngl_config *config, SDL_Window *window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        fprintf(stderr, "Failed to get window WM information: %s\n", SDL_GetError());
        return -1;
    }
    if (info.subsystem == SDL_SYSWM_WAYLAND) {
#ifdef SDL_VIDEO_DRIVER_WAYLAND
        config->platform = NGL_PLATFORM_WAYLAND;
        config->display = (uintptr_t)info.info.wl.display;
        config->window = (uintptr_t)info.info.wl.surface;
        return 0;
#endif
    } else if (info.subsystem == SDL_SYSWM_X11) {
#ifdef SDL_VIDEO_DRIVER_X11
        config->platform = NGL_PLATFORM_XLIB;
        config->display = (uintptr_t)info.info.x11.display;
        config->window = (uintptr_t)info.info.x11.window;
        return 0;
#endif
    }

    return -1;
}
