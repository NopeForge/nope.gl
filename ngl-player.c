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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <nodegl.h>
#include <sxplayer.h>

#include <GLFW/glfw3.h>

struct view_info {
    double x;
    double y;
    double width;
    double height;
};

#define ASPECT_RATIO(info) ((info).width / (double)(info).height)

struct ngl_ctx *g_ctx;
int64_t g_clock_off = -1;
struct sxplayer_info g_info;
struct view_info g_view_info;

static int64_t gettime()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
}

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

    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_add(tshape, "textures", 1, &texture);

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

static void render()
{
    int64_t clock_cur = gettime();

    if (g_clock_off < 0)
        g_clock_off = gettime();

    int64_t time = (clock_cur - g_clock_off);

    ngl_draw(g_ctx, time / 1000000.0);
}

static void reset()
{
    ngl_free(&g_ctx);
}

static double clipd(double v, double min, double max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double pos;
        double xpos;
        double ypos;

        glfwGetCursorPos(window, &xpos, &ypos);

        pos = clipd(xpos - g_view_info.x, 0.0, g_view_info.width);
        g_clock_off = gettime() - (pos / g_view_info.width * g_info.duration * 1000000);
    }
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
    GLFWwindow *window;
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <media>\n", argv[0]);
        return -1;
    }

    ret = probe(argv[1]);
    if (ret < 0)
        return ret;

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    window = glfwCreateWindow(g_info.width, g_info.height, "ngl-player", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to initialize GL context\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetWindowSizeCallback(window, size_callback);

    ret = init(window, argv[1]);
    if (ret < 0)
        goto end;

    do {
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
