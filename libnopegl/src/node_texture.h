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

#ifndef NODE_TEXTURE_H
#define NODE_TEXTURE_H

#include <stdint.h>

#include "image.h"
#include "ngpu/texture.h"
#include "nopegl.h"
#include "params.h"

struct ngl_node;

struct texture_info {
    int requested_format;
    struct ngpu_texture_params params;
    uint32_t supported_image_layouts;
    int clamp_video;
    int rtt;
    struct ngpu_texture *texture;
    struct image image;
    size_t image_rev;
};

enum ngpu_pgcraft_shader_tex_type ngli_node_texture_get_pgcraft_shader_tex_type(const struct ngl_node *node);
enum ngpu_pgcraft_shader_tex_type ngli_node_texture_get_pgcraft_shader_image_type(const struct ngl_node *node);
int ngli_node_texture_has_media_data_src(const struct ngl_node *node);

#endif
