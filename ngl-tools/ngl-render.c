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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <nodegl.h>

#include "common.h"
#include "wsi.h"

static struct ngl_node *get_scene(const char *filename)
{
    struct ngl_node *scene = NULL;
    char *buf = NULL;
    struct stat st;

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "unable to open %s\n", filename);
        goto end;
    }

    if (fstat(fd, &st) == -1)
        goto end;

    buf = malloc(st.st_size + 1);
    if (!buf)
        goto end;

    int n = read(fd, buf, st.st_size);
    buf[n] = 0;

    scene = ngl_node_deserialize(buf);

end:
    if (fd != -1)
        close(fd);
    free(buf);
    return scene;
}

struct range {
    float start;
    float duration;
    int freq;
};

int main(int argc, char *argv[])
{
    int ret = 0;
    const char *input = NULL;
    const char *output = NULL;
    int width = 320, height = 240;
    struct range ranges[128] = {0};
    struct range *r;
    int nb_ranges = 0;
    int show_window = 0;
    int swap_interval = 0;
    int debug = 0;
    GLFWwindow *window = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            debug = 1;
        } else if (!strcmp(argv[i], "-w")) {
            show_window = 1;
        } else if (argv[i][0] == '-' && i < argc - 1) {
            const char opt = argv[i][1];
            const char *arg = argv[i + 1];
            switch (opt) {
                case 'o':
                    output = arg;
                    break;
                case 's':
                    if (sscanf(arg, "%dx%d", &width, &height) != 2) {
                        fprintf(stderr, "Invalid size format: \"%s\" "
                                "is not following \"WxH\"\n", arg);
                        return EXIT_FAILURE;
                    }
                    if (width <= 0 || width > 8192 || height <= 0 || height > 8192) {
                        fprintf(stderr, "Invalid size: \"%s\" exceeds 8192x8192\n", arg);
                        return EXIT_FAILURE;
                    }
                    break;
                case 'z':
                    swap_interval = atoi(arg);
                    break;
                case 't':
                    if (nb_ranges >= sizeof(ranges)/sizeof(*ranges)) {
                        fprintf(stderr, "Too much ranges specified (max:%d)\n",
                                (int)(sizeof(ranges)/sizeof(*ranges)));
                        return EXIT_FAILURE;
                    }
                    r = &ranges[nb_ranges++];
                    if (sscanf(arg, "%f:%f:%d", &r->start, &r->duration, &r->freq) != 3) {
                        fprintf(stderr, "Invalid range format: \"%s\" "
                                "is not following \"start:duration:freq\"\n", arg);
                        return EXIT_FAILURE;
                    }
                    break;
                default:
                    fprintf(stderr, "Unknown option -%c\n", opt);
                    return EXIT_FAILURE;
            }
            i++;
        } else if (!input) {
            input = argv[i];
        } else {
            fprintf(stderr, "Unexpected option \"%s\"\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (!input) {
        fprintf(stderr, "Usage: %s [-o out.raw] [-s WxH] [-w] [-d] [-z swapinterval] input.ngl\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!nb_ranges) {
        fprintf(stderr, "At least one range needs to be specified\n");
        return EXIT_FAILURE;
    }

    printf("%s -> %s %dx%d\n", input, output ? output : "-", width, height);

    if (show_window) {
        if (init_glfw() < 0)
            return EXIT_FAILURE;

        window = get_window("ngl-render", width, height);
        if (!window) {
            glfwTerminate();
            return EXIT_FAILURE;
        }
    }

    int fd = -1;
    struct ngl_ctx *ctx = NULL;
    uint8_t *capture_buffer = NULL;

    struct ngl_node *scene = get_scene(input);
    if (!scene) {
        ret = EXIT_FAILURE;
        goto end;
    }

    if (output) {
        int flags = O_WRONLY|O_CREAT|O_TRUNC;
#ifdef O_BINARY
        flags |= O_BINARY;
#endif
        fd = open(output, flags, 0644);
        if (fd == -1) {
            fprintf(stderr, "Unable to open %s\n", output);
            ret = EXIT_FAILURE;
            goto end;
        }
        capture_buffer = calloc(width * height, 4);
        if (!capture_buffer)
            goto end;
    }

    ctx = ngl_create();
    if (!ctx) {
        ngl_node_unrefp(&scene);
        goto end;
    }

    struct ngl_config config = {
        .width = width,
        .height = height,
        .viewport = {0, 0, width, height},
        .offscreen = !show_window,
        .capture_buffer = capture_buffer,
    };
    if (show_window) {
        ret = wsi_set_ngl_config(&config, window);
        if (ret < 0) {
            ngl_node_unrefp(&scene);
            return ret;
        }
        config.swap_interval = swap_interval;
    }

    ret = ngl_configure(ctx, &config);
    if (ret < 0) {
        ngl_node_unrefp(&scene);
        goto end;
    }

    ret = ngl_set_scene(ctx, scene);
    ngl_node_unrefp(&scene);
    if (ret < 0)
        goto end;

    for (int i = 0; i < nb_ranges; i++) {
        int k = 0;
        const struct range *r = &ranges[i];
        const float t0 = r->start;
        const float t1 = r->start + r->duration;

        const int64_t start = gettime();

        for (;;) {
            const float t = t0 + k*1./r->freq;
            if (t >= t1)
                break;
            if (debug)
                printf("draw @ t=%f [range %d/%d: %g-%g @ %dHz]\n",
                       t, i + 1, nb_ranges, t0, t1, r->freq);
            ret = ngl_draw(ctx, t);
            if (ret < 0) {
                fprintf(stderr, "Unable to draw @ t=%g\n", t);
                goto end;
            }
            if (capture_buffer)
                write(fd, capture_buffer, 4 * width * height);
            if (show_window)
                glfwPollEvents();
            k++;
        }

        const double tdiff = (gettime() - start) / 1000000.;
        printf("Rendered %d frames in %g (FPS=%g)\n", k, tdiff, k / tdiff);
    }

end:
    ngl_freep(&ctx);

    if (fd != -1)
        close(fd);

    free(capture_buffer);

    if (show_window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return ret;
}
