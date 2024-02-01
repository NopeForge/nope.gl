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

#ifndef NODE_MEDIA_H
#define NODE_MEDIA_H

#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <nopemd.h>

#if defined(TARGET_ANDROID)
#include "android_imagereader.h"

struct android_surface_compat {
    struct android_imagereader *imagereader;
    void *surface_handle;
};
#endif

struct media_priv {
    struct nmd_ctx *player;
    struct nmd_frame *frame;
    size_t nb_parents;
    double start_time;
    double end_time;
    int prefetched;
    int invalidated;

#if defined(TARGET_ANDROID)
    struct android_surface_compat android_surface;
#endif
};

#endif
