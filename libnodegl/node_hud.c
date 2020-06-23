/*
 * Copyright 2016-2018 GoPro Inc.
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

#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "hmap.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "log.h"
#include "drawutils.h"
#include "pgcache.h"
#include "pgcraft.h"
#include "pipeline.h"
#include "type.h"
#include "topology.h"
#include "gtimer.h"
#include "graphicstate.h"

struct hud_priv {
    struct ngl_node *child;
    int measure_window;
    int refresh_rate[2];
    char *export_filename;
    float bg_color[4];
    int aspect_ratio[2];

    struct darray widgets;
    uint32_t bg_color_u32;
    int fd_export;
    struct bstr *csv_line;
    struct canvas canvas;
    double refresh_rate_interval;
    double last_refresh_time;
    int need_refresh;

    struct pgcraft *crafter;
    struct texture *texture;
    struct buffer *coords;
    struct pipeline pipeline;
    struct graphicstate graphicstate;

    int modelview_matrix_index;
    int projection_matrix_index;
};

#define OFFSET(x) offsetof(struct hud_priv, x)
static const struct node_param hud_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR,
              .desc=NGLI_DOCSTRING("scene to benchmark")},
    {"measure_window", PARAM_TYPE_INT, OFFSET(measure_window), {.i64=60},
                       .desc=NGLI_DOCSTRING("window size for latency measures")},
    {"refresh_rate",   PARAM_TYPE_RATIONAL, OFFSET(refresh_rate),
                       .desc=NGLI_DOCSTRING("refresh data buffer every `update_rate` second")},
    {"export_filename", PARAM_TYPE_STR, OFFSET(export_filename),
                        .desc=NGLI_DOCSTRING("path to export file (CSV), disable display if enabled")},
    {"bg_color", PARAM_TYPE_VEC4, OFFSET(bg_color), {.vec={0.0, 0.0, 0.0, 1.0}},
                 .desc=NGLI_DOCSTRING("background buffer color")},
    {"aspect_ratio", PARAM_TYPE_RATIONAL, OFFSET(aspect_ratio),
                     .desc=NGLI_DOCSTRING("buffer aspect ratio")},
    {NULL}
};

#define WIDGET_PADDING 4
#define WIDGET_MARGIN  2

#define LATENCY_WIDGET_TEXT_LEN     20
#define MEMORY_WIDGET_TEXT_LEN      25
#define ACTIVITY_WIDGET_TEXT_LEN    12
#define DRAWCALL_WIDGET_TEXT_LEN    12

enum {
    LATENCY_UPDATE_CPU,
    LATENCY_UPDATE_GPU,
    LATENCY_DRAW_CPU,
    LATENCY_DRAW_GPU,
    LATENCY_TOTAL_CPU,
    LATENCY_TOTAL_GPU,
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
    DRAWCALL_RENDERS,
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
    [LATENCY_UPDATE_GPU] = {"update GPU", 0x3D3DF4FF, 'n'},
    [LATENCY_DRAW_CPU]   = {"draw   CPU", 0x3DF4F4FF, 'u'},
    [LATENCY_DRAW_GPU]   = {"draw   GPU", 0x3DF43DFF, 'n'},
    [LATENCY_TOTAL_CPU]  = {"total  CPU", 0xF4F43DFF, 'u'},
    [LATENCY_TOTAL_GPU]  = {"total  GPU", 0xF43D3DFF, 'n'},
};

static const struct {
    const char *label;
    const int *node_types;
    uint32_t color;
} memory_specs[] = {
    [MEMORY_BUFFERS_CPU] = {
        .label="Buffers CPU",
        .node_types=(const int[]){BUFFER_NODES, -1},
        .color=0xD632FFFF,
    },
    [MEMORY_BUFFERS_GPU] = {
        .label="Buffers GPU",
        .node_types=(const int[]){BUFFER_NODES, -1},
        .color=0x3284FFFF,
    },
    [MEMORY_BLOCKS_CPU] = {
        .label="Blocks CPU",
        .node_types=(const int[]){NGL_NODE_BLOCK, -1},
        .color=0x32FF84FF,
    },
    [MEMORY_BLOCKS_GPU] = {
        .label="Blocks GPU",
        .node_types=(const int[]){NGL_NODE_BLOCK, -1},
        .color=0xD6FF32FF,
    },
    [MEMORY_TEXTURES] = {
        .label="Textures",
        .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURE3D, -1},
        .color=0xFF3232FF,
    },
};

static const struct activity_spec {
    const char *label;
    const int *node_types;
} activity_specs[] = {
    [ACTIVITY_BUFFERS] = {
        .label="Buffers",
        .node_types=(const int[]){BUFFER_NODES, -1},
    },
    [ACTIVITY_BLOCKS] = {
        .label="Blocks",
        .node_types=(const int[]){NGL_NODE_BLOCK, -1},
    },
    [ACTIVITY_MEDIAS] = {
        .label="Medias",
        .node_types=(const int[]){NGL_NODE_MEDIA, -1},
    },
    [ACTIVITY_TEXTURES] = {
        .label="Textures",
        .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURE3D, -1},
    },
};

static const struct drawcall_spec {
    const char *label;
    const int *node_types;
} drawcall_specs[] = {
    [DRAWCALL_COMPUTES] = {
        .label="Computes",
        .node_types=(const int[]){NGL_NODE_COMPUTE, -1},
    },
    [DRAWCALL_GRAPHICCONFIGS] = {
        .label="GraphicCfgs",
        .node_types=(const int[]){NGL_NODE_GRAPHICCONFIG, -1},
    },
    [DRAWCALL_RENDERS] = {
        .label="Renders",
        .node_types=(const int[]){NGL_NODE_RENDER, -1},
    },
    [DRAWCALL_RTTS] = {
        .label="RTTs",
        .node_types=(const int[]){NGL_NODE_RENDERTOTEXTURE, -1},
    },
};

NGLI_STATIC_ASSERT(hud_nb_latency,  NGLI_ARRAY_NB(latency_specs)  == NB_LATENCY);
NGLI_STATIC_ASSERT(hud_nb_memory,   NGLI_ARRAY_NB(memory_specs)   == NB_MEMORY);
NGLI_STATIC_ASSERT(hud_nb_activity, NGLI_ARRAY_NB(activity_specs) == NB_ACTIVITY);
NGLI_STATIC_ASSERT(hud_nb_drawcall, NGLI_ARRAY_NB(drawcall_specs) == NB_DRAWCALL);

enum widget_type {
    WIDGET_LATENCY,
    WIDGET_MEMORY,
    WIDGET_ACTIVITY,
    WIDGET_DRAWCALL,
};

struct data_graph {
    int64_t *values;
    int nb_values;
    int count;
    int pos;
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
    struct gtimer timer;
};

struct widget_memory {
    struct darray nodes[NB_MEMORY];
    uint64_t sizes[NB_MEMORY];
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
    int nb_data_graph;
    size_t priv_size;
    int (*init)(struct ngl_node *node, struct widget *widget);
    void (*make_stats)(struct ngl_node *node, struct widget *widget);
    void (*draw)(struct ngl_node *node, struct widget *widget);
    void (*csv_header)(struct ngl_node *node, struct widget *widget, struct bstr *dst);
    void (*csv_report)(struct ngl_node *node, struct widget *widget, struct bstr *dst);
    void (*uninit)(struct ngl_node *node, struct widget *widget);
};

/* Widget init */

static int widget_latency_init(struct ngl_node *node, struct widget *widget)
{
    struct ngl_ctx *ctx = node->ctx;
    struct hud_priv *s = node->priv_data;
    struct widget_latency *priv = widget->priv_data;

    int ret = ngli_gtimer_init(&priv->timer, ctx);
    if (ret < 0)
        return ret;

    ngli_assert(NB_LATENCY == NGLI_ARRAY_NB(priv->measures));

    s->measure_window = NGLI_MAX(s->measure_window, 1);
    for (int i = 0; i < NB_LATENCY; i++) {
        int64_t *times = ngli_calloc(s->measure_window, sizeof(*times));
        if (!times)
            return NGL_ERROR_MEMORY;
        priv->measures[i].times = times;
    }

    return 0;
}

static int track_children_per_types(struct hmap *map, struct ngl_node *node, int node_type)
{
    if (node->class->id == node_type) {
        char key[32];
        int ret = snprintf(key, sizeof(key), "%p", node);
        if (ret < 0)
            return ret;
        ret = ngli_hmap_set(map, key, node);
        if (ret < 0)
            return ret;
    }

    struct darray *children_array = &node->children;
    struct ngl_node **children = ngli_darray_data(children_array);
    for (int i = 0; i < ngli_darray_count(children_array); i++) {
        int ret = track_children_per_types(map, children[i], node_type);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int make_nodes_set(struct ngl_node *scene, struct darray *nodes_list, const int *node_types)
{
    /* construct a set of the nodes of a given type(s) */
    struct hmap *nodes_set = ngli_hmap_create();
    if (!nodes_set)
        return NGL_ERROR_MEMORY;
    for (int n = 0; node_types[n] != -1; n++) {
        int ret = track_children_per_types(nodes_set, scene, node_types[n]);
        if (ret < 0) {
            ngli_hmap_freep(&nodes_set);
            return ret;
        }
    }

    /* transfer the set content to a list of elements */
    ngli_darray_init(nodes_list, sizeof(struct ngl_node *), 0);
    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(nodes_set, entry))) {
        if (!ngli_darray_push(nodes_list, &entry->data)) {
            ngli_hmap_freep(&nodes_set);
            return NGL_ERROR_MEMORY;
        }
    }

    ngli_hmap_freep(&nodes_set);
    return 0;
}

static int widget_memory_init(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    struct widget_memory *priv = widget->priv_data;

    for (int i = 0; i < NB_MEMORY; i++) {
        const int *node_types = memory_specs[i].node_types;
        int ret = make_nodes_set(s->child, &priv->nodes[i], node_types);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int widget_activity_init(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    const struct activity_spec *spec = widget->user_data;
    struct widget_activity *priv = widget->priv_data;
    const int *node_types = spec->node_types;
    return make_nodes_set(s->child, &priv->nodes, node_types);
}

static int widget_drawcall_init(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    const struct drawcall_spec *spec = widget->user_data;
    struct widget_drawcall *priv = widget->priv_data;
    const int *node_types = spec->node_types;
    return make_nodes_set(s->child, &priv->nodes, node_types);
}

/* Widget update */

static void register_time(struct hud_priv *s, struct latency_measure *m, int64_t t)
{
    m->total_times = m->total_times - m->times[m->pos] + t;
    m->times[m->pos] = t;
    m->pos = (m->pos + 1) % s->measure_window;
    m->count = NGLI_MIN(m->count + 1, s->measure_window);
}

static int widget_latency_update(struct ngl_node *node, struct widget *widget, double t)
{
    int ret;
    struct hud_priv *s = node->priv_data;
    struct ngl_node *child = s->child;
    struct widget_latency *priv = widget->priv_data;

    ngli_gtimer_start(&priv->timer);
    int64_t update_start = ngli_gettime_relative();
    ret = ngli_node_update(child, t);
    int64_t update_end = ngli_gettime_relative();
    ngli_gtimer_stop(&priv->timer);

    const int64_t gpu_tupdate = ngli_gtimer_read(&priv->timer);
    register_time(s, &priv->measures[LATENCY_UPDATE_CPU], update_end - update_start);
    register_time(s, &priv->measures[LATENCY_UPDATE_GPU], gpu_tupdate);

    return ret;
}

/* Widget make stats */

static void widget_latency_make_stats(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    struct widget_latency *priv = widget->priv_data;

    ngli_gtimer_start(&priv->timer);
    const int64_t draw_start = ngli_gettime_relative();
    ngli_node_draw(s->child);
    const int64_t draw_end = ngli_gettime_relative();
    ngli_gtimer_stop(&priv->timer);

    int64_t cpu_tdraw = draw_end - draw_start;
    int64_t gpu_tdraw = ngli_gtimer_read(&priv->timer);
    register_time(s, &priv->measures[LATENCY_DRAW_CPU], cpu_tdraw);
    register_time(s, &priv->measures[LATENCY_DRAW_GPU], gpu_tdraw);

    const struct latency_measure *cpu_up = &priv->measures[LATENCY_UPDATE_CPU];
    const struct latency_measure *gpu_up = &priv->measures[LATENCY_UPDATE_GPU];
    const int last_cpu_up_pos = (cpu_up->pos ? cpu_up->pos : s->measure_window) - 1;
    const int last_gpu_up_pos = (gpu_up->pos ? gpu_up->pos : s->measure_window) - 1;
    const int64_t cpu_tupdate = cpu_up->times[last_cpu_up_pos];
    const int64_t gpu_tupdate = gpu_up->times[last_gpu_up_pos];
    register_time(s, &priv->measures[LATENCY_TOTAL_CPU], cpu_tdraw + cpu_tupdate);
    register_time(s, &priv->measures[LATENCY_TOTAL_GPU], gpu_tdraw + gpu_tupdate);
}

static void widget_memory_make_stats(struct ngl_node *node, struct widget *widget)
{
    struct widget_memory *priv = widget->priv_data;

    struct darray *nodes_buf_array_cpu = &priv->nodes[MEMORY_BUFFERS_CPU];
    struct ngl_node **nodes_buf_cpu = ngli_darray_data(nodes_buf_array_cpu);
    priv->sizes[MEMORY_BUFFERS_CPU] = 0;
    for (int i = 0; i < ngli_darray_count(nodes_buf_array_cpu); i++) {
        const struct ngl_node *buf_node = nodes_buf_cpu[i];
        const struct buffer_priv *buffer = buf_node->priv_data;
        priv->sizes[MEMORY_BUFFERS_CPU] += buffer->block ? 0 : buffer->data_size;
    }

    struct darray *nodes_buf_array_gpu = &priv->nodes[MEMORY_BUFFERS_GPU];
    struct ngl_node **nodes_buf_gpu = ngli_darray_data(nodes_buf_array_gpu);
    priv->sizes[MEMORY_BUFFERS_GPU] = 0;
    for (int i = 0; i < ngli_darray_count(nodes_buf_array_gpu); i++) {
        const struct ngl_node *buf_node = nodes_buf_gpu[i];
        const struct buffer_priv *buffer = buf_node->priv_data;
        priv->sizes[MEMORY_BUFFERS_GPU] += buffer->block ? 0 : buffer->data_size * (buffer->buffer_refcount > 0);
    }

    struct darray *nodes_blk_array_cpu = &priv->nodes[MEMORY_BLOCKS_CPU];
    struct ngl_node **nodes_blk_cpu = ngli_darray_data(nodes_blk_array_cpu);
    priv->sizes[MEMORY_BLOCKS_CPU] = 0;
    for (int i = 0; i < ngli_darray_count(nodes_blk_array_cpu); i++) {
        const struct ngl_node *blk_node = nodes_blk_cpu[i];
        const struct block_priv *block = blk_node->priv_data;
        priv->sizes[MEMORY_BLOCKS_CPU] += block->data_size;
    }

    struct darray *nodes_blk_array_gpu = &priv->nodes[MEMORY_BLOCKS_GPU];
    struct ngl_node **nodes_blk_gpu = ngli_darray_data(nodes_blk_array_gpu);
    priv->sizes[MEMORY_BLOCKS_GPU] = 0;
    for (int i = 0; i < ngli_darray_count(nodes_blk_array_gpu); i++) {
        const struct ngl_node *blk_node = nodes_blk_gpu[i];
        const struct block_priv *block = blk_node->priv_data;
        priv->sizes[MEMORY_BLOCKS_GPU] += block->data_size * (block->buffer_refcount > 0);
    }

    struct darray *nodes_tex_array = &priv->nodes[MEMORY_TEXTURES];
    struct ngl_node **nodes_tex = ngli_darray_data(nodes_tex_array);
    priv->sizes[MEMORY_TEXTURES] = 0;
    for (int i = 0; i < ngli_darray_count(nodes_tex_array); i++) {
        const struct ngl_node *tex_node = nodes_tex[i];
        const struct texture_priv *texture = tex_node->priv_data;
        priv->sizes[MEMORY_TEXTURES] += ngli_image_get_memory_size(&texture->image)
                                      * tex_node->is_active;
    }
}

static void widget_activity_make_stats(struct ngl_node *node, struct widget *widget)
{
    struct widget_activity *priv = widget->priv_data;
    struct darray *nodes_array = &priv->nodes;
    struct ngl_node **nodes = ngli_darray_data(nodes_array);
    priv->nb_actives = 0;
    for (int i = 0; i < ngli_darray_count(nodes_array); i++)
        priv->nb_actives += nodes[i]->is_active;
}

static void widget_drawcall_make_stats(struct ngl_node *node, struct widget *widget)
{
    struct widget_drawcall *priv = widget->priv_data;
    struct darray *nodes_array = &priv->nodes;
    struct ngl_node **nodes = ngli_darray_data(nodes_array);
    priv->nb_draws = 0;
    for (int i = 0; i < ngli_darray_count(nodes_array); i++)
        priv->nb_draws += nodes[i]->draw_count;
}

/* Draw utils */

static inline uint8_t *set_color(uint8_t *p, uint32_t rgba)
{
    p[0] = rgba >> 24;
    p[1] = rgba >> 16 & 0xff;
    p[2] = rgba >>  8 & 0xff;
    p[3] = rgba       & 0xff;
    return p + 4;
}

static inline int get_pixel_pos(struct hud_priv *s, int px, int py)
{
    return (py * s->canvas.w + px) * 4;
}

static int clip(int x, int min, int max)
{
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}

static void draw_block_graph(struct hud_priv *s,
                             const struct data_graph *d,
                             const struct rect *rect,
                             int64_t graph_min, int64_t graph_max,
                             const uint32_t c)
{
    const int64_t graph_h = graph_max - graph_min;
    const float vscale = (float)rect->h / graph_h;
    const int start = (d->pos - d->count + d->nb_values) % d->nb_values;

    for (int k = 0; k < d->count; k++) {
        const int64_t v = d->values[(start + k) % d->nb_values];
        const int h = (v - graph_min) * vscale;
        const int y = clip(rect->h - h, 0, rect->h);
        uint8_t *p = s->canvas.buf + get_pixel_pos(s, rect->x + k, rect->y + y);

        for (int z = 0; z < h; z++) {
            set_color(p, c);
            p += s->canvas.w * 4;
        }
    }
}

static void draw_line_graph(struct hud_priv *s,
                            const struct data_graph *d,
                            const struct rect *rect,
                            int64_t graph_min, int64_t graph_max,
                            const uint32_t c)
{
    const int64_t graph_h = graph_max - graph_min;
    const float vscale = (float)rect->h / graph_h;
    const int start = (d->pos - d->count + d->nb_values) % d->nb_values;
    int prev_y;

    for (int k = 0; k < d->count; k++) {
        const int64_t v = d->values[(start + k) % d->nb_values];
        const int h = (v - graph_min) * vscale;
        const int y = clip(rect->h - 1 - h, 0, rect->h - 1);
        uint8_t *p = s->canvas.buf + get_pixel_pos(s, rect->x + k, rect->y + y);

        set_color(p, c);
        if (k) {
            const int sign = prev_y < y ? 1 : -1;
            const int column_h = abs(prev_y - y);
            uint8_t *p = s->canvas.buf + get_pixel_pos(s, rect->x + k, rect->y + prev_y);
            for (int z = 0; z < column_h; z++) {
                set_color(p, c);
                p += sign * s->canvas.w * 4;
            }
        }
        prev_y = y;
    }
}

static void print_text(struct hud_priv *s, int x, int y, const char *buf, const uint32_t c)
{
    ngli_drawutils_print(&s->canvas, x, y, buf, c);
}

static void widgets_clear(struct hud_priv *s)
{
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++)
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

static int64_t get_latency_avg(const struct widget_latency *priv, int id)
{
    const struct latency_measure *m = &priv->measures[id];
    return m->total_times / m->count / (latency_specs[id].unit == 'u' ? 1 : 1000);
}

static void widget_latency_draw(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    struct widget_latency *priv = widget->priv_data;

    char buf[LATENCY_WIDGET_TEXT_LEN + 1];
    for (int i = 0; i < NB_LATENCY; i++) {
        const int64_t t = get_latency_avg(priv, i);

        snprintf(buf, sizeof(buf), "%s %5" PRId64 "usec", latency_specs[i].label, t);
        print_text(s, widget->text_x, widget->text_y + i * NGLI_FONT_H, buf, latency_specs[i].color);
        register_graph_value(&widget->data_graph[i], t);
    }

    int64_t graph_min = widget->data_graph[0].min;
    int64_t graph_max = widget->data_graph[0].max;
    for (int i = 1; i < NB_LATENCY; i++) {
        graph_min = NGLI_MIN(graph_min, widget->data_graph[i].min);
        graph_max = NGLI_MAX(graph_max, widget->data_graph[i].max);
    }

    const int64_t graph_h = graph_max - graph_min;
    if (graph_h) {
        for (int i = 0; i < NB_LATENCY; i++)
            draw_line_graph(s, &widget->data_graph[i], &widget->graph_rect,
                            graph_min, graph_max, latency_specs[i].color);
    }
}

static void widget_memory_draw(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    struct widget_memory *priv = widget->priv_data;
    char buf[MEMORY_WIDGET_TEXT_LEN + 1];

    for (int i = 0; i < NB_MEMORY; i++) {
        const uint64_t size = priv->sizes[i];
        const uint32_t color = memory_specs[i].color;
        const char *label = memory_specs[i].label;

        if (size < 1024)
            snprintf(buf, sizeof(buf), "%-12s %"PRIu64, label, size);
        else if (size < 1024 * 1024)
            snprintf(buf, sizeof(buf), "%-12s %"PRIu64"K", label, size / 1024);
        else if (size < 1024 * 1024 * 1024)
            snprintf(buf, sizeof(buf), "%-12s %"PRIu64"M", label, size / (1024 * 1024));
        else
            snprintf(buf, sizeof(buf), "%-12s %"PRIu64"G", label, size / (1024 * 1024 * 1024));
        print_text(s, widget->text_x, widget->text_y + i * NGLI_FONT_H, buf, color);
        register_graph_value(&widget->data_graph[i], size);
    }

    int64_t graph_min = widget->data_graph[0].min;
    int64_t graph_max = widget->data_graph[0].max;
    for (int i = 1; i < NB_MEMORY; i++) {
        graph_min = NGLI_MIN(graph_min, widget->data_graph[i].min);
        graph_max = NGLI_MAX(graph_max, widget->data_graph[i].max);
    }

    const int64_t graph_h = graph_max - graph_min;
    if (graph_h) {
        for (int i = 0; i < NB_MEMORY; i++)
            draw_line_graph(s, &widget->data_graph[i], &widget->graph_rect,
                            graph_min, graph_max, memory_specs[i].color);
    }
}

static void widget_activity_draw(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
    struct widget_activity *priv = widget->priv_data;
    const struct activity_spec *spec = widget->user_data;
    const uint32_t color = 0x3df4f4ff;

    char buf[ACTIVITY_WIDGET_TEXT_LEN + 1];
    snprintf(buf, sizeof(buf), "%d/%d", priv->nb_actives, priv->nodes.count);
    print_text(s, widget->text_x, widget->text_y, spec->label, color);
    print_text(s, widget->text_x, widget->text_y + NGLI_FONT_H, buf, color);

    struct data_graph *d = &widget->data_graph[0];
    register_graph_value(d, priv->nb_actives);
    draw_block_graph(s, d, &widget->graph_rect, d->amin, d->amax, color);
}

static void widget_drawcall_draw(struct ngl_node *node, struct widget *widget)
{
    struct hud_priv *s = node->priv_data;
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

static void widget_latency_csv_header(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    for (int i = 0; i < NB_LATENCY; i++)
        ngli_bstr_printf(dst, "%s%s", i ? "," : "", latency_specs[i].label);
}

static void widget_memory_csv_header(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    for (int i = 0; i < NB_MEMORY; i++)
        ngli_bstr_printf(dst, "%s%s memory", i ? "," : "", memory_specs[i].label);
}

static void widget_activity_csv_header(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    const struct activity_spec *spec = widget->user_data;
    ngli_bstr_printf(dst, "%s count,%s total", spec->label, spec->label);
}

static void widget_drawcall_csv_header(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    const struct drawcall_spec *spec = widget->user_data;
    ngli_bstr_print(dst, spec->label);
}

/* Widget CSV report */

static void widget_latency_csv_report(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    const struct widget_latency *priv = widget->priv_data;
    for (int i = 0; i < NB_LATENCY; i++) {
        const int64_t t = get_latency_avg(priv, i);
        ngli_bstr_printf(dst, "%s%"PRId64, i ? "," : "", t);
    }
}

static void widget_memory_csv_report(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    const struct widget_memory *priv = widget->priv_data;
    for (int i = 0; i < NB_MEMORY; i++) {
        const uint64_t size = priv->sizes[i];
        ngli_bstr_printf(dst, "%s%"PRIu64, i ? "," : "", size);
    }
}

static void widget_activity_csv_report(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    const struct widget_activity *priv = widget->priv_data;
    ngli_bstr_printf(dst, "%d,%d", priv->nb_actives, priv->nodes.count);
}

static void widget_drawcall_csv_report(struct ngl_node *node, struct widget *widget, struct bstr *dst)
{
    const struct widget_drawcall *priv = widget->priv_data;
    ngli_bstr_printf(dst, "%d", priv->nb_draws);
}

/* Widget uninit */

static void widget_latency_uninit(struct ngl_node *node, struct widget *widget)
{
    struct widget_latency *priv = widget->priv_data;
    for (int i = 0; i < NB_LATENCY; i++)
        ngli_free(priv->measures[i].times);
    ngli_gtimer_reset(&priv->timer);
}

static void widget_memory_uninit(struct ngl_node *node, struct widget *widget)
{
    struct widget_memory *priv = widget->priv_data;
    for (int i = 0; i < NB_MEMORY; i++)
        ngli_darray_reset(&priv->nodes[i]);
}

static void widget_activity_uninit(struct ngl_node *node, struct widget *widget)
{
    struct widget_activity *priv = widget->priv_data;
    ngli_darray_reset(&priv->nodes);
}

static void widget_drawcall_uninit(struct ngl_node *node, struct widget *widget)
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
        .graph_h       = 50,
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

static int create_widget(struct hud_priv *s, enum widget_type type, const void *user_data, int x, int y)
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
    for (int i = 0; i < spec->nb_data_graph; i++) {
        struct data_graph *d = &widgetp->data_graph[i];
        d->nb_values = widgetp->graph_rect.w;
        d->values = ngli_calloc(d->nb_values, sizeof(*d->values));
        if (!d->values)
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int widgets_init(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;

    ngli_darray_init(&s->widgets, sizeof(struct widget), 0);

    /* Smallest dimensions possible (in pixels) */
    const int top_width    = WIDGET_MARGIN * 3
                           + get_widget_width(WIDGET_LATENCY)
                           + get_widget_width(WIDGET_MEMORY);
    const int bot_width    = WIDGET_MARGIN * 3
                           + get_widget_width(WIDGET_ACTIVITY) * NB_ACTIVITY + WIDGET_MARGIN * (NB_ACTIVITY - 1)
                           + get_widget_width(WIDGET_DRAWCALL) * NB_DRAWCALL + WIDGET_MARGIN * (NB_DRAWCALL - 1);
    const int left_height  = WIDGET_MARGIN * 3
                           + get_widget_height(WIDGET_LATENCY)
                           + get_widget_height(WIDGET_ACTIVITY);
    const int right_height = WIDGET_MARGIN * 3
                           + get_widget_height(WIDGET_MEMORY)
                           + get_widget_height(WIDGET_DRAWCALL);
    const int min_width    = NGLI_MAX(top_width, bot_width);
    const int min_height   = NGLI_MAX(left_height, right_height);

    /* Compute buffer dimensions according to user specified aspect ratio and
     * minimal dimensions */
    static const int default_ar[] = {1, 1};
    const int *ar = s->aspect_ratio[0] && s->aspect_ratio[1] ? s->aspect_ratio : default_ar;
    s->canvas.w = min_width;
    s->canvas.h = min_width * ar[1] / ar[0];
    if (s->canvas.h < min_height) {
        s->canvas.w = min_height * ar[0] / ar[1];
        s->canvas.h = min_height;
    }

    /* Latency widget in the top-left */
    const int x_latency = WIDGET_MARGIN;
    const int y_latency = WIDGET_MARGIN;
    int ret = create_widget(s, WIDGET_LATENCY, NULL, x_latency, y_latency);
    if (ret < 0)
        return ret;

    /* Memory widget in the top-right */
    const int x_memory = -get_widget_width(WIDGET_MEMORY) - WIDGET_MARGIN;
    const int y_memory = WIDGET_MARGIN;
    ret = create_widget(s, WIDGET_MEMORY, NULL, x_memory, y_memory);
    if (ret < 0)
        return ret;

    /* Activity nodes counter widgets in the bottom-left */
    int x_activity = WIDGET_MARGIN;
    const int y_activity = -get_widget_height(WIDGET_ACTIVITY) - WIDGET_MARGIN;
    const int x_activity_step = get_widget_width(WIDGET_ACTIVITY) + WIDGET_MARGIN;
    for (int i = 0; i < NB_ACTIVITY; i++) {
        ret = create_widget(s, WIDGET_ACTIVITY, &activity_specs[i], x_activity, y_activity);
        if (ret < 0)
            return ret;
        x_activity += x_activity_step;
    }

    /* Draw-calls widgets in the bottom-right */
    int x_drawcall =  -get_widget_width(WIDGET_DRAWCALL) * NB_DRAWCALL - WIDGET_MARGIN * NB_DRAWCALL;
    const int y_drawcall = -get_widget_height(WIDGET_DRAWCALL) - WIDGET_MARGIN;
    const int x_drawcall_step = get_widget_width(WIDGET_DRAWCALL) + WIDGET_MARGIN;
    for (int i = 0; i < NB_DRAWCALL; i++) {
        ret = create_widget(s, WIDGET_DRAWCALL, &drawcall_specs[i], x_drawcall, y_drawcall);
        if (ret < 0)
            return ret;
        x_drawcall += x_drawcall_step;
    }

    /* Call init on every widget */
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        int ret = widget_specs[widget->type].init(node, widget);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void widget_drawcall_reset_draws(struct widget *widget)
{
    struct widget_drawcall *priv = widget->priv_data;
    for (int i = 0; i < NB_DRAWCALL; i++) {
        struct darray *nodes_array = &priv->nodes;
        struct ngl_node **nodes = ngli_darray_data(nodes_array);
        for (int i = 0; i < ngli_darray_count(nodes_array); i++)
            nodes[i]->draw_count = 0;
    }
}

static void widgets_make_stats(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;

    /* HACK: reset drawcall draw counts before calling
     * widget_latency_make_stats(). This is needed here because several draws
     * can happen without update (for instance in case of a resize). */
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *w = &widgets[i];
        if (w->type == WIDGET_DRAWCALL)
            widget_drawcall_reset_draws(w);
    }

    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].make_stats(node, widget);
    }
}

static void widgets_draw(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].draw(node, widget);
    }
}

static int widgets_csv_header(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;

    s->fd_export = open(s->export_filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (s->fd_export == -1) {
        LOG(ERROR, "unable to open \"%s\" for writing", s->export_filename);
        return NGL_ERROR_IO;
    }

    s->csv_line = ngli_bstr_create();
    if (!s->csv_line)
        return NGL_ERROR_MEMORY;

    ngli_bstr_print(s->csv_line, "time,");

    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        ngli_bstr_print(s->csv_line, i ? "," : "");
        widget_specs[widget->type].csv_header(node, widget, s->csv_line);
    }

    ngli_bstr_print(s->csv_line, "\n");

    const int len = ngli_bstr_len(s->csv_line);
    ssize_t n = write(s->fd_export, ngli_bstr_strptr(s->csv_line), len);
    if (n != len) {
        LOG(ERROR, "unable to write CSV header");
        return NGL_ERROR_IO;
    }

    return 0;
}

static void widgets_csv_report(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;

    ngli_bstr_clear(s->csv_line);
    /* Quoting to prevent locale issues with float printing */
    ngli_bstr_printf(s->csv_line, "\"%f\"", s->last_refresh_time);

    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        ngli_bstr_print(s->csv_line, ",");
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].csv_report(node, widget, s->csv_line);
    }
    ngli_bstr_print(s->csv_line, "\n");

    const int len = ngli_bstr_len(s->csv_line);
    write(s->fd_export, ngli_bstr_strptr(s->csv_line), len);
}

static void free_widget(struct widget *widget)
{
    ngli_free(widget->priv_data);
    for (int i = 0; i < widget_specs[widget->type].nb_data_graph; i++)
        ngli_free(widget->data_graph[i].values);
    ngli_free(widget->data_graph);
}

static void widgets_uninit(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;
    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    for (int i = 0; i < ngli_darray_count(widgets_array); i++) {
        struct widget *widget = &widgets[i];
        widget_specs[widget->type].uninit(node, widget);
        free_widget(widget);
    }
    ngli_darray_reset(&s->widgets);
}

static const char * const vertex_data =
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    ngl_out_pos = projection_matrix"                                   "\n"
    "                * modelview_matrix"                                    "\n"
    "                * vec4(coords.xy, 0.0, 1.0);"                          "\n"
    "    var_tex_coord = coords.zw;"                                        "\n"
    "}";

static const char * const fragment_data =
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    ngl_out_color = ngl_tex2d(tex, var_tex_coord);"                    "\n"
    "}";

static const struct pgcraft_iovar vert_out_vars[] = {
    {.name = "var_tex_coord", .type = NGLI_TYPE_VEC2},
};

static int hud_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct hud_priv *s = node->priv_data;

    int ret = widgets_init(node);
    if (ret < 0)
        return ret;

    if (s->refresh_rate[1])
        s->refresh_rate_interval = s->refresh_rate[0] / (double)s->refresh_rate[1];
    s->last_refresh_time = -1;

    if (s->export_filename)
        return widgets_csv_header(node);

    s->canvas.buf = ngli_calloc(s->canvas.w * s->canvas.h, 4);
    if (!s->canvas.buf)
        return NGL_ERROR_MEMORY;

    s->bg_color_u32 = NGLI_COLOR_VEC4_TO_U32(s->bg_color);
    widgets_clear(s);

    static const float coords[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
    };

    s->coords = ngli_buffer_create(ctx);
    if (!s->coords)
        return NGL_ERROR_MEMORY;

    ret = ngli_buffer_init(s->coords, sizeof(coords), NGLI_BUFFER_USAGE_STATIC);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(s->coords, coords, sizeof(coords));
    if (ret < 0)
        return ret;

    struct texture_params tex_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    tex_params.width = s->canvas.w;
    tex_params.height = s->canvas.h;
    tex_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    tex_params.min_filter = NGLI_FILTER_LINEAR;
    tex_params.mag_filter = NGLI_FILTER_NEAREST;
    tex_params.mipmap_filter = NGLI_MIPMAP_FILTER_LINEAR;
    s->texture = ngli_texture_create(ctx);
    if (!s->texture)
        return NGL_ERROR_MEMORY;
    ret = ngli_texture_init(s->texture, &tex_params);
    if (ret < 0)
        return ret;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
    };

    struct pgcraft_texture textures[] = {
        {
            .name     = "tex",
            .type     = NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE2D,
            .stage    = NGLI_PROGRAM_SHADER_FRAG,
            .texture  = s->texture,
        },
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "coords",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * 4,
            .buffer   = s->coords,
        },
    };

    struct graphicstate graphicstate = ctx->graphicstate;
    graphicstate.blend = 1;
    graphicstate.blend_src_factor = NGLI_BLEND_FACTOR_SRC_ALPHA;
    graphicstate.blend_dst_factor = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    graphicstate.blend_src_factor_a = NGLI_BLEND_FACTOR_ZERO;
    graphicstate.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE;

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology    = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
            .nb_vertices = 4,
            .state       = graphicstate,
            .rt_desc     = *ctx->rendertarget_desc,
        }
    };

    const struct pgcraft_params crafter_params = {
        .vert_base        = vertex_data,
        .frag_base        = fragment_data,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    s->crafter = ngli_pgcraft_create(ctx);
    if (!s->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcraft_craft(s->crafter, &pipeline_params, &crafter_params);
    if (ret < 0)
        return ret;

    ret = ngli_pipeline_init(&s->pipeline, ctx, &pipeline_params);
    if (ret < 0)
        return ret;

    s->modelview_matrix_index = ngli_pgcraft_get_uniform_index(s->crafter, "modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    s->projection_matrix_index = ngli_pgcraft_get_uniform_index(s->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);

    return 0;
}

static int hud_update(struct ngl_node *node, double t)
{
    struct hud_priv *s = node->priv_data;

    s->need_refresh = fabs(t - s->last_refresh_time) >= s->refresh_rate_interval;
    if (s->need_refresh)
        s->last_refresh_time = t;

    struct darray *widgets_array = &s->widgets;
    struct widget *widgets = ngli_darray_data(widgets_array);
    return widget_latency_update(node, &widgets[0], t);
}

static void hud_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct hud_priv *s = node->priv_data;

    widgets_make_stats(node);
    if (s->need_refresh) {
        if (s->export_filename) {
            widgets_csv_report(node);
        } else {
            widgets_clear(s);
            widgets_draw(node);
        }
    }

    if (s->export_filename)
        return;

    int ret = ngli_texture_upload(s->texture, s->canvas.buf, 0);
    if (ret < 0)
        return;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);
    ngli_pipeline_update_uniform(&s->pipeline, s->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_update_uniform(&s->pipeline, s->projection_matrix_index, projection_matrix);
    ngli_pipeline_exec(&s->pipeline);
}

static void hud_uninit(struct ngl_node *node)
{
    struct hud_priv *s = node->priv_data;

    ngli_pipeline_reset(&s->pipeline);
    ngli_pgcraft_freep(&s->crafter);
    ngli_texture_freep(&s->texture);
    ngli_buffer_freep(&s->coords);

    widgets_uninit(node);
    ngli_free(s->canvas.buf);
    if (s->export_filename) {
        close(s->fd_export);
        ngli_bstr_freep(&s->csv_line);
    }
}

const struct node_class ngli_hud_class = {
    .id        = NGL_NODE_HUD,
    .name      = "HUD",
    .init      = hud_init,
    .update    = hud_update,
    .draw      = hud_draw,
    .uninit    = hud_uninit,
    .priv_size = sizeof(struct hud_priv),
    .params    = hud_params,
    .file      = __FILE__,
};
