/*
 * Copyright 2018 GoPro Inc.
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

#include <Cocoa/Cocoa.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <nodegl.h>

#include "wsi.h"

static int get_default_backend(void)
{
    int nb_backends;
    struct ngl_backend *backends;
    int ret = ngl_backends_get(NULL, &nb_backends, &backends);
    if (ret < 0)
        return ret;
    for (int i = 0; i < nb_backends; i++) {
        if (backends[i].is_default) {
            ret = backends[i].id;
            ngl_backends_freep(&backends);
            return ret;
        }
    }
    ngl_backends_freep(&backends);
    return NGL_ERROR_NOT_FOUND;
}

int wsi_set_ngl_config(struct ngl_config *config, SDL_Window *window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        fprintf(stderr, "Failed to get window WM information\n");
        return -1;
    }

    if (info.subsystem == SDL_SYSWM_COCOA) {
        NSWindow *nswindow = info.info.cocoa.window;
        NSView *view = [nswindow contentView];

        if (config->backend == NGL_BACKEND_AUTO) {
            config->backend = get_default_backend();
            if (config->backend < 0)
                return -1;
        }
        if (config->backend == NGL_BACKEND_VULKAN) {
            NSBundle *bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
            view.layer = [[bundle classNamed:@"CAMetalLayer"] layer];
            view.wantsLayer = YES;
        }

        config->platform = NGL_PLATFORM_MACOS;
        config->window = (uintptr_t)view;
        return 0;
    }

    return -1;
}
