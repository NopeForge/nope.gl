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

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"
#include "player.h"
#include "wsi.h"

static int save_ppm(const char *filename, uint8_t *data, int32_t width, int32_t height)
{
    int ret = 0;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
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

    const size_t size = header_size + width * height * 3;
    const size_t n = fwrite(buf, 1, size, fp);
    if (n != size) {
        ret = -1;
        fprintf(stderr, "Failed to write PPM data\n");
        goto end;
    }

end:
    free(buf);
    fclose(fp);
    return ret;
}

static int screenshot(struct player *p)
{
    struct ngl_config *config = &p->ngl_config;
    struct ngl_config backup = *config;

    uint8_t *capture_buffer = calloc(config->width * config->height, 4);
    if (!capture_buffer)
        return -1;

    config->offscreen = 1;
    config->capture_buffer = capture_buffer;

    int ret = ngl_configure(p->ngl, config);
    if (ret < 0) {
        fprintf(stderr, "Could not configure nope.gl for offscreen capture\n");
        goto end;
    }
    ngl_draw(p->ngl, p->frame_time);

    char filename[32];
    snprintf(filename, sizeof(filename), "ngl-%" PRId64 ".ppm", gettime());
    fprintf(stdout, "Screenshot saved to '%s'\n", filename);
    ret = save_ppm(filename, capture_buffer, config->width, config->height);
    if (ret < 0) {
        fprintf(stderr, "Could not save screenshot to '%s'", filename);
    }

end:
    *config = backup;
    ret = ngl_configure(p->ngl, config);
    if (ret < 0)
        fprintf(stderr, "Could not configure nope.gl for onscreen rendering\n");
    p->clock_off = gettime_relative() - p->frame_ts;

    free(capture_buffer);
    return ret;
}

static void kill_scene(struct player *p)
{
    ngl_set_scene(p->ngl, NULL);
    p->pgbar_opacity_node  = NULL;
    p->pgbar_duration_node = NULL;
    p->pgbar_text_node     = NULL;
}

static void update_text(struct player *p)
{
    if (!p->pgbar_text_node)
        return;

    const int64_t frame_ts = (int64_t)p->frame_time;
    const int64_t duration = p->duration / 1000000;
    if (p->frame_index == p->text_last_frame_index && duration == p->text_last_duration)
        return;

    char buf[128];
    const int cm = (int)(frame_ts / 60);
    const int cs = (int)(frame_ts % 60);
    const int dm = (int)(duration / 60);
    const int ds = (int)(duration % 60);
    snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d (%" PRId64 " @ %d/%d)",
             cm, cs, dm, ds, p->frame_index, p->framerate[0], p->framerate[1]);
    ngl_node_param_set_str(p->pgbar_text_node, "text", buf);
    p->text_last_frame_index = p->frame_index;
    p->text_last_duration = duration;
}

static void update_pgbar(struct player *p)
{
    if (!p->pgbar_opacity_node || p->lasthover < 0)
        return;

    const int64_t t64_diff = gettime_relative() - p->lasthover;
    const float opacity = (float)clipf64(1.5 - (double)t64_diff / 1000000.0, 0, 1);
    ngl_node_param_set_f32(p->pgbar_opacity_node, "value", opacity);

    ngl_node_param_set_f32(p->pgbar_text_node, "bg_opacity", .8f * opacity);
    ngl_node_param_set_f32(p->pgbar_text_node, "fg_opacity", opacity);

    update_text(p);
}

static void set_frame_ts(struct player *p, int64_t frame_ts)
{
    p->frame_ts = frame_ts;
    p->frame_index = llrint((double)(p->frame_ts * p->framerate[0]) / (double)(p->framerate[1] * 1000000));
    p->frame_time = (double)(p->frame_index * p->framerate[1]) / (double)p->framerate[0];
}

static void set_frame_index(struct player *p, int64_t frame_index)
{
    p->frame_index = frame_index;
    p->frame_time = (double)(p->frame_index * p->framerate[1]) / (double)p->framerate[0];
    p->frame_ts = llrint((double)(p->frame_index * p->framerate[1] * 1000000) / (double)p->framerate[0]);
}

static void update_time(struct player *p, int64_t seek_at)
{
    if (seek_at >= 0) {
        p->seeking = 1;
        p->clock_off = gettime_relative() - seek_at;
        set_frame_ts(p, seek_at);
        return;
    }

    if (!p->paused && !p->mouse_down) {
        const int64_t now = gettime_relative();
        if (p->clock_off < 0 || now - p->clock_off > p->duration) {
            p->seeking = 1;
            p->clock_off = now;
        }

        set_frame_ts(p, now - p->clock_off);
    }
}

static void reset_running_time(struct player *p)
{
    p->clock_off = gettime_relative() - p->frame_ts;
}

static int toggle_hud(struct player *p)
{
    struct ngl_config *config = &p->ngl_config;
    config->hud ^= 1;
    return ngl_configure(p->ngl, config);
}

static int key_callback(struct player *p, SDL_KeyboardEvent *event)
{
    const SDL_Keycode key = event->keysym.sym;
    switch (key) {
    case SDLK_ESCAPE:
    case SDLK_q:
        return 1;
    case SDLK_SPACE:
        p->paused ^= 1;
        reset_running_time(p);
        break;
    case SDLK_f:
        p->fullscreen ^= 1;
        SDL_SetWindowFullscreen(p->window, p->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        break;
    case SDLK_h:
        return toggle_hud(p);
    case SDLK_s:
        screenshot(p);
        break;
    case SDLK_k:
        kill_scene(p);
        break;
    case SDLK_LEFT:
        p->lasthover = gettime_relative();
        update_time(p, clipi64(p->frame_ts - 10 * 1000000, 0, p->duration));
        break;
    case SDLK_RIGHT:
        p->lasthover = gettime_relative();
        update_time(p, clipi64(p->frame_ts + 10 * 1000000, 0, p->duration));
        break;
    case SDLK_o:
        p->paused = 1;
        p->lasthover = gettime_relative();
        set_frame_index(p, clipi64(p->frame_index - 1, 0, p->duration_i));
        break;
    case SDLK_p:
        p->paused = 1;
        p->lasthover = gettime_relative();
        set_frame_index(p, clipi64(p->frame_index + 1, 0, p->duration_i));
        break;
    default:
        break;
    }

    return 0;
}

static void size_callback(struct player *p, int32_t width, int32_t height)
{
    p->ngl_config.width = width;
    p->ngl_config.height = height;
    ngl_resize(p->ngl, width, height);
}

static void seek_event(struct player *p, int x)
{
    int32_t vp[4];
    ngl_get_viewport(p->ngl, vp);
    const int pos = clipi32(x - vp[0], 0, vp[2]);
    const int64_t seek_at64 = p->duration * pos / vp[2];
    p->lasthover = gettime_relative();
    update_time(p, clipi64(seek_at64, 0, p->duration));
}

static void mouse_buttondown_callback(struct player *p, SDL_MouseButtonEvent *event)
{
    p->mouse_down = 1;
    seek_event(p, event->x);
}

static void mouse_buttonup_callback(struct player *p, SDL_MouseButtonEvent *event)
{
    p->mouse_down = 0;
    p->clock_off = gettime_relative() - p->frame_ts;
}

static void mouse_pos_callback(struct player *p, SDL_MouseMotionEvent *event)
{
    p->lasthover = gettime_relative();
    if (p->mouse_down)
        seek_event(p, event->x);
}

static const char *pgbar_vert =
    "void main()"                                                                               "\n"
    "{"                                                                                         "\n"
    "    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);" "\n"
    "    coord = ngl_uvcoord;"                                                                  "\n"
    "}";

static const char *pgbar_frag =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    float stime = time / duration;"                                                "\n"
    "    float alpha = opacity * (coord.x < stime ? 1.0 : 0.3);"                        "\n"
    "    ngl_out_color = vec4(1.0) * alpha;"                                            "\n"
    "}";

static int add_progress_bar(struct player *p, struct ngl_scene *scene)
{
    int ret = 0;

    static const float bar_corner[3] = {-1.0f, -1.0f + 0.1f, 0.0f};
    static const float bar_width[3]  = { 2.0f,  0.0f, 0.0f};
    static const float bar_height[3] = { 0.0f,  2.0f * 0.01f, 0.0f}; // 1% of the height

    static const float text_box[4] = {-1.0f, -1.0f, 2.0f, 2.0f * 0.05f}; // 5% of the height

    struct ngl_node *text       = ngl_node_create(NGL_NODE_TEXT);
    struct ngl_node *quad       = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program    = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *draw       = ngl_node_create(NGL_NODE_DRAW);
    struct ngl_node *time       = ngl_node_create(NGL_NODE_TIME);
    struct ngl_node *v_duration = ngl_node_create(NGL_NODE_UNIFORMFLOAT);
    struct ngl_node *v_opacity  = ngl_node_create(NGL_NODE_UNIFORMFLOAT);
    struct ngl_node *coord      = ngl_node_create(NGL_NODE_IOVEC2);
    struct ngl_node *gcfg       = ngl_node_create(NGL_NODE_GRAPHICCONFIG);
    struct ngl_node *group      = ngl_node_create(NGL_NODE_GROUP);

    if (!text || !quad || !program || !draw || !time || !v_duration || !v_opacity ||
        !coord || !group) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    const struct ngl_scene_params *params = ngl_scene_get_params(scene);
    struct ngl_node *children[] = {params->root, draw, text};

    ngl_node_param_set_vec3(quad, "corner", bar_corner);
    ngl_node_param_set_vec3(quad, "width",  bar_width);
    ngl_node_param_set_vec3(quad, "height", bar_height);

    ngl_node_param_set_str(program, "vertex",   pgbar_vert);
    ngl_node_param_set_str(program, "fragment", pgbar_frag);
    ngl_node_param_set_dict(program, "vert_out_vars", "coord", coord);

    ngl_node_param_set_f32(v_duration, "value", (float)p->duration_f);
    ngl_node_param_set_f32(v_opacity,  "value", 0.f);

    ngl_node_param_set_node(draw, "geometry", quad);
    ngl_node_param_set_node(draw, "program", program);
    ngl_node_param_set_dict(draw, "frag_resources", "time",     time);
    ngl_node_param_set_dict(draw, "frag_resources", "duration", v_duration);
    ngl_node_param_set_dict(draw, "frag_resources", "opacity",  v_opacity);
    ngl_node_param_set_select(draw, "blending", "src_over");

    ngl_node_param_add_nodes(group, "children", ARRAY_NB(children), children);

    ngl_node_param_set_vec4(text, "box", text_box);
    ngl_node_param_set_f32(text, "bg_opacity", 0.f);
    ngl_node_param_set_f32(text, "fg_opacity", 0.f);

    struct ngl_scene_params new_params = *params;
    new_params.root = group;
    ret = ngl_scene_init(scene, &new_params);
    if (ret < 0)
        goto end;

    p->pgbar_opacity_node  = v_opacity;
    p->pgbar_duration_node = v_duration;
    p->pgbar_text_node     = text;

end:
    ngl_node_unrefp(&group);
    ngl_node_unrefp(&text);
    ngl_node_unrefp(&quad);
    ngl_node_unrefp(&program);
    ngl_node_unrefp(&draw);
    ngl_node_unrefp(&time);
    ngl_node_unrefp(&v_duration);
    ngl_node_unrefp(&v_opacity);
    ngl_node_unrefp(&coord);
    ngl_node_unrefp(&gcfg);

    return ret;
}

static int set_duration(struct player *p, double duration)
{
    p->duration_f = duration;
    p->duration = (int64_t)(p->duration_f * 1000000.0);
    p->duration_i = llrint(p->duration_f * p->framerate[0] / (double)p->framerate[1]);
    if (p->pgbar_duration_node)
        ngl_node_param_set_f32(p->pgbar_duration_node, "value", (float)p->duration_f);
    return 0;
}

static int set_framerate(struct player *p, const int *rate)
{
    if (!rate[0] || !rate[1]) {
        fprintf(stderr, "Invalid framerate %d/%d\n", rate[0], rate[1]);
        return -1;
    }
    memcpy(p->framerate, rate, sizeof(p->framerate));
    p->duration_i = llrint(p->duration_f * rate[0] / (double)rate[1]);
    set_frame_ts(p, p->frame_ts);
    return 0;
}

static int set_aspect_ratio(struct player *p, const int32_t *aspect)
{
    memcpy(p->aspect, aspect, sizeof(p->aspect));
    if (!p->aspect[0] || !p->aspect[1])
        p->aspect[0] = p->aspect[1] = 1;
    int32_t width, height;
    SDL_GetWindowSize(p->window, &width, &height);
    size_callback(p, width, height);
    return 0;
}

static int set_scene(struct player *p, struct ngl_scene *scene)
{
    int ret;

    if (p->enable_ui) {
        ret = add_progress_bar(p, scene);
        if (ret < 0)
            return ret;
    }
    ret = ngl_set_scene(p->ngl, scene);
    if (ret < 0) {
        p->pgbar_opacity_node  = NULL;
        p->pgbar_duration_node = NULL;
        p->pgbar_text_node     = NULL;
    }

    const struct ngl_scene_params *params = ngl_scene_get_params(scene);
    if ((ret = set_duration(p, params->duration)) < 0 ||
        (ret = set_framerate(p, params->framerate)) < 0 ||
        (ret = set_aspect_ratio(p, params->aspect_ratio)) < 0)
        return ret;

    return 0;
}

int player_init(struct player *p, const char *win_title, struct ngl_scene *scene,
                const struct ngl_config *cfg, int enable_ui)
{
    memset(p, 0, sizeof(*p));

    if (init_window() < 0)
        return -1;

    p->window = get_window(win_title, cfg->width, cfg->height);
    if (!p->window) {
        SDL_Quit();
        return -1;
    }

    const struct ngl_scene_params *params = ngl_scene_get_params(scene);

    p->clock_off = -1;
    p->lasthover = -1;
    p->text_last_frame_index = -1;
    p->duration_f = params->duration;
    p->duration = (int64_t)(params->duration * 1000000.0);
    p->enable_ui = enable_ui;
    p->framerate[0] = 60;
    p->framerate[1] = 1;

    if (!params->framerate[0] || !params->framerate[1]) {
        fprintf(stderr, "Invalid framerate %d/%d\n", params->framerate[0], params->framerate[1]);
        return -1;
    }
    memcpy(p->framerate, params->framerate, sizeof(p->framerate));
    p->duration_i = llrint(p->duration_f * params->framerate[0] / (double)params->framerate[1]);

    p->ngl_config = *cfg;

    p->aspect[0] = params->aspect_ratio[0];
    p->aspect[1] = params->aspect_ratio[1];

    static const int refresh_rate[2] = {1, 60};
    p->ngl_config.hud_refresh_rate[0] = refresh_rate[0];
    p->ngl_config.hud_refresh_rate[1] = refresh_rate[1];
    p->ngl_config.hud_measure_window = refresh_rate[1] / (4 * refresh_rate[0]); /* 1/4-second measurement window */

    int ww, wh, dw, dh;
    SDL_GetWindowSize(p->window, &ww, &wh);
    SDL_GL_GetDrawableSize(p->window, &dw, &dh);
    p->ngl_config.hud_scale = dw / ww;

    int ret = wsi_set_ngl_config(&p->ngl_config, p->window);
    if (ret < 0)
        return ret;

    p->ngl = ngl_create();
    if (!p->ngl)
        return -1;

    ret = ngl_configure(p->ngl, &p->ngl_config);
    if (ret < 0)
        return ret;

    return set_scene(p, scene);
}

void player_uninit(struct player *p)
{
    if (!p)
        return;

    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_USEREVENT)
            free(event.user.data1);

    ngl_freep(&p->ngl);
    SDL_DestroyWindow(p->window);
    SDL_Quit();
}

static int handle_scene(struct player *p, const void *data)
{
    struct ngl_scene *scene = ngl_scene_create();
    if (!scene)
        return NGL_ERROR_MEMORY;
    int ret = ngl_scene_init_from_str(scene, data);
    if (ret < 0)
        goto end;
    ret = set_scene(p, scene);
end:
    ngl_scene_unrefp(&scene);
    return ret;
}

static int handle_clearcolor(struct player *p, const void *data)
{
    memcpy(p->ngl_config.clear_color, data, sizeof(p->ngl_config.clear_color));
    return 0;
}

static int handle_samples(struct player *p, const void *data)
{
#ifdef _WIN32
    if (p->ngl_config.backend == NGL_BACKEND_OPENGL ||
        p->ngl_config.backend == NGL_BACKEND_OPENGLES) {
        const int32_t *samples = data;
        if (*samples != p->ngl_config.samples) {
            fprintf(stderr, "MSAA cannot be reconfigured on Windows, "
                            "the player needs to be restarted instead\n");
            return 0;
        }
    }
#endif
    memcpy(&p->ngl_config.samples, data, sizeof(p->ngl_config.samples));
    return 0;
}

static int handle_reconfigure(struct player *p, const void *data)
{
    return ngl_configure(p->ngl, &p->ngl_config);
}

typedef int (*handle_func)(struct player *p, const void *data);

static const handle_func handle_map[] = {
    [PLAYER_SIGNAL_SCENE]        = handle_scene,
    [PLAYER_SIGNAL_CLEARCOLOR]   = handle_clearcolor,
    [PLAYER_SIGNAL_SAMPLES]      = handle_samples,
    [PLAYER_SIGNAL_RECONFIGURE]  = handle_reconfigure,
};

void player_main_loop(struct player *p)
{
    int run = 1;
    while (run) {
        update_time(p, -1);
        update_pgbar(p);
        ngl_draw(p->ngl, p->frame_time);
        if (p->seeking) {
            reset_running_time(p);
            p->seeking = 0;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                run = 0;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    run = 0;
                else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    size_callback(p, event.window.data1, event.window.data2);
                break;
            case SDL_KEYDOWN:
                run = key_callback(p, &event.key) == 0;
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_buttondown_callback(p, &event.button);
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_buttonup_callback(p, &event.button);
                break;
            case SDL_MOUSEMOTION:
                mouse_pos_callback(p, &event.motion);
                break;
            case SDL_USEREVENT:
                run = handle_map[event.user.code](p, event.user.data1) == 0;
                free(event.user.data1);
                p->text_last_frame_index = -1;
                p->lasthover = gettime_relative();
                break;
            }
        }
    }
}
