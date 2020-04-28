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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"
#include "player.h"
#include "wsi.h"

static struct player *g_player;


static int save_ppm(const char *filename, uint8_t *data, int width, int height)
{
    int ret = 0;
    int flags = O_WRONLY|O_CREAT|O_TRUNC;
#ifdef O_BINARY
    flags |= O_BINARY;
#endif
    int fd = open(filename, flags, 0644);
    if (fd == -1) {
        fprintf(stderr, "Unable to open '%s'\n", filename);
        return -1;
    }

    uint8_t *buf = malloc(32 + width * height * 3);
    if (!buf) {
        ret = -1;
        goto end;
    }

    const int header_size = snprintf((char *)buf, 32, "P6 %d %d 255\n", width, height);
    if (header_size < 0) {
        ret = -1;
        fprintf(stderr, "Failed to write PPM header\n");
        goto end;
    }

    uint8_t *dst = buf + header_size;
    for (int i = 0; i < width * height; i++) {
        memcpy(dst, data, 3);
        dst += 3;
        data += 4;
    }

    const int size = header_size + width * height * 3;
    ret = write(fd, buf, size);
    if (ret < 0) {
        fprintf(stderr, "Failed to write PPM data\n");
        goto end;
    }

end:
    free(buf);
    close(fd);
    return ret;
}

static int screenshot(void)
{
    struct player *p = g_player;
    struct ngl_config *config = &p->ngl_config;
    struct ngl_config backup = *config;

    uint8_t *capture_buffer = calloc(config->width * config->height, 4);
    if (!capture_buffer)
        return -1;

    config->offscreen = 1;
    config->width = config->viewport[2];
    config->height = config->viewport[3];
    memset(config->viewport, 0, sizeof(config->viewport));
    config->capture_buffer = capture_buffer;

    int ret = ngl_configure(p->ngl, config);
    if (ret < 0) {
        fprintf(stderr, "Could not configure node.gl for offscreen capture\n");
        goto end;
    }
    ngl_draw(p->ngl, p->frame_ts / 1000000.0);

    char filename[32];
    snprintf(filename, sizeof(filename), "ngl-%" PRId64 ".ppm", gettime());
    fprintf(stdout, "Screenshot saved to '%s'\n", filename);
    ret = save_ppm(filename, capture_buffer, config->width, config->height);
    if (ret < 0) {
        fprintf(stderr, "Could not save screenshot to '%s'", filename);
    }

    *config = backup;
    ret = ngl_configure(p->ngl, config);
    if (ret < 0) {
        fprintf(stderr, "Could not configure node.gl for onscreen rendering\n");
        goto end;
    }
end:
    *config = backup;
    ngl_configure(p->ngl, config);
    p->clock_off = gettime() - p->frame_ts;

    free(capture_buffer);
    return ret;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    struct player *p = g_player;

    if (action == GLFW_PRESS) {
        switch(key) {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_SPACE:
            p->paused ^= 1;
            p->clock_off = gettime() - p->frame_ts;
            break;
        case GLFW_KEY_F: {
            p->fullscreen ^= 1;
            int *wi = p->win_info_backup;
            if (p->fullscreen) {
                glfwGetWindowPos(window, &wi[0], &wi[1]);
                glfwGetWindowSize(window, &wi[2], &wi[3]);

                GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode *mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0,
                                     mode->width, mode->height, 0);
            } else {
                glfwSetWindowMonitor(window, NULL,
                                     wi[0], wi[1], wi[2], wi[3], 0);
            }
            break;
        }
        case GLFW_KEY_S:
            screenshot();
            break;
        default:
            break;
        }
    }
}

static void size_callback(GLFWwindow *window, int width, int height)
{
    struct player *p = g_player;
    const double ar = p->width / (double)p->height;

    p->view.width = width;
    p->view.height = width / ar;

    if (p->view.height > height) {
        p->view.height = height;
        p->view.width = height * ar;
    }

    p->view.x = (width  - p->view.width)  / 2.0;
    p->view.y = (height - p->view.height) / 2.0;

    p->ngl_config.width = width;
    p->ngl_config.height = height;
    p->ngl_config.viewport[0] = p->view.x;
    p->ngl_config.viewport[1] = p->view.y;
    p->ngl_config.viewport[2] = p->view.width;
    p->ngl_config.viewport[3] = p->view.height;
    ngl_resize(p->ngl, width, height, p->ngl_config.viewport);
}

static void update_time(int64_t seek_at)
{
    struct player *p = g_player;

    if (seek_at >= 0) {
        p->clock_off = gettime() - seek_at;
        p->frame_ts = seek_at;
        return;
    }

    if (!p->paused) {
        const int64_t now = gettime();
        if (p->clock_off < 0 || now - p->clock_off > p->duration)
            p->clock_off = now;

        p->frame_ts = now - p->clock_off;
    }

    if (p->tick_callback)
        p->tick_callback(p);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    struct player *p = g_player;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;

        glfwGetCursorPos(window, &xpos, &ypos);

        const double pos = clipd(xpos - p->view.x, 0.0, p->view.width);
        const int64_t seek_at64 = p->duration * pos / p->view.width;

        p->lasthover = gettime();
        update_time(seek_at64);
    }
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    struct player *p = g_player;

    p->lasthover = gettime();
}

int player_init(struct player *p, const char *win_title, struct ngl_node *scene,
                int width, int height, double duration)
{
    memset(p, 0, sizeof(*p));

    g_player = p;

    if (init_window() < 0)
        return -1;

    p->window = get_window(win_title, width, height);
    if (!p->window) {
        glfwTerminate();
        return -1;
    }

    p->clock_off = -1;
    p->lasthover = -1;
    p->width = width;
    p->height = height;
    p->duration = duration * 1000000;

    glfwSetInputMode(p->window, GLFW_STICKY_KEYS, GLFW_TRUE);
    glfwSetKeyCallback(p->window, key_callback);
    glfwSetMouseButtonCallback(p->window, mouse_button_callback);
    glfwSetWindowSizeCallback(p->window, size_callback);
    glfwSetCursorPosCallback(p->window, cursor_pos_callback);

    int ret = wsi_set_ngl_config(&p->ngl_config, p->window);
    if (ret < 0)
        return ret;
    p->ngl_config.swap_interval = -1;
    p->ngl_config.width = width;
    p->ngl_config.height = height;
    p->ngl_config.viewport[0] = 0;
    p->ngl_config.viewport[1] = 0;
    p->ngl_config.viewport[2] = p->width;
    p->ngl_config.viewport[3] = p->height;
    p->ngl_config.clear_color[0] = 0.0f;
    p->ngl_config.clear_color[1] = 0.0f;
    p->ngl_config.clear_color[2] = 0.0f;
    p->ngl_config.clear_color[3] = 1.0f;

    p->ngl = ngl_create();
    if (!p->ngl)
        return -1;

    ret = ngl_configure(p->ngl, &p->ngl_config);
    if (ret < 0)
        return ret;

    ret = ngl_set_scene(p->ngl, scene);
    if (ret < 0)
        return ret;

    return 0;
}

void player_uninit(void)
{
    struct player *p = g_player;

    ngl_freep(&p->ngl);
    glfwDestroyWindow(p->window);
    glfwTerminate();
}

void player_main_loop(void)
{
    struct player *p = g_player;

    do {
        update_time(-1);
        ngl_draw(p->ngl, p->frame_ts / 1000000.0);
        glfwPollEvents();
    } while (glfwGetKey(p->window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(p->window) == 0);
}
