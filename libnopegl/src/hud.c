/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2016-2022 GoPro Inc.
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

#include "config.h"

#if HAVE_USELOCALE
# define _POSIX_C_SOURCE 200809L
# include <locale.h>
# ifdef __APPLE__
#  include <xlocale.h>
# endif
#elif defined(_WIN32)
# include <locale.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "drawutils.h"
#include "hud.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/block.h"
#include "ngpu/ctx.h"
#include "ngpu/graphics_state.h"
#include "ngpu/pgcraft.h"
#include "ngpu/type.h"
#include "node_block.h"
#include "node_buffer.h"
#include "node_texture.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "utils/memory.h"
#include "utils/time.h"

struct transforms_block {
    NGLI_ALIGNED_MAT(modelview_matrix);
    NGLI_ALIGNED_MAT(projection_matrix);
};

struct hud {
    struct ngl_ctx *ctx;

    int measure_window;
    int refresh_rate[2];
    const char *export_filename;
    int scale;

#if HAVE_USELOCALE
    locale_t c_locale;
#endif
    struct darray widgets;
    uint32_t bg_color_u32;
    FILE *fp_export;
    struct bstr *csv_line;
    struct canvas canvas;
    double refresh_rate_interval;
    double last_refresh_time;

    struct ngpu_pgcraft *crafter;
    struct ngpu_texture *texture;
    struct ngpu_buffer *coords;
    struct ngpu_block transforms_block;
    struct pipeline_compat *pipeline_compat;
    struct ngpu_graphics_state graphics_state;
};

#define WIDGET_PADDING 4
#define WIDGET_MARGIN  2

#define LATENCY_WIDGET_TEXT_LEN     20
#define MEMORY_WIDGET_TEXT_LEN      25
#define ACTIVITY_WIDGET_TEXT_LEN    12
#define DRAWCALL_WIDGET_TEXT_LEN    12

enum {
    LATENCY_UPDATE_CPU,
    LATENCY_DRAW_CPU,
    LATENCY_TOTAL_CPU,
    LATENCY_DRAW_GPU,
    NB_LATENCY
};

enum {
    MEMORY_BUFFERS_CPU,
    MEMORY_BUFFERS_GPU,
    MEMORY_BLOCKS_CPU,
    MEMORY_BLOCKS_GPU,
    MEMORY_TEXTURES,
    NB_MEMORY
};

enum {
    ACTIVITY_BUFFERS,
    ACTIVITY_BLOCKS,
    ACTIVITY_MEDIAS,
    ACTIVITY_TEXTURES,
    NB_ACTIVITY
};

enum {
    DRAWCALL_COMPUTES,
    DRAWCALL_GRAPHICCONFIGS,
    DRAWCALL_DRAWS,
    DRAWCALL_RTTS,
    NB_DRAWCALL
};

#define BUFFER_NODES                \
    NGL_NODE_ANIMATEDBUFFERFLOAT,   \
    NGL_NODE_ANIMATEDBUFFERVEC2,    \
    NGL_NODE_ANIMATEDBUFFERVEC3,    \
    NGL_NODE_ANIMATEDBUFFERVEC4,    \
    NGL_NODE_BUFFERBYTE,            \
    NGL_NODE_BUFFERBVEC2,           \
    NGL_NODE_BUFFERBVEC3,           \
    NGL_NODE_BUFFERBVEC4,           \
    NGL_NODE_BUFFERINT,             \
    NGL_NODE_BUFFERIVEC2,           \
    NGL_NODE_BUFFERIVEC3,           \
    NGL_NODE_BUFFERIVEC4,           \
    NGL_NODE_BUFFERSHORT,           \
    NGL_NODE_BUFFERSVEC2,           \
    NGL_NODE_BUFFERSVEC3,           \
    NGL_NODE_BUFFERSVEC4,           \
    NGL_NODE_BUFFERUBYTE,           \
    NGL_NODE_BUFFERUBVEC2,          \
    NGL_NODE_BUFFERUBVEC3,          \
    NGL_NODE_BUFFERUBVEC4,          \
    NGL_NODE_BUFFERUINT,            \
    NGL_NODE_BUFFERUIVEC2,          \
    NGL_NODE_BUFFERUIVEC3,          \
    NGL_NODE_BUFFERUIVEC4,          \
    NGL_NODE_BUFFERUSHORT,          \
    NGL_NODE_BUFFERUSVEC2,          \
    NGL_NODE_BUFFERUSVEC3,          \
    NGL_NODE_BUFFERUSVEC4,          \
    NGL_NODE_BUFFERFLOAT,           \
    NGL_NODE_BUFFERVEC2,            \
    NGL_NODE_BUFFERVEC3,            \
    NGL_NODE_BUFFERVEC4,            \
    NGL_NODE_BUFFERMAT4

static const struct {
    const char *label;
    const uint32_t color;
    char unit;
} latency_specs[] = {
    [LATENCY_UPDATE_CPU] = {"update CPU", 0xF43DF4FF, 'u'},
    [LATENCY_DRAW_CPU]   = {"draw   CPU", 0x3DF4F4FF, 'u'},
    [LATENCY_TOTAL_CPU]  = {"total  CPU", 0xF4F43DFF, 'u'},
    [LATENCY_DRAW_GPU]   = {"draw   GPU", 0x3DF43DFF, 'n'},
};

static const struct {
    const char *label;
    const uint32_t *node_types;
    uint32_t color;
} memory_specs[] = {
    [MEMORY_BUFFERS_CPU] = {
        .label="Buffers CPU",
        .node_types=(const uint32_t[]){BUFFER_NODES, NGLI_NODE_NONE},
        .color=0xD632FFFF,
    },
    [MEMORY_BUFFERS_GPU] = {
        .label="Buffers GPU",
        .node_types=(const uint32_t[]){BUFFER_NODES, NGLI_NODE_NONE},
        .color=0x3284FFFF,
    },
    [MEMORY_BLOCKS_CPU] = {
        .label="Blocks CPU",
        .node_types=(const uint32_t[]){NGL_NODE_BLOCK, NGLI_NODE_NONE},
        .color=0x32FF84FF,
    },
    [MEMORY_BLOCKS_GPU] = {
        .label="Blocks GPU",
        .node_types=(const uint32_t[]){NGL_NODE_BLOCK, NGL_NODE_COLORSTATS, NGLI_NODE_NONE},
        .color=0xD6FF32FF,
    },
    [MEMORY_TEXTURES] = {
        .label="Textures",
        .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURE3D, NGLI_NODE_NONE},
        .color=0xFF3232FF,
    },
};

static const struct activity_spec {
    const char *label;
    const uint32_t *node_types;
} activity_specs[] = {
    [ACTIVITY_BUFFERS] = {
        .label="Buffers",
        .node_types=(const uint32_t[]){BUFFER_NODES, NGLI_NODE_NONE},
    },
    [ACTIVITY_BLOCKS] = {
        .label="Blocks",
        .node_types=(const uint32_t[]){NGL_NODE_BLOCK, NGL_NODE_COLORSTATS, NGLI_NODE_NONE},
    },
    [ACTIVITY_MEDIAS] = {
        .label="Medias",
        .node_types=(const uint32_t[]){NGL_NODE_MEDIA, NGLI_NODE_NONE},
    },
    [ACTIVITY_TEXTURES] = {
        .label="Textures",
        .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURE3D, NGLI_NODE_NONE},
    },
};

static const struct drawcall_spec {
    const char *label;
    const uint32_t *node_types;
} drawcall_specs[] = {
    [DRAWCALL_COMPUTES] = {
        .label="Computes",
        .node_types=(const uint32_t[]){NGL_NODE_COMPUTE, NGLI_NODE_NONE},
    },
    [DRAWCALL_GRAPHICCONFIGS] = {
        .label="GraphicCfgs",
        .node_types=(const uint32_t[]){NGL_NODE_GRAPHICCONFIG, NGLI_NODE_NONE},
    },
    [DRAWCALL_DRAWS] = {
        .label="Draws",
        .node_types=(const uint32_t[]){
            NGL_NODE_DRAW,
            NGL_NODE_DRAWCOLOR,
            NGL_NODE_DRAWGRADIENT,
            NGL_NODE_DRAWGRADIENT4,
            NGL_NODE_DRAWHISTOGRAM,
            NGL_NODE_DRAWPATH,
            NGL_NODE_DRAWTEXTURE,
            NGL_NODE_DRAWWAVEFORM,
            NGLI_NODE_NONE
        },
    },
    [DRAWCALL_RTTS] = {
        .label="RTTs",
        .node_types=(const uint32_t[]){NGL_NODE_RENDERTOTEXTURE, NGLI_NODE_NONE},
    },
};

NGLI_STATIC_ASSERT(NGLI_ARRAY_NB(latency_specs)  == NB_LATENCY,  "hud nb latency");
NGLI_STATIC_ASSERT(NGLI_ARRAY_NB(memory_specs)   == NB_MEMORY,   "hud nb memory");
NGLI_STATIC_ASSERT(NGLI_ARRAY_NB(activity_specs) == NB_ACTIVITY, "hud nb activity");
NGLI_STATIC_ASSERT(NGLI_ARRAY_NB(drawcall_specs) == NB_DRAWCALL, "hud nb drawcall");

enum widget_type {
    WIDGET_LATENCY,
    WIDGET_MEMORY,
    WIDGET_ACTIVITY,
    WIDGET_DRAWCALL,
};

struct data_graph {
    int64_t *values;
    size_t nb_values;
    size_t count;
    size_t pos;
    int64_t min;
    int64_t max;
    int64_t amin; // all-time min
    int64_t amax; // all-time max
};

struct latency_measure {
    int64_t *times;
    int count;
    int pos;
    int64_t total_times;
};

struct widget_latency {
    struct latency_measure measures[NB_LATENCY];
};

struct widget_memory {
    struct darray nodes[NB_MEMORY];
    size_t sizes[NB_MEMORY];
};

struct widget_activity {
    struct darray nodes;
    int nb_actives;
};

struct widget_drawcall {
    struct darray nodes;
    int nb_draws;
};

struct widget {
    enum widget_type type;
    struct rect rect;
    int text_x, text_y;
    struct rect graph_rect;
    struct data_graph *data_graph;
    const void *user_data;
    void *priv_data;
};

struct widget_spec {
    int text_cols, text_rows;
    int graph_w, graph_h;
    size_t nb_data_graph;
    size_t priv_size;
    int (*init)(struct hud *s, struct widget *widget);
    void (*make_stats)(struct hud *s, struct widget *widget);
    void (*draw)(struct hud *s, struct widget *widget);
    void (*csv_header)(struct hud *s, struct widget *widget, struct bstr *dst);
    void (*csv_report)(struct hud *s, struct widget *widget, struct bstr *dst);
    void (*uninit)(struct hud *s, struct widget *widget);
};

/* Widget init */

static int widget_latency_init(struct hud *s, struct widget *widget)
{
    struct widget_latency *priv = widget->priv_data;

    ngli_assert(NB_LATENCY == NGLI_ARRAY_NB(priv->measures));

    s->measure_window = NGLI_MAX(s->measure_window, 1);
    for (size_t i = 0; i < NB_LATENCY; i++) {
        int64_t *times = ngli_calloc((size_t)s->measure_window, sizeof(*times));
        if (!times)
            return NGL_ERROR_MEMORY;
        priv->measures[i].times = times;
    }

    return 0;
}

static int make_nodes_set(struct ngl_scene *scene, struct darray *nodes_list, const uint32_t *node_types)
{
    if (!scene)
        return 0;

    ngli_darray_init(nodes_list, sizeof(struct ngl_node *), 0);
    for (size_t n = 0; node_types[n] != NGLI_NODE_NONE; n++) {
        const uint32_t node_type = node_types[n];
        const struct ngl_node **nodes = ngli_darray_data(&scene->nodes);
        for (size_t i = 0; i < ngli_darray_count(&scene->nodes); i++) {
            const struct ngl_node *node = nodes[i];
            if (node->cls->id != node_type)
                continue;
            if (!ngli_darray_push(nodes_list, &node))
                return NGL_ERROR_MEMORY;
        }
    }

    return 0;
}

static int widget_memory_init(struct hud *s, struct widget *widget)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_scene *scene = ctx->scene;
    struct widget_memory *priv = widget->priv_data;

    for (size_t i = 0; i < NB_MEMORY; i++) {
        const uint32_t *node_types = memory_specs[i].node_types;
        int ret = make_nodes_set(scene, &priv->nodes[i], node_types);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int widget_activity_init(struct hud *s, struct widget *widget)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_scene *scene = ctx->scene;
    const struct activity_spec *spec = widget->user_data;
    struct widget_activity *priv = widget->priv_data;
    const uint32_t *node_types = spec->node_types;
    return make_nodes_set(scene, &priv->nodes, node_types);
}

static int widget_drawcall_init(struct hud *s, struct widget *widget)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_scene *scene = ctx->scene;
    const struct drawcall_spec *spec = widget->user_data;
    struct widget_drawcall *priv = widget->priv_data;
    const uint32_t *node_types = spec->node_types;
    return make_nodes_set(scene, &priv->nodes, node_types);
}

/* Widget update */

static void register_time(struct hud *s, struct latency_measure *m, int64_t t)
{
    m->total_times = m->total_times - m->times[m->pos] + t;
    m->times[m->pos] = t;
    m->pos = (m->pos + 1) % s->measure_window;
    m->count = NGLI_MIN(m->count + 1, s->measure_window);
}

/* Widget make stats */

static void widget_latency_make_stats(struct hud *s, struct widget *widget)
{
    struct ngl_ctx *ctx = s->ctx;
    struct widget_latency *priv = widget->priv_data;

    register_time(s, &priv->measures[LATENCY_UPDATE_CPU], ctx->cpu_update_time);
    register_time(s, &priv->measures[LATENCY_DRAW_CPU],   ctx->cpu_draw_time);
    register_time(s, &priv->measures[LATENCY_TOTAL_CPU],  ctx->cpu_update_time + ctx->cpu_draw_time);
    register_time(s, &priv->measures[LATENCY_DRAW_GPU],   ctx->gpu_draw_time);
}

static void widget_memory_make_stats(struct hud *s, struct widget *widget)
{
    struct widget_memory *priv = widget->priv_data;

    struct darray *nodes_buf_array_cpu = &priv->nodes[MEMORY_BUFFERS_CPU];
    struct ngl_node **nodes_buf_cpu = ngli_darray_data(nodes_buf_array_cpu);
    priv->sizes[MEMORY_BUFFERS_CPU] = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_buf_array_cpu); i++)
        priv->sizes[MEMORY_BUFFERS_CPU] += ngli_node_buffer_get_cpu_size(nodes_buf_cpu[i]);

    struct darray *nodes_buf_array_gpu = &priv->nodes[MEMORY_BUFFERS_GPU];
    struct ngl_node **nodes_buf_gpu = ngli_darray_data(nodes_buf_array_gpu);
    priv->sizes[MEMORY_BUFFERS_GPU] = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_buf_array_gpu); i++)
        priv->sizes[MEMORY_BUFFERS_GPU] += ngli_node_buffer_get_gpu_size(nodes_buf_gpu[i]);

    struct darray *nodes_blk_array_cpu = &priv->nodes[MEMORY_BLOCKS_CPU];
    struct ngl_node **nodes_blk_cpu = ngli_darray_data(nodes_blk_array_cpu);
    priv->sizes[MEMORY_BLOCKS_CPU] = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_blk_array_cpu); i++)
        priv->sizes[MEMORY_BLOCKS_CPU] += ngli_node_block_get_cpu_size(nodes_blk_cpu[i]);

    struct darray *nodes_blk_array_gpu = &priv->nodes[MEMORY_BLOCKS_GPU];
    struct ngl_node **nodes_blk_gpu = ngli_darray_data(nodes_blk_array_gpu);
    priv->sizes[MEMORY_BLOCKS_GPU] = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_blk_array_gpu); i++)
        priv->sizes[MEMORY_BLOCKS_GPU] += ngli_node_block_get_gpu_size(nodes_blk_gpu[i]);

    struct darray *nodes_tex_array = &priv->nodes[MEMORY_TEXTURES];
    struct ngl_node **nodes_tex = ngli_darray_data(nodes_tex_array);
    priv->sizes[MEMORY_TEXTURES] = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_tex_array); i++) {
        const struct ngl_node *tex_node = nodes_tex[i];
        const struct texture_info *texture_info = tex_node->priv_data;
        priv->sizes[MEMORY_TEXTURES] += ngli_image_get_memory_size(&texture_info->image)
                                      * tex_node->is_active;
    }
}

static void widget_activity_make_stats(struct hud *s, struct widget *widget)
{
    struct widget_activity *priv = widget->priv_data;
    struct darray *nodes_array = &priv->nodes;
    struct ngl_node **nodes = ngli_darray_data(nodes_array);
    priv->nb_actives = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_array); i++)
        priv->nb_actives += nodes[i]->is_active;
}

static void widget_drawcall_make_stats(struct hud *s, struct widget *widget)
{
    struct widget_drawcall *priv = widget->priv_data;
    struct darray *nodes_array = &priv->nodes;
    struct ngl_node **nodes = ngli_darray_data(nodes_array);
    priv->nb_draws = 0;
    for (size_t i = 0; i < ngli_darray_count(nodes_array); i++)
        priv->nb_draws += nodes[i]->draw_count;
}

/* Draw utils */

static inline uint8_t *set_color(uint8_t *p, uint32_t rgba)
{
    p[0] = (uint8_t)(rgba >> 24);
    p[1] = rgba >> 16 & 0xff;
    p[2] = rgba >>  8 & 0xff;
    p[3] = rgba       & 0xff;
    return p + 4;
}

static inline int get_pixel_pos(struct hud *s, int px, int py)
{
    return (py * s->canvas.w + px) * 4;
}

static inline void set_color_at(struct hud *s, int px, int py, uint32_t rgba)
{
    uint8_t *p = s->canvas.buf + get_pixel_pos(s, px, py);
    set_color(p, rgba);
}

static inline void set_color_at_column(struct hud *s, int px, int py, int height, uint32_t rgba)
{
    uint8_t *p = s->canvas.buf + get_pixel_pos(s, px, py);
    const int sign = height >= 0 ? 1 : -1;
    for (int h = 0; h < height; h++) {
        set_color(p, rgba);
        p += sign * s->canvas.w * 4;
    }
}

static void draw_block_graph(struct hud *s,
                             const struct data_graph *d,
                             const struct rect *rect,
                             int64_t graph_min, int64_t graph_max,
                             const uint32_t c)
{
    const int64_t graph_h = graph_max - graph_min;
    const float vscale = (float)rect->h / (float)graph_h;
    const size_t start = (d->nb_values + d->pos - d->count) % d->nb_values;

    for (size_t k = 0; k < d->count; k++) {
        const int64_t v = d->values[(start + k) % d->nb_values];
        const int h = (int)((float)(v - graph_min) * vscale);
        const int y = NGLI_CLAMP(rect->h - h, 0, rect->h);
        set_color_at_column(s, rect->x + (int)k, rect->y + y, h, c);
    }
}

static void draw_line_graph(struct hud *s,
                            const struct data_graph *d,
                            const struct rect *rect,
                            int64_t graph_min, int64_t graph_max,
                            const uint32_t c)
{
    const int64_t graph_h = graph_max - graph_min;
    const float vscale = (float)rect->h / (float)graph_h;
    const size_t start = (d->nb_values + d->pos - d->count) % d->nb_values;
    int prev_y;

    for (size_t k = 0; k < d->count; k++) {
        const int64_t v = d->values[(start + k) % d->nb_values];
        const int h = (int)((float)(v - graph_min) * vscale);
        const int y = NGLI_CLAMP(rect->h - 1 - h, 0, rect->h - 1);

        set_color_at(s, rect->x + (int)k, rect->y + y, c);
        if (k)
            set_color_at_column(s, rect->x + (int)k, rect->y + prev_y, y - prev_y, c);
        prev_y = y;
    }
}

static void print_text(struct hud *s, int x, int y, const char *buf, const uint32_t c)
{
    ngli_drawutils_print(&s->canvas, x, y, buf, c);
}

static void widgets_clear(struct hud *s)
{
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++)
        ngli_drawutils_draw_rect(&s->canvas, &widgets[i].rect, s->bg_color_u32);
}

/* Widget draw */

static void register_graph_value(struct data_graph *d, int64_t v)
{
    const int64_t old_v = d->values[d->pos];

    d->values[d->pos] = v;
    d->pos = (d->pos + 1) % d->nb_values;
    d->count = NGLI_MIN(d->count + 1, d->nb_values);

    /* update min */
    if (old_v == d->min) {
        d->min = d->values[0];
        for (int i = 1; i < d->nb_values; i++)
            d->min = NGLI_MIN(d->min, d->values[i]);
    } else if (v < d->min) {
        d->min = v;
    }
    d->amin = NGLI_MIN(d->amin, d->min);

    /* update max */
    if (old_v == d->max) {
        d->max = d->values[0];
        for (int i = 1; i < d->nb_values; i++)
            d->max = NGLI_MAX(d->max, d->values[i]);
    } else if (v > d->max) {
        d->max = v;
    }
    d->amax = NGLI_MAX(d->amax, d->max);
}

static int64_t get_latency_avg(const struct widget_latency *priv, size_t id)
{
    const struct latency_measure *m = &priv->measures[id];
    return m->total_times / m->count / (latency_specs[id].unit == 'u' ? 1 : 1000);
}

static void widget_latency_draw(struct hud *s, struct widget *widget)
{
    struct widget_latency *priv = widget->priv_data;

    char buf[LATENCY_WIDGET_TEXT_LEN + 1];
    for (size_t i = 0; i < NB_LATENCY; i++) {
        const int64_t t = get_latency_avg(priv, i);

        snprintf(buf, sizeof(buf), "%s %5" PRId64 "usec", latency_specs[i].label, t);
        print_text(s, widget->text_x, widget->text_y + (int)i * NGLI_FONT_H, buf, latency_specs[i].color);
        register_graph_value(&widget->data_graph[i], t);
    }

    int64_t graph_min = widget->data_graph[0].min;
    int64_t graph_max = widget->data_graph[0].max;
    for (size_t i = 1; i < NB_LATENCY; i++) {
        graph_min = NGLI_MIN(graph_min, widget->data_graph[i].min);
        graph_max = NGLI_MAX(graph_max, widget->data_graph[i].max);
    }

    const int64_t graph_h = graph_max - graph_min;
    if (graph_h) {
        for (size_t i = 0; i < NB_LATENCY; i++)
            draw_line_graph(s, &widget->data_graph[i], &widget->graph_rect,
                            graph_min, graph_max, latency_specs[i].color);
    }
}

static void widget_memory_draw(struct hud *s, struct widget *widget)
{
    struct widget_memory *priv = widget->priv_data;
    char buf[MEMORY_WIDGET_TEXT_LEN + 1];

    for (size_t i = 0; i < NB_MEMORY; i++) {
        const size_t size = priv->sizes[i];
        const uint32_t color = memory_specs[i].color;
        const char *label = memory_specs[i].label;

        if (size < 1024)
            snprintf(buf, sizeof(buf), "%-12s %zu", label, size);
        else if (size < 1024 * 1024)
            snprintf(buf, sizeof(buf), "%-12s %zuK", label, size / 1024);
        else if (size < 1024 * 1024 * 1024)
            snprintf(buf, sizeof(buf), "%-12s %zuM", label, size / (1024 * 1024));
        else
            snprintf(buf, sizeof(buf), "%-12s %zuG", label, size / (1024 * 1024 * 1024));
        print_text(s, widget->text_x, widget->text_y + (int)i * NGLI_FONT_H, buf, color);

        const int64_t size_i64 = (int64_t)NGLI_MIN(size, INT64_MAX);
        register_graph_value(&widget->data_graph[i], size_i64);
    }

    int64_t graph_min = widget->data_graph[0].min;
    int64_t graph_max = widget->data_graph[0].max;
    for (size_t i = 1; i < NB_MEMORY; i++) {
        graph_min = NGLI_MIN(graph_min, widget->data_graph[i].min);
        graph_max = NGLI_MAX(graph_max, widget->data_graph[i].max);
    }

    const int64_t graph_h = graph_max - graph_min;
    if (graph_h) {
        for (size_t i = 0; i < NB_MEMORY; i++)
            draw_line_graph(s, &widget->data_graph[i], &widget->graph_rect,
                            graph_min, graph_max, memory_specs[i].color);
    }
}

static void widget_activity_draw(struct hud *s, struct widget *widget)
{
    struct widget_activity *priv = widget->priv_data;
    const struct activity_spec *spec = widget->user_data;
    const uint32_t color = 0x3df4f4ff;

    char buf[ACTIVITY_WIDGET_TEXT_LEN + 1];
    snprintf(buf, sizeof(buf), "%d/%zu", priv->nb_actives, priv->nodes.count);
    print_text(s, widget->text_x, widget->text_y, spec->label, color);
    print_text(s, widget->text_x, widget->text_y + NGLI_FONT_H, buf, color);

    struct data_graph *d = &widget->data_graph[0];
    register_graph_value(d, priv->nb_actives);
    draw_block_graph(s, d, &widget->graph_rect, d->amin, d->amax, color);
}

static void widget_drawcall_draw(struct hud *s, struct widget *widget)
{
    struct widget_drawcall *priv = widget->priv_data;
    const struct drawcall_spec *spec = widget->user_data;
    const uint32_t color = 0x3df43dff;

    char buf[DRAWCALL_WIDGET_TEXT_LEN + 1];
    snprintf(buf, sizeof(buf), "%d", priv->nb_draws);
    print_text(s, widget->text_x, widget->text_y, spec->label, color);
    print_text(s, widget->text_x, widget->text_y + NGLI_FONT_H, buf, color);

    struct data_graph *d = &widget->data_graph[0];
    register_graph_value(d, priv->nb_draws);
    draw_block_graph(s, d, &widget->graph_rect, d->amin, d->amax, color);
}

/* Widget CSV header */

static void widget_latency_csv_header(struct hud *s, struct widget *widget, struct bstr *dst)
{
    for (size_t i = 0; i < NB_LATENCY; i++)
        ngli_bstr_printf(dst, "%s%s", i ? "," : "", latency_specs[i].label);
}

static void widget_memory_csv_header(struct hud *s, struct widget *widget, struct bstr *dst)
{
    for (size_t i = 0; i < NB_MEMORY; i++)
        ngli_bstr_printf(dst, "%s%s memory", i ? "," : "", memory_specs[i].label);
}

static void widget_activity_csv_header(struct hud *s, struct widget *widget, struct bstr *dst)
{
    const struct activity_spec *spec = widget->user_data;
    ngli_bstr_printf(dst, "%s count,%s total", spec->label, spec->label);
}

static void widget_drawcall_csv_header(struct hud *s, struct widget *widget, struct bstr *dst)
{
    const struct drawcall_spec *spec = widget->user_data;
    ngli_bstr_print(dst, spec->label);
}

/* Widget CSV report */

static void widget_latency_csv_report(struct hud *s, struct widget *widget, struct bstr *dst)
{
    const struct widget_latency *priv = widget->priv_data;
    for (size_t i = 0; i < NB_LATENCY; i++) {
        const int64_t t = get_latency_avg(priv, i);
        ngli_bstr_printf(dst, "%s%"PRId64, i ? "," : "", t);
    }
}

static void widget_memory_csv_report(struct hud *s, struct widget *widget, struct bstr *dst)
{
    const struct widget_memory *priv = widget->priv_data;
    for (size_t i = 0; i < NB_MEMORY; i++) {
        const size_t size = priv->sizes[i];
        ngli_bstr_printf(dst, "%s%zu", i ? "," : "", size);
    }
}

static void widget_activity_csv_report(struct hud *s, struct widget *widget, struct bstr *dst)
{
    const struct widget_activity *priv = widget->priv_data;
    ngli_bstr_printf(dst, "%d,%zu", priv->nb_actives, priv->nodes.count);
}

static void widget_drawcall_csv_report(struct hud *s, struct widget *widget, struct bstr *dst)
{
    const struct widget_drawcall *priv = widget->priv_data;
    ngli_bstr_printf(dst, "%d", priv->nb_draws);
}

/* Widget uninit */

static void widget_latency_uninit(struct hud *s, struct widget *widget)
{
    struct widget_latency *priv = widget->priv_data;
    for (size_t i = 0; i < NB_LATENCY; i++)
        ngli_free(priv->measures[i].times);
}

static void widget_memory_uninit(struct hud *s, struct widget *widget)
{
    struct widget_memory *priv = widget->priv_data;
    for (size_t i = 0; i < NB_MEMORY; i++)
        ngli_darray_reset(&priv->nodes[i]);
}

static void widget_activity_uninit(struct hud *s, struct widget *widget)
{
    struct widget_activity *priv = widget->priv_data;
    ngli_darray_reset(&priv->nodes);
}

static void widget_drawcall_uninit(struct hud *s, struct widget *widget)
{
    struct widget_drawcall *priv = widget->priv_data;
    ngli_darray_reset(&priv->nodes);
}

static const struct widget_spec widget_specs[] = {
    [WIDGET_LATENCY] = {
        .text_cols     = LATENCY_WIDGET_TEXT_LEN,
        .text_rows     = NB_LATENCY,
        .graph_w       = 320,
        .nb_data_graph = NB_LATENCY,
        .priv_size     = sizeof(struct widget_latency),
        .init          = widget_latency_init,
        .make_stats    = widget_latency_make_stats,
        .draw          = widget_latency_draw,
        .csv_header    = widget_latency_csv_header,
        .csv_report    = widget_latency_csv_report,
        .uninit        = widget_latency_uninit,
    },
    [WIDGET_MEMORY] = {
        .text_cols     = MEMORY_WIDGET_TEXT_LEN,
        .text_rows     = NB_MEMORY,
        .graph_w       = 285,
        .nb_data_graph = NB_MEMORY,
        .priv_size     = sizeof(struct widget_memory),
        .init          = widget_memory_init,
        .make_stats    = widget_memory_make_stats,
        .draw          = widget_memory_draw,
        .csv_header    = widget_memory_csv_header,
        .csv_report    = widget_memory_csv_report,
        .uninit        = widget_memory_uninit,
    },
    [WIDGET_ACTIVITY] = {
        .text_cols     = ACTIVITY_WIDGET_TEXT_LEN,
        .text_rows     = 2,
        .graph_h       = 40,
        .nb_data_graph = 1,
        .priv_size     = sizeof(struct widget_activity),
        .init          = widget_activity_init,
        .make_stats    = widget_activity_make_stats,
        .draw          = widget_activity_draw,
        .csv_header    = widget_activity_csv_header,
        .csv_report    = widget_activity_csv_report,
        .uninit        = widget_activity_uninit,
    },
    [WIDGET_DRAWCALL]  = {
        .text_cols     = DRAWCALL_WIDGET_TEXT_LEN,
        .text_rows     = 2,
        .graph_h       = 40,
        .nb_data_graph = 1,
        .priv_size     = sizeof(struct widget_drawcall),
        .init          = widget_drawcall_init,
        .make_stats    = widget_drawcall_make_stats,
        .draw          = widget_drawcall_draw,
        .csv_header    = widget_drawcall_csv_header,
        .csv_report    = widget_drawcall_csv_report,
        .uninit        = widget_drawcall_uninit,
    },
};

static inline int get_widget_width(enum widget_type type)
{
    const struct widget_spec *spec = &widget_specs[type];
    const int horizontal_layout = !spec->graph_h;
    return spec->graph_w
         + spec->text_cols * NGLI_FONT_W
         + WIDGET_PADDING * (2 + horizontal_layout);
}

static inline int get_widget_height(enum widget_type type)
{
    const struct widget_spec *spec = &widget_specs[type];
    const int vertical_layout = !!spec->graph_h;
    return spec->graph_h
         + spec->text_rows * NGLI_FONT_H
         + WIDGET_PADDING * (2 + vertical_layout);
}

static int create_widget(struct hud *s, enum widget_type type, const void *user_data, int x, int y)
{
    if (x < 0)
        x = s->canvas.w + x;
    if (y < 0)
        y = s->canvas.h + y;

    const struct widget_spec *spec = &widget_specs[type];

    ngli_assert(spec->text_cols && spec->text_rows);
    ngli_assert(spec->graph_w ^ spec->graph_h);
    ngli_assert(spec->nb_data_graph);

    const int horizontal_layout = !spec->graph_h;
    struct widget widget = {
        .type      = type,
        .rect.x    = x,
        .rect.y    = y,
        .rect.w    = get_widget_width(type),
        .rect.h    = get_widget_height(type),
        .text_x    = x + WIDGET_PADDING,
        .text_y    = y + WIDGET_PADDING,
        .user_data = user_data,
    };

    if (horizontal_layout) {
        widget.graph_rect.x = x + spec->text_cols * NGLI_FONT_W + WIDGET_PADDING * 2;
        widget.graph_rect.y = y + WIDGET_PADDING;
        widget.graph_rect.w = spec->graph_w;
        widget.graph_rect.h = widget.rect.h - WIDGET_PADDING * 2;
    } else {
        widget.graph_rect.x = x + WIDGET_PADDING;
        widget.graph_rect.y = y + spec->text_rows * NGLI_FONT_H + WIDGET_PADDING * 2;
        widget.graph_rect.w = widget.rect.w - WIDGET_PADDING * 2;
        widget.graph_rect.h = spec->graph_h;
    }

    struct widget *widgetp = ngli_darray_push(&s->widgets, &widget);
    if (!widgetp)
        return NGL_ERROR_MEMORY;

    widgetp->priv_data = ngli_calloc(1, spec->priv_size);
    if (!widgetp->priv_data)
        return NGL_ERROR_MEMORY;

    widgetp->data_graph = ngli_calloc(spec->nb_data_graph, sizeof(*widgetp->data_graph));
    if (!widgetp->data_graph)
        return NGL_ERROR_MEMORY;
    for (size_t i = 0; i < spec->nb_data_graph; i++) {
        struct data_graph *d = &widgetp->data_graph[i];
        d->nb_values = (size_t)widgetp->graph_rect.w;
        d->values = ngli_calloc(d->nb_values, sizeof(*d->values));
        if (!d->values)
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void reset_widget(void *user_arg, void *data)
{
    struct hud *s = user_arg;
    struct widget *widget = data;
    widget_specs[widget->type].uninit(s, widget);
    ngli_free(widget->priv_data);
    for (size_t i = 0; i < widget_specs[widget->type].nb_data_graph; i++)
        ngli_free(widget->data_graph[i].values);
    ngli_free(widget->data_graph);
}

static int widgets_init(struct hud *s)
{
    ngli_darray_init(&s->widgets, sizeof(struct widget), 0);
    ngli_darray_set_free_func(&s->widgets, reset_widget, s);

    /* Smallest dimensions possible (in pixels) */
    const int latency_width  = get_widget_width(WIDGET_LATENCY);
    const int memory_width   = get_widget_width(WIDGET_MEMORY);
    const int activity_width = get_widget_width(WIDGET_ACTIVITY) * NB_ACTIVITY + WIDGET_MARGIN * (NB_ACTIVITY - 1);
    const int drawcall_width = get_widget_width(WIDGET_DRAWCALL) * NB_DRAWCALL + WIDGET_MARGIN * (NB_DRAWCALL - 1);

    s->canvas.w = WIDGET_MARGIN * 2
                + NGLI_MAX(NGLI_MAX(NGLI_MAX(latency_width, memory_width), activity_width), drawcall_width);

    s->canvas.h = WIDGET_MARGIN * 4
                + get_widget_height(WIDGET_LATENCY)
                + get_widget_height(WIDGET_MEMORY)
                + get_widget_height(WIDGET_ACTIVITY)
                + get_widget_height(WIDGET_DRAWCALL);

    /* Latency widget in the top-left */
    const int x_latency = WIDGET_MARGIN;
    const int y_latency = WIDGET_MARGIN;
    int ret = create_widget(s, WIDGET_LATENCY, NULL, x_latency, y_latency);
    if (ret < 0)
        return ret;

    /* Memory widget in the top-right */
    const int x_memory = WIDGET_MARGIN;
    const int y_memory = WIDGET_MARGIN + y_latency + get_widget_height(WIDGET_LATENCY);
    ret = create_widget(s, WIDGET_MEMORY, NULL, x_memory, y_memory);
    if (ret < 0)
        return ret;

    /* Activity nodes counter widgets in the bottom-left */
    int x_activity = WIDGET_MARGIN;
    const int y_activity = WIDGET_MARGIN + y_memory + get_widget_height(WIDGET_MEMORY);
    const int x_activity_step = get_widget_width(WIDGET_ACTIVITY) + WIDGET_MARGIN;
    for (size_t i = 0; i < NB_ACTIVITY; i++) {
        ret = create_widget(s, WIDGET_ACTIVITY, &activity_specs[i], x_activity, y_activity);
        if (ret < 0)
            return ret;
        x_activity += x_activity_step;
    }

    /* Draw-calls widgets in the bottom-right */
    int x_drawcall = WIDGET_MARGIN;
    const int y_drawcall = WIDGET_MARGIN + y_activity + get_widget_height(WIDGET_ACTIVITY);
    const int x_drawcall_step = get_widget_width(WIDGET_DRAWCALL) + WIDGET_MARGIN;
    for (size_t i = 0; i < NB_DRAWCALL; i++) {
        ret = create_widget(s, WIDGET_DRAWCALL, &drawcall_specs[i], x_drawcall, y_drawcall);
        if (ret < 0)
            return ret;
        x_drawcall += x_drawcall_step;
    }

    /* Call init on every widget */
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        ret = widget_specs[widget->type].init(s, widget);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void widget_drawcall_reset_draws(struct widget *widget)
{
    struct widget_drawcall *priv = widget->priv_data;
    for (size_t i = 0; i < NB_DRAWCALL; i++) {
        struct darray *nodes_array = &priv->nodes;
        struct ngl_node **nodes = ngli_darray_data(nodes_array);
        for (size_t j = 0; j < ngli_darray_count(nodes_array); j++)
            nodes[j]->draw_count = 0;
    }
}

static void widgets_make_stats(struct hud *s)
{
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].make_stats(s, widget);
    }
    /* HACK: reset drawcall draw counts after calling
     * widget_latency_make_stats(). This is needed here because several draws
     * can happen without update (for instance in case of a resize). */
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *w = &widgets[i];
        if (w->type == WIDGET_DRAWCALL)
            widget_drawcall_reset_draws(w);
    }
}

static void widgets_draw(struct hud *s)
{
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].draw(s, widget);
    }
}

static int widgets_csv_header(struct hud *s)
{
    s->fp_export = fopen(s->export_filename, "wb");
    if (!s->fp_export) {
        LOG(ERROR, "unable to open \"%s\" for writing", s->export_filename);
        return NGL_ERROR_IO;
    }

    s->csv_line = ngli_bstr_create();
    if (!s->csv_line)
        return NGL_ERROR_MEMORY;

    ngli_bstr_print(s->csv_line, "time,");

    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        ngli_bstr_print(s->csv_line, i ? "," : "");
        widget_specs[widget->type].csv_header(s, widget, s->csv_line);
    }

    ngli_bstr_print(s->csv_line, "\n");

    const size_t len = ngli_bstr_len(s->csv_line);
    size_t n = fwrite(ngli_bstr_strptr(s->csv_line), 1, len, s->fp_export);
    if (n != len) {
        LOG(ERROR, "unable to write CSV header");
        return NGL_ERROR_IO;
    }

    return 0;
}

static void widgets_csv_report(struct hud *s)
{
    const struct ngl_ctx *ctx = s->ctx;
    const struct ngl_node *scene = ctx->scene->params.root;

    /*
     * Set C locale temporarily so floats are printed deterministically. We
     * don't know how the API user is handling the locale, so we do it at the
     * beginning of the inner draw function and restore it at its end.
     */
#if HAVE_USELOCALE
    const locale_t prev_locale = uselocale(s->c_locale);
#elif defined(_WIN32)
    const char *prev_locale = setlocale(LC_ALL, NULL);
    if (!setlocale(LC_ALL, "C"))
        LOG(ERROR, "unable to set C locale");
#endif

    ngli_bstr_clear(s->csv_line);
    ngli_bstr_printf(s->csv_line, "%f", scene ? scene->last_update_time : 0);

    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (size_t i = 0; i < ngli_darray_count(widgets_array); i++) {
        ngli_bstr_print(s->csv_line, ",");
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].csv_report(s, widget, s->csv_line);
    }
    ngli_bstr_print(s->csv_line, "\n");

    const size_t len = ngli_bstr_len(s->csv_line);
    fwrite(ngli_bstr_strptr(s->csv_line), 1, len, s->fp_export);

#if HAVE_USELOCALE
    uselocale(prev_locale);
#elif defined(_WIN32)
    setlocale(LC_ALL, prev_locale);
#endif
}

static void widgets_uninit(struct hud *s)
{
    ngli_darray_reset(&s->widgets);
}

static const char * const vertex_data =
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    ngl_out_pos = projection_matrix"                                   "\n"
    "                * modelview_matrix"                                    "\n"
    "                * vec4(coords.xy, 0.0, 1.0);"                          "\n"
    "    tex_coord = coords.zw;"                                            "\n"
    "}";

static const char * const fragment_data =
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    ngl_out_color = texture(tex, tex_coord);"                          "\n"
    "}";

static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
    {.name = "tex_coord", .type = NGPU_TYPE_VEC2},
};

struct hud *ngli_hud_create(struct ngl_ctx *ctx)
{
    struct hud *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_hud_init(struct hud *s)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;

    s->scale = config->hud_scale;
    s->measure_window = config->hud_measure_window;
    s->refresh_rate[0] = config->hud_refresh_rate[0];
    s->refresh_rate[1] = config->hud_refresh_rate[1];
    s->export_filename = config->hud_export_filename;
    s->scale = config->hud_scale;

    if (!s->measure_window)
        s->measure_window = 60;

    if (s->refresh_rate[1])
        s->refresh_rate_interval = s->refresh_rate[0] / (double)s->refresh_rate[1];
    s->last_refresh_time = -1;

    int ret = widgets_init(s);
    if (ret < 0)
        return ret;

    if (s->export_filename) {
#if HAVE_USELOCALE
        s->c_locale = newlocale(LC_CTYPE_MASK, "C", (locale_t)0);
        if (!s->c_locale) {
            LOG(ERROR, "unable to create C locale");
            return NGL_ERROR_EXTERNAL;
        }
#elif defined(_WIN32)
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
#else
        LOG(WARNING, "no locale support found, assuming C is currently in use");
#endif

        return widgets_csv_header(s);
    }

    s->canvas.buf = ngli_calloc((size_t)s->canvas.w * (size_t)s->canvas.h, 4);
    if (!s->canvas.buf)
        return NGL_ERROR_MEMORY;

    static const float bg_color[] = {0.0f, 0.0f, 0.0f, 0.8f};
    s->bg_color_u32 = NGLI_COLOR_VEC4_TO_U32(bg_color);
    widgets_clear(s);

    static const float coords[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };

    s->coords = ngpu_buffer_create(gpu_ctx);
    if (!s->coords)
        return NGL_ERROR_MEMORY;


    ret = ngpu_buffer_init(s->coords, sizeof(coords), NGPU_BUFFER_USAGE_DYNAMIC_BIT |
                                                          NGPU_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                          NGPU_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (ret < 0)
        return ret;

    ret = ngpu_buffer_upload(s->coords, coords, 0, sizeof(coords));
    if (ret < 0)
        return ret;

    struct ngpu_texture_params tex_params = {
        .type          = NGPU_TEXTURE_TYPE_2D,
        .format        = NGPU_FORMAT_R8G8B8A8_UNORM,
        .width         = (uint32_t)s->canvas.w,
        .height        = (uint32_t)s->canvas.h,
        .min_filter    = NGPU_FILTER_NEAREST,
        .mag_filter    = NGPU_FILTER_NEAREST,
        .usage         = NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT | NGPU_TEXTURE_USAGE_SAMPLED_BIT,
    };
    s->texture = ngpu_texture_create(gpu_ctx);
    if (!s->texture)
        return NGL_ERROR_MEMORY;
    ret = ngpu_texture_init(s->texture, &tex_params);
    if (ret < 0)
        return ret;

    const struct ngpu_block_entry block_fields[] = {
        NGPU_BLOCK_FIELD(struct transforms_block, modelview_matrix, NGPU_TYPE_MAT4, 0),
        NGPU_BLOCK_FIELD(struct transforms_block, projection_matrix, NGPU_TYPE_MAT4, 0),
    };
    const struct ngpu_block_params block_params = {
        .count      = 1,
        .entries    = block_fields,
        .nb_entries = NGLI_ARRAY_NB(block_fields),
    };
    ret = ngpu_block_init(gpu_ctx, &s->transforms_block, &block_params);
    if (ret < 0)
        return ret;
    ngpu_block_update(&s->transforms_block, 0, &(struct transforms_block){
        .modelview_matrix = NGLI_MAT4_IDENTITY,
        .projection_matrix = NGLI_MAT4_IDENTITY,
    });

    const struct ngpu_pgcraft_block blocks[] = {
        {
            .name          = "transforms",
            .instance_name = "",
            .type          = NGPU_TYPE_UNIFORM_BUFFER,
            .stage         = NGPU_PROGRAM_STAGE_VERT,
            .block         = &s->transforms_block.block_desc,
            .buffer = {
                .buffer = s->transforms_block.buffer,
                .size   = s->transforms_block.buffer->size,
            }
        },
    };

    struct ngpu_pgcraft_texture textures[] = {
        {
            .name     = "tex",
            .type     = NGPU_PGCRAFT_TEXTURE_TYPE_2D,
            .stage    = NGPU_PROGRAM_STAGE_FRAG,
            .texture  = s->texture,
        },
    };

    const struct ngpu_pgcraft_attribute attributes[] = {
        {
            .name     = "coords",
            .type     = NGPU_TYPE_VEC4,
            .format   = NGPU_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * sizeof(float),
            .buffer   = s->coords,
        },
    };

    struct rnode *rnode = ctx->rnode_pos;
    struct ngpu_graphics_state graphics_state = rnode->graphics_state;
    graphics_state.blend = 1;
    graphics_state.blend_src_factor = NGPU_BLEND_FACTOR_SRC_ALPHA;
    graphics_state.blend_dst_factor = NGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    graphics_state.blend_src_factor_a = NGPU_BLEND_FACTOR_ZERO;
    graphics_state.blend_dst_factor_a = NGPU_BLEND_FACTOR_ONE;

    const struct ngpu_pgcraft_params crafter_params = {
        .program_label    = "nopegl/hud",
        .vert_base        = vertex_data,
        .frag_base        = fragment_data,
        .blocks           = blocks,
        .nb_blocks        = NGLI_ARRAY_NB(blocks),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    s->crafter = ngpu_pgcraft_create(gpu_ctx);
    if (!s->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngpu_pgcraft_craft(s->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    s->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type         = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state    = graphics_state,
            .rt_layout    = rnode->rendertarget_layout,
            .vertex_state = ngpu_pgcraft_get_vertex_state(s->crafter),
        },
        .program          = ngpu_pgcraft_get_program(s->crafter),
        .layout_desc      = ngpu_pgcraft_get_bindgroup_layout_desc(s->crafter),
        .resources        = ngpu_pgcraft_get_bindgroup_resources(s->crafter),
        .vertex_resources = ngpu_pgcraft_get_vertex_resources(s->crafter),
        .compat_info      = ngpu_pgcraft_get_compat_info(s->crafter),
    };

    ret = ngli_pipeline_compat_init(s->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    return 0;
}

void ngli_hud_draw(struct hud *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;

    widgets_make_stats(s);
    if (s->export_filename) {
        widgets_csv_report(s);
        return;
    }

    const double t = (double)ngli_gettime_relative() / 1000000.;
    const int need_refresh = fabs(t - s->last_refresh_time) >= s->refresh_rate_interval;
    if (need_refresh) {
        s->last_refresh_time = t;
        widgets_clear(s);
        widgets_draw(s);
    }

    const int scale = s->scale > 0 ? s->scale : 1;
    const float ratio_w = (float)(scale * s->canvas.w) / (float)ctx->viewport.width;
    const float ratio_h = (float)(scale * s->canvas.h) / (float)ctx->viewport.height;
    const float x =-1.0f + 2 * ratio_w;
    const float y = 1.0f - 2 * ratio_h;
    const float coords[] = {
        -1.0f,  y,    0.0f, 1.0f,
         x,     y,    1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         x,     1.0f, 1.0f, 0.0f,
    };

    int ret = ngpu_buffer_upload(s->coords, coords, 0, sizeof(coords));
    if (ret < 0)
        return;

    ret = ngpu_texture_upload(s->texture, s->canvas.buf, 0);
    if (ret < 0)
        return;

    if (!ngpu_ctx_is_render_pass_active(gpu_ctx)) {
        ngpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    }

    ngpu_ctx_set_viewport(gpu_ctx, &ctx->viewport);
    ngpu_ctx_set_scissor(gpu_ctx, &ctx->scissor);

    struct transforms_block transforms_block = {0};
    const float *modelview_matrix = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);
    memcpy(transforms_block.modelview_matrix, modelview_matrix, sizeof(transforms_block.modelview_matrix));
    memcpy(transforms_block.projection_matrix, projection_matrix, sizeof(transforms_block.projection_matrix));
    ngpu_block_update(&s->transforms_block, 0, &transforms_block);

    ngli_pipeline_compat_draw(s->pipeline_compat, 4, 1, 0);
}

void ngli_hud_freep(struct hud **sp)
{
    struct hud *s = *sp;
    if (!s)
        return;

    ngli_pipeline_compat_freep(&s->pipeline_compat);
    ngpu_pgcraft_freep(&s->crafter);
    ngpu_texture_freep(&s->texture);
    ngpu_buffer_freep(&s->coords);
    ngpu_block_reset(&s->transforms_block);

    widgets_uninit(s);
    ngli_free(s->canvas.buf);
    if (s->fp_export) {
        fclose(s->fp_export);
        ngli_bstr_freep(&s->csv_line);
    }

    ngli_freep(sp);
}
