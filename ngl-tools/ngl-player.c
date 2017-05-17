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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <nodegl.h>
#include <sxplayer.h>

#include <GLFW/glfw3.h>

#include "common.h"

struct view_info {
    double x;
    double y;
    double width;
    double height;
};

#define ASPECT_RATIO(info) ((info).width / (double)(info).height)

struct ngl_ctx *g_ctx;
int64_t g_clock_off = -1;
int64_t g_frame_ts;
int64_t g_lasthover = -1;
struct sxplayer_info g_info;
struct view_info g_view_info;
int g_paused;
struct ngl_node *g_opacity_uniform;

static const char *pgbar_shader = \
"#version 100"                                                          "\n" \
"precision mediump float;"                                              "\n" \
                                                                        "\n" \
"uniform float time;"                                                   "\n" \
"uniform float ar;"                                                     "\n" \
"uniform float opacity;"                                                "\n" \
"uniform sampler2D tex0_sampler;"                                       "\n" \
"varying vec2 var_tex0_coords;"                                         "\n" \
                                                                        "\n" \
"void main()"                                                           "\n" \
"{"                                                                     "\n" \
"    float height = 2.0 / 100. * ar;"                                   "\n" \
"    float x = var_tex0_coords.x;"                                      "\n" \
"    float y = var_tex0_coords.y;"                                      "\n" \
"    vec4 video_pix = texture2D(tex0_sampler, var_tex0_coords);"        "\n" \
"    vec4 color = video_pix;"                                           "\n" \
"    if (y > 1. - height)"                                              "\n" \
"        color = x < time ? vec4(1) : mix(video_pix, vec4(1), 0.3);"    "\n" \
"    gl_FragColor = mix(video_pix, color, opacity);"                    "\n" \
"}"                                                                     "\n";

static struct ngl_node *get_scene(const char *filename)
{
    static const float corner[3] = { -1.0, -1.0, 0.0 };
    static const float width[3]  = {  2.0,  0.0, 0.0 };
    static const float height[3] = {  0.0,  2.0, 0.0 };

    struct ngl_node *media   = ngl_node_create(NGL_NODE_MEDIA, filename);
    struct ngl_node *texture = ngl_node_create(NGL_NODE_TEXTURE);
    struct ngl_node *quad    = ngl_node_create(NGL_NODE_QUAD, corner, width, height);
    struct ngl_node *shader  = ngl_node_create(NGL_NODE_SHADER);
    struct ngl_node *tshape  = ngl_node_create(NGL_NODE_TEXTUREDSHAPE, quad, shader);

    struct ngl_node *uniforms[3] = {
        ngl_node_create(NGL_NODE_UNIFORMSCALAR, "time"),
        ngl_node_create(NGL_NODE_UNIFORMSCALAR, "ar"),
        ngl_node_create(NGL_NODE_UNIFORMSCALAR, "opacity"),
    };

    struct ngl_node *time_animkf[2] = {
        ngl_node_create(NGL_NODE_ANIMKEYFRAMESCALAR, 0.0, 0.0),
        ngl_node_create(NGL_NODE_ANIMKEYFRAMESCALAR, g_info.duration, 1.0),
    };

    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_set(shader, "fragment_data", pgbar_shader);
    ngl_node_param_add(uniforms[0], "animkf", 2, time_animkf);
    ngl_node_param_set(uniforms[1], "value", ASPECT_RATIO(g_info));
    ngl_node_param_set(uniforms[2], "value", 0.0);

    ngl_node_param_add(tshape, "textures", 1, &texture);
    ngl_node_param_add(tshape, "uniforms", 3, uniforms);

    g_opacity_uniform = uniforms[2];

    ngl_node_unrefp(&uniforms[0]);
    ngl_node_unrefp(&uniforms[1]);
    ngl_node_unrefp(&uniforms[2]);
    ngl_node_unrefp(&time_animkf[0]);
    ngl_node_unrefp(&time_animkf[1]);

    ngl_node_unrefp(&shader);
    ngl_node_unrefp(&media);
    ngl_node_unrefp(&texture);
    ngl_node_unrefp(&quad);

    return tshape;
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

static int init(GLFWwindow *window, const char *filename)
{
    g_ctx = ngl_create();
    ngl_set_glcontext(g_ctx, NULL, NULL, NULL, NGL_GLPLATFORM_AUTO, NGL_GLAPI_AUTO);
    glViewport(0, 0, g_info.width, g_info.height);

    struct ngl_node *scene = get_scene(filename);
    if (!scene)
        return -1;

    int ret = ngl_set_scene(g_ctx, scene);
    if (ret < 0)
        return ret;

    ngl_node_unrefp(&scene);
    return 0;
}

static void update_time(int64_t seek_at)
{
    if (seek_at >= 0) {
        g_clock_off = gettime() - seek_at;
        g_frame_ts = seek_at;
        return;
    }

    if (!g_paused) {
        const int64_t now = gettime();

        if (g_clock_off < 0)
            g_clock_off = now;

        g_frame_ts = now - g_clock_off;
    }

    if (g_lasthover >= 0) {
        const int64_t t64_diff = gettime() - g_lasthover;
        const double opacity = clipd(1.5 - t64_diff / 1000000.0, 0, 1);
        ngl_node_param_set(g_opacity_uniform, "value", opacity);
    }
}

static void render(void)
{
    ngl_draw(g_ctx, g_frame_ts / 1000000.0);
}

static void reset(void)
{
    ngl_free(&g_ctx);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        switch(key) {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_SPACE:
            g_paused ^= 1;
            g_clock_off = gettime() - g_frame_ts;
            break;
        default:
            break;
        }
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double pos;
        double xpos;
        double ypos;
        int64_t seek_at;

        glfwGetCursorPos(window, &xpos, &ypos);

        pos = clipd(xpos - g_view_info.x, 0.0, g_view_info.width);
        seek_at = 1000000 * g_info.duration * pos / g_view_info.width;

        g_lasthover = gettime();
        update_time(seek_at);
    }
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    g_lasthover = gettime();
}

static void size_callback(GLFWwindow *window, int width, int height)
{
    g_view_info.width = width;
    g_view_info.height = width / ASPECT_RATIO(g_info);

    if (g_view_info.height > height) {
        g_view_info.height = height;
        g_view_info.width = height * ASPECT_RATIO(g_info);
    }

    g_view_info.x = (width - g_view_info.width) / 2.0;
    g_view_info.y = (height - g_view_info.height) / 2.0;

    glViewport(g_view_info.x, g_view_info.y, g_view_info.width, g_view_info.height);
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

    if (init_glfw() < 0)
        return EXIT_FAILURE;

    GLFWwindow *window = get_window("ngl-player", g_info.width, g_info.height);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetWindowSizeCallback(window, size_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    ret = init(window, argv[1]);
    if (ret < 0)
        goto end;

    do {
        update_time(-1);
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);

end:
    reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    return ret;
}
