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

#include <stdio.h>

#include <nodegl.h>
#include <sxplayer.h>

#include "common.h"
#include "player.h"

struct sxplayer_info g_info;
static const char *media_vertex =
"void main()"                                                                       "\n"
"{"                                                                                 "\n"
"    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;"    "\n"
"    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;"        "\n"
"}";

static const char *media_fragment =
"void main()"                                                           "\n"
"{"                                                                     "\n"
"    ngl_out_color = ngl_texvideo(tex0, var_tex0_coord);"               "\n"
"}";

static struct ngl_node *get_scene(const char *filename)
{
    static const float corner[3] = {-1.0, -1.0, 0.0};
    static const float width[3]  = { 2.0,  0.0, 0.0};
    static const float height[3] = { 0.0,  2.0, 0.0};

    struct ngl_node *media   = ngl_node_create(NGL_NODE_MEDIA, filename);
    struct ngl_node *texture = ngl_node_create(NGL_NODE_TEXTURE2D);
    struct ngl_node *quad    = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *render  = ngl_node_create(NGL_NODE_RENDER, quad);

    struct ngl_node *var_tex0_coord = ngl_node_create(NGL_NODE_IOVEC2);

    ngl_node_param_set(quad, "corner", corner);
    ngl_node_param_set(quad, "width", width);
    ngl_node_param_set(quad, "height", height);

    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_set(program, "vertex",   media_vertex);
    ngl_node_param_set(program, "fragment", media_fragment);
    ngl_node_param_set(program, "vert_out_vars", "var_tex0_coord", var_tex0_coord);

    ngl_node_param_set(render, "program", program);
    ngl_node_param_set(render, "frag_resources", "tex0",           texture);

    ngl_node_unrefp(&var_tex0_coord);

    ngl_node_unrefp(&program);
    ngl_node_unrefp(&media);
    ngl_node_unrefp(&texture);
    ngl_node_unrefp(&quad);

    return render;
}

static int probe(const char *filename)
{
    struct sxplayer_ctx *ctx = sxplayer_create(filename);
    if (!ctx)
        return -1;

    int ret = sxplayer_get_info(ctx, &g_info);
    if (ret < 0)
        return ret;

    sxplayer_free(&ctx);

    return 0;
}

int main(int argc, char *argv[])
{
    int ret;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <media>\n", argv[0]);
        return -1;
    }

    ret = probe(argv[1]);
    if (ret < 0)
        return ret;

    struct ngl_node *scene = get_scene(argv[1]);
    if (!scene)
        return -1;

    struct player p;
    struct ngl_config cfg = {
        .width  = g_info.width,
        .height = g_info.height,
    };
    ret = player_init(&p, "ngl-player", scene, &cfg, g_info.duration, 1);
    if (ret < 0)
        goto end;
    ngl_node_unrefp(&scene);

    player_main_loop();

end:
    player_uninit();

    return ret;
}
