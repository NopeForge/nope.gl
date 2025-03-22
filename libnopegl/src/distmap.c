/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Forge
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "distmap.h"
#include "distmap_frag.h"
#include "distmap_vert.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/buffer.h"
#include "ngpu/ctx.h"
#include "ngpu/format.h"
#include "ngpu/rendertarget.h"
#include "ngpu/texture.h"
#include "ngpu/type.h"
#include "nopegl.h"
#include "path.h"
#include "pipeline_compat.h"
#include "ngpu/pgcraft.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"

/*
 * Padding percent is arbitrary: it represents how far an effect such as glowing
 * could be applied.
 */
#define PCENT_PADDING 80

struct bezier3 {
    float p0, p1, p2, p3;
};

struct shape {
    int32_t width, height;
};

struct distmap {
    struct ngl_ctx *ctx;

    int32_t pad;
    int32_t max_shape_w, max_shape_h;
    int32_t max_shape_padded_w, max_shape_padded_h;
    int32_t texture_w, texture_h;
    int32_t nb_rows, nb_cols;
    float scale;

    struct darray shapes;              // struct shape
    struct darray bezier_x;            // struct bezier3
    struct darray bezier_y;            // struct bezier3
    struct darray bezier_counts;       // int32_t
    struct darray beziergroup_counts;  // int32_t

    struct ngpu_texture *texture;
    struct ngpu_rendertarget *rt;
    struct ngpu_pgcraft *crafter;
    struct ngpu_block_desc vert_block;
    struct ngpu_block_desc frag_block;
    struct ngpu_buffer *vert_buffer;
    size_t vert_offset;
    struct ngpu_buffer *frag_buffer;
    size_t frag_offset;
    struct pipeline_compat *pipeline_compat;
};

struct distmap *ngli_distmap_create(struct ngl_ctx *ctx)
{
    struct distmap *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    ngli_darray_init(&s->shapes, sizeof(struct shape), 0);
    ngli_darray_init(&s->bezier_x, sizeof(struct bezier3), 0);
    ngli_darray_init(&s->bezier_y, sizeof(struct bezier3), 0);
    ngli_darray_init(&s->bezier_counts, sizeof(int32_t), 0);
    ngli_darray_init(&s->beziergroup_counts, sizeof(int32_t), 0);
    return s;
}

int ngli_distmap_init(struct distmap *s)
{
    return 0;
}

static struct bezier3 b3_from_line(float p0, float p1)
{
    struct bezier3 ret = {p0, NGLI_MIX_F32(p0, p1, 1.f/3.f), NGLI_MIX_F32(p0, p1, 2.f/3.f), p1};
    return ret;
}

static struct bezier3 b3_from_bezier2(float p0, float p1, float p2)
{
    struct bezier3 ret = {p0, NGLI_MIX_F32(p0, p1, 2.f/3.f), NGLI_MIX_F32(p1, p2, 1.f/3.f), p2};
    return ret;
}

static struct bezier3 b3_from_bezier3(float p0, float p1, float p2, float p3)
{
    struct bezier3 ret = {p0, p1, p2, p3};
    return ret;
}

int ngli_distmap_add_shape(struct distmap *s, int32_t shape_w, int32_t shape_h,
                           const struct path *path, uint32_t flags, int32_t *shape_id)
{
    if (shape_w <= 0 || shape_h <= 0) {
        LOG(ERROR, "invalid shape dimensions %dx%d", shape_w, shape_h);
        return NGL_ERROR_INVALID_ARG;
    }

    int32_t nb_beziers = 0, nb_beziergroups = 0;
    const struct darray *segments_array = ngli_path_get_segments(path);
    const struct path_segment *segments = ngli_darray_data(segments_array);
    for (size_t i = 0; i < ngli_darray_count(segments_array); i++) {
        const struct path_segment *segment = &segments[i];

        /* Extend all lines and bézier curves to cubic bézier curves */
        const float *x = segment->bezier_x;
        const float *y = segment->bezier_y;
        struct bezier3 bezier_x, bezier_y;
        switch (segment->degree) {
        case 1:
            bezier_x = b3_from_line(x[0], x[1]);
            bezier_y = b3_from_line(y[0], y[1]);
            break;
        case 2:
            bezier_x = b3_from_bezier2(x[0], x[1], x[2]);
            bezier_y = b3_from_bezier2(y[0], y[1], y[2]);
            break;
        case 3:
            bezier_x = b3_from_bezier3(x[0], x[1], x[2], x[3]);
            bezier_y = b3_from_bezier3(y[0], y[1], y[2], y[3]);
            break;
        default:
            ngli_assert(0);
        }

        if (!ngli_darray_push(&s->bezier_x, &bezier_x) ||
            !ngli_darray_push(&s->bezier_y, &bezier_y))
            return NGL_ERROR_MEMORY;

        /* Artificially insert a closing segment if necessary */
        if ((flags & NGLI_DISTMAP_FLAG_PATH_AUTO_CLOSE) && (segment->flags & NGLI_PATH_SEGMENT_FLAG_OPEN_END)) {
            const struct path_segment *segment0 = &segments[i - nb_beziers];
            const float *x0 = segment0->bezier_x;
            const float *y0 = segment0->bezier_y;
            const struct bezier3 bezier_x_close = b3_from_line(bezier_x.p3, x0[0]);
            const struct bezier3 bezier_y_close = b3_from_line(bezier_y.p3, y0[0]);
            if (!ngli_darray_push(&s->bezier_x, &bezier_x_close) ||
                !ngli_darray_push(&s->bezier_y, &bezier_y_close))
                return NGL_ERROR_MEMORY;
            nb_beziers++;
        }

        nb_beziers++;

        /* A group of bézier curves ends when a sub-shape is closed or we reach an open end */
        if (segment->flags & (NGLI_PATH_SEGMENT_FLAG_CLOSING | NGLI_PATH_SEGMENT_FLAG_OPEN_END)) {
            const int closed = (segment->flags & NGLI_PATH_SEGMENT_FLAG_CLOSING) || (flags & NGLI_DISTMAP_FLAG_PATH_AUTO_CLOSE);
            /* Pass down the closing flag to the shader using negative integers */
            const int32_t bezier_count = (closed ? -1 : 1) * nb_beziers;
            if (!ngli_darray_push(&s->bezier_counts, &bezier_count))
                return NGL_ERROR_MEMORY;
            nb_beziergroups++;
            nb_beziers = 0;
        }
    }

    ngli_assert(nb_beziers == 0);

    const struct shape shape = {.width=shape_w, .height=shape_h};
    if (!ngli_darray_push(&s->shapes, &shape) ||
        !ngli_darray_push(&s->beziergroup_counts, &nb_beziergroups))
        return NGL_ERROR_MEMORY;

    s->max_shape_w = NGLI_MAX(shape_w, s->max_shape_w);
    s->max_shape_h = NGLI_MAX(shape_h, s->max_shape_h);

    *shape_id = (int32_t)ngli_darray_count(&s->shapes) - 1;
    return 0;
}

static int32_t get_beziergroup_start(const struct distmap *s, int32_t shape_id)
{
    const int32_t *group_counts = ngli_darray_data(&s->beziergroup_counts);
    int32_t group_count = 0;
    for (int32_t i = 0; i < shape_id; i++)
        group_count += group_counts[i];
    return group_count;
}

static int32_t get_beziergroup_end(const struct distmap *s, int32_t shape_id)
{
    const int32_t group_start = get_beziergroup_start(s, shape_id);
    const int32_t *group_counts = ngli_darray_data(&s->beziergroup_counts);
    return group_start + group_counts[shape_id];
}

static int32_t get_bezier_start(const struct distmap *s, int32_t shape_id)
{
    const int32_t group_start = get_beziergroup_start(s, shape_id);
    const int32_t *counts = ngli_darray_data(&s->bezier_counts);
    int32_t start = 0;
    for (int32_t i = 0; i < group_start; i++)
        start += abs(counts[i]);
    return start;
}

static int32_t get_bezier_end(const struct distmap *s, int32_t shape_id)
{
    const int32_t group_end = get_beziergroup_end(s, shape_id);
    const int32_t *counts = ngli_darray_data(&s->bezier_counts);
    int32_t end = 0;
    for (int32_t i = 0; i < group_end; i++)
        end += abs(counts[i]);
    return end;
}

/*
 * Get the maximum number of beziers across all shapes. This is useful to
 * get how large the bezier uniform buffer must be (it will be re-used for
 * each shape).
 */
static int32_t get_max_beziers_per_shape(const struct distmap *s)
{
    int32_t max_beziers = 0;
    const int32_t *counts = ngli_darray_data(&s->bezier_counts);
    const int32_t *group_counts = ngli_darray_data(&s->beziergroup_counts);
    for (size_t i = 0; i < ngli_darray_count(&s->beziergroup_counts); i++) {
        const int32_t group_count = group_counts[i];

        int32_t sum = 0;
        for (int32_t j = 0; j < group_count; j++)
            sum += abs(counts[j]);
        counts += group_count;

        max_beziers = NGLI_MAX(max_beziers, sum);
    }
    return max_beziers;
}

static int32_t get_max_beziergroups_per_shape(const struct distmap *s)
{
    int32_t max_groups = 0;
    const int32_t *group_counts = ngli_darray_data(&s->beziergroup_counts);
    for (size_t i = 0; i < ngli_darray_count(&s->beziergroup_counts); i++)
        max_groups = NGLI_MAX(max_groups, group_counts[i]);
    return max_groups;
}

enum {
    VERTICES_INDEX,
};

enum {
    COORDS_INDEX,
    SCALE_INDEX,
    BEZIER_X_BUF_INDEX,
    BEZIER_Y_BUF_INDEX,
    BEZIER_COUNTS_INDEX,
    BEZIERGROUP_COUNT_INDEX,
};

static void load_buffers_data(struct distmap *s, uint8_t *vert_data, uint8_t *frag_data)
{
    const int32_t *bezier_counts = ngli_darray_data(&s->bezier_counts);
    const struct bezier3 *bezier_x = ngli_darray_data(&s->bezier_x);
    const struct bezier3 *bezier_y = ngli_darray_data(&s->bezier_y);

    const float qw = 1.f / (float)s->nb_cols;
    const float qh = 1.f / (float)s->nb_rows;

    const int32_t nb_shapes = (int32_t)ngli_darray_count(&s->shapes);
    int32_t shape_id = 0;

    for (int32_t y = 0; y < s->nb_rows; y++) {
        for (int32_t x = 0; x < s->nb_cols; x++) {
            if (shape_id == nb_shapes)
                return;

            const int32_t beziergroup_start_idx = get_beziergroup_start(s, shape_id);
            const int32_t beziergroup_end_idx   = get_beziergroup_end(s, shape_id);
            const int32_t beziergroup_count     = beziergroup_end_idx - beziergroup_start_idx;

            const int32_t bezier_start_idx      = get_bezier_start(s, shape_id);
            const int32_t bezier_end_idx        = get_bezier_end(s, shape_id);
            const int32_t bezier_count          = bezier_end_idx - bezier_start_idx;

            const struct shape *shape = ngli_darray_get(&s->shapes, shape_id);

            /*
             * Defines the quad coordinates of the atlas into which the glyph
             * distance must be drawn. The geometry respects the proportions of
             * the shape and is located on a grid of cells of the maximum size.
             */
            const int32_t padded_w = 2*s->pad + shape->width + 1;
            const int32_t padded_h = 2*s->pad + shape->height + 1;
            const float xr = (float)padded_w / (float)s->max_shape_padded_w;
            const float yr = (float)padded_h / (float)s->max_shape_padded_h;
            const float x0 = (float)x * qw;
            const float y0 = (float)y * qh;
            const float x1 = x0 + qw * xr;
            const float y1 = y0 + qh * yr;
            const float vertices[] = {x0, y0, x1, y1};

            /*
             * Given p for padding and m for pixel width or height, we have:
             * x₀ = p      (start of the shape, in pixels, without padding)
             * x₁ = p + m  (end of the shape, in pixels, without padding)
             *
             * If we consider 0 to be the start of the padded shape, and 1 its
             * width or height (basically the UV of the geometry), we can
             * identify the boundaries of the shape without padding:
             *
             * start = linear(x₀,x₁,0)    = -p/m
             * end   = linear(x₀,x₁,m+2p) = 1+p/m
             *
             * The +0.5 is used to take into account the extra texel used for
             * safe picking.
             */
            const float pad_w = ((float)s->pad + .5f) / (float)shape->width;
            const float pad_h = ((float)s->pad + .5f) / (float)shape->height;
            const float coords[] = {-pad_w, -pad_h, 1.f + pad_w, 1.f + pad_h};

            const float scale[] = {(float)shape->width * s->scale, (float)shape->height * s->scale};

            const struct ngpu_block_field_data vert_data_src[] = {
                [VERTICES_INDEX] = {.data=vertices},
            };

            const struct ngpu_block_field_data frag_data_src[] = {
                [COORDS_INDEX]            = {.data = coords},
                [SCALE_INDEX]             = {.data = scale},
                [BEZIER_X_BUF_INDEX]      = {.data = bezier_x + bezier_start_idx, .count = bezier_count},
                [BEZIER_Y_BUF_INDEX]      = {.data = bezier_y + bezier_start_idx, .count = bezier_count},
                [BEZIER_COUNTS_INDEX]     = {.data = bezier_counts + beziergroup_start_idx, .count = beziergroup_count},
                [BEZIERGROUP_COUNT_INDEX] = {.data = &beziergroup_count},
            };

            ngpu_block_desc_fields_copy(&s->vert_block, vert_data_src, vert_data);
            vert_data += s->vert_offset;
            ngpu_block_desc_fields_copy(&s->frag_block, frag_data_src, frag_data);
            frag_data += s->frag_offset;

            shape_id++;
        }
    }
}

static int map_and_load_buffers_data(struct distmap *s)
{
    uint8_t *vert_data = NULL;
    uint8_t *frag_data = NULL;

    int ret = ngpu_buffer_map(s->frag_buffer, 0, s->frag_buffer->size, (void **) &frag_data);
    if (ret < 0)
        goto end;

    ret = ngpu_buffer_map(s->vert_buffer, 0, s->vert_buffer->size, (void **) &vert_data);
    if (ret < 0)
        goto end;

    load_buffers_data(s, vert_data, frag_data);

end:
    if (vert_data)
        ngpu_buffer_unmap(s->vert_buffer);
    if (frag_data)
        ngpu_buffer_unmap(s->frag_buffer);
    return ret;
}

/*
 * Multiple draw calls (one for each shape) are executed instead of just a big
 * one wrapping them all because the number of beziers in the array can be
 * too large on certain platforms.
 */
static int draw_glyphs(struct distmap *s)
{
    int ret = map_and_load_buffers_data(s);
    if (ret < 0)
        return ret;

    ngli_pipeline_compat_update_buffer(s->pipeline_compat, 0, s->vert_buffer, 0, (int)s->vert_offset);
    ngli_pipeline_compat_update_buffer(s->pipeline_compat, 1, s->frag_buffer, 0, (int)s->frag_offset);

    const int32_t nb_shapes = (int32_t)ngli_darray_count(&s->shapes);
    int32_t shape_id = 0;

    for (int32_t y = 0; y < s->nb_rows; y++) {
        for (int32_t x = 0; x < s->nb_cols; x++) {
            if (shape_id == nb_shapes)
                return 0;

            const uint32_t offsets[] = {shape_id * (uint32_t)s->vert_offset, shape_id * (uint32_t)s->frag_offset};
            ret = ngli_pipeline_compat_update_dynamic_offsets(s->pipeline_compat, offsets, NGLI_ARRAY_NB(offsets));
            if (ret < 0)
                return ret;
            ngli_pipeline_compat_draw(s->pipeline_compat, 3, 1, 0);
            shape_id++;
        }
    }

    return 0;
}

static void reset_tmp_data(struct distmap *s)
{
    ngli_darray_reset(&s->bezier_x);
    ngli_darray_reset(&s->bezier_y);
    ngli_darray_reset(&s->bezier_counts);
    ngli_darray_reset(&s->beziergroup_counts);

    ngli_pipeline_compat_freep(&s->pipeline_compat);
    ngpu_block_desc_reset(&s->vert_block);
    ngpu_buffer_freep(&s->vert_buffer);
    ngpu_block_desc_reset(&s->frag_block);
    ngpu_buffer_freep(&s->frag_buffer);
    ngpu_pgcraft_freep(&s->crafter);
    ngpu_rendertarget_freep(&s->rt);
}

static struct bezier3 scaled_bezier(struct bezier3 bezier, float scale)
{
    bezier.p0 *= scale;
    bezier.p1 *= scale;
    bezier.p2 *= scale;
    bezier.p3 *= scale;
    return bezier;
}

static void normalize_coordinates(struct distmap *s)
{
    struct bezier3 *bezier_x = ngli_darray_data(&s->bezier_x);
    struct bezier3 *bezier_y = ngli_darray_data(&s->bezier_y);
    const size_t count = ngli_darray_count(&s->bezier_x);
    ngli_assert(count == ngli_darray_count(&s->bezier_y));
    for (size_t i = 0; i < count; i++) {
        bezier_x[i] = scaled_bezier(bezier_x[i], s->scale);
        bezier_y[i] = scaled_bezier(bezier_y[i], s->scale);
    }
}

#define DISTMAP_FEATURES (NGPU_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |               \
                          NGPU_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | \
                          NGPU_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)

static enum ngpu_format get_preferred_distmap_format(const struct distmap *s)
{
    struct ngpu_ctx *gpu_ctx = s->ctx->gpu_ctx;

    static const enum ngpu_format formats[] = {
            NGPU_FORMAT_R32_SFLOAT,
            NGPU_FORMAT_R16_SFLOAT,
            NGPU_FORMAT_R8_UNORM,
    };
    for (size_t i = 0; i < NGLI_ARRAY_NB(formats); i++) {
        const uint32_t features = ngpu_ctx_get_format_features(gpu_ctx, formats[i]);
        if (NGLI_HAS_ALL_FLAGS(features, DISTMAP_FEATURES))
            return formats[i];
    }
    ngli_assert(0);
}

int ngli_distmap_finalize(struct distmap *s)
{
    if (s->texture) {
        LOG(ERROR, "texture already generated");
        return NGL_ERROR_INVALID_USAGE;
    }

    const int32_t nb_shapes = (int32_t)ngli_darray_count(&s->shapes);
    if (!nb_shapes)
        return 0;

    /*
     * Assuming the path points are all within the view box
     * (0,0,max_shape_w,max_shape_h), the computed distance will never be larger
     * than the following:
     */
    const float longest_distance = hypotf(
        (float)(s->max_shape_w + s->pad) + .5f,
        (float)(s->max_shape_h + s->pad) + .5f
    );
    s->scale = 1.f / (float)longest_distance;

    /*
     * Define texture dimension (mostly squared).
     * TODO shapes are assumed to be square when balancing the number of rows
     * and cols, we're not taking into account max_shape_padded_[wh] as we should
     */
    s->nb_rows = (int32_t)lrintf(sqrtf((float)nb_shapes));
    s->nb_cols = (int32_t)ceilf((float)nb_shapes / (float)s->nb_rows);
    ngli_assert(s->nb_rows * s->nb_cols >= nb_shapes);

    /*
     * Padding needs to be the same length in both directions and for all
     * shapes so that effects are consistent whatever the ratio or size of a
     * given shape.
     */
    s->pad = NGLI_MAX(s->max_shape_w, s->max_shape_h) * PCENT_PADDING / 100;

    /*
     * +1 represents the extra half texel on each side used to prevent texture
     * bleeding between shapes because of the linear filtering.
     */
    s->max_shape_padded_w = s->max_shape_w + 2 * s->pad + 1;
    s->max_shape_padded_h = s->max_shape_h + 2 * s->pad + 1;

    /*
     * We normalize the coordinates with regards to the container shape so that
     * distances are within [0,1] while remaining proportionnal against each
     * others. This help making effects consistent accross all shapes.
     */
    normalize_coordinates(s);

    s->texture_w = s->max_shape_padded_w * s->nb_cols;
    s->texture_h = s->max_shape_padded_h * s->nb_rows;

    /*
     * Build pipeline and execute the computation of the complete signed
     * distance map.
     */
    struct ngpu_ctx *gpu_ctx = s->ctx->gpu_ctx;

    const struct ngpu_texture_params tex_params = {
        .type       = NGPU_TEXTURE_TYPE_2D,
        .width      = s->texture_w,
        .height     = s->texture_h,
        .format     = get_preferred_distmap_format(s),
        .min_filter = NGPU_FILTER_LINEAR,
        .mag_filter = NGPU_FILTER_LINEAR,
        .usage      = NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT
                      | NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT
                      | NGPU_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT
                      | NGPU_TEXTURE_USAGE_SAMPLED_BIT,
    };

    s->texture = ngpu_texture_create(gpu_ctx);
    if (!s->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngpu_texture_init(s->texture, &tex_params);
    if (ret < 0)
        return ret;

    const struct ngpu_rendertarget_params rt_params = {
        .width = s->texture_w,
        .height = s->texture_h,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = s->texture,
            .load_op    = NGPU_LOAD_OP_CLEAR,
            .store_op   = NGPU_STORE_OP_STORE,
        },
    };
    s->rt = ngpu_rendertarget_create(gpu_ctx);
    if (!s->rt)
        return NGL_ERROR_MEMORY;
    ret = ngpu_rendertarget_init(s->rt, &rt_params);
    if (ret < 0)
        return ret;

    const int32_t bezier_max_count = get_max_beziers_per_shape(s);
    const int32_t beziergroup_max_count = get_max_beziergroups_per_shape(s);

    const struct ngpu_block_field vert_fields[] = {
        [VERTICES_INDEX] = {.name="vertices", .type=NGPU_TYPE_VEC4},
    };

    const struct ngpu_block_field frag_fields[] = {
        [COORDS_INDEX]            = {.name="coords",            .type=NGPU_TYPE_VEC4},
        [SCALE_INDEX]             = {.name="scale",             .type=NGPU_TYPE_VEC2},
        [BEZIER_X_BUF_INDEX]      = {.name="bezier_x_buf",      .type=NGPU_TYPE_VEC4, .count=bezier_max_count},
        [BEZIER_Y_BUF_INDEX]      = {.name="bezier_y_buf",      .type=NGPU_TYPE_VEC4, .count=bezier_max_count},
        [BEZIER_COUNTS_INDEX]     = {.name="bezier_counts",     .type=NGPU_TYPE_I32,  .count=beziergroup_max_count},
        [BEZIERGROUP_COUNT_INDEX] = {.name="beziergroup_count", .type=NGPU_TYPE_I32},
    };

    ngpu_block_desc_init(gpu_ctx, &s->vert_block, NGPU_BLOCK_LAYOUT_STD140);
    ngpu_block_desc_init(gpu_ctx, &s->frag_block, NGPU_BLOCK_LAYOUT_STD140);

    if ((ret = ngpu_block_desc_add_fields(&s->vert_block, vert_fields, NGLI_ARRAY_NB(vert_fields))) < 0 ||
        (ret = ngpu_block_desc_add_fields(&s->frag_block, frag_fields, NGLI_ARRAY_NB(frag_fields))))
        return ret;

    s->vert_buffer = ngpu_buffer_create(gpu_ctx);
    s->frag_buffer = ngpu_buffer_create(gpu_ctx);
    if (!s->vert_buffer || !s->frag_buffer)
        return NGL_ERROR_MEMORY;

    s->vert_offset = ngpu_block_desc_get_aligned_size(&s->vert_block, 0);
    s->frag_offset = ngpu_block_desc_get_aligned_size(&s->frag_block, 0);

    static const uint32_t usage = NGPU_BUFFER_USAGE_UNIFORM_BUFFER_BIT | NGPU_BUFFER_USAGE_MAP_WRITE;
    if ((ret = ngpu_buffer_init(s->vert_buffer, nb_shapes * s->vert_offset, usage)) < 0 ||
        (ret = ngpu_buffer_init(s->frag_buffer, nb_shapes * s->frag_offset, usage)) < 0)
        return ret;

    const struct ngpu_pgcraft_block crafter_blocks[] = {
        {
            .name          = "vert",
            .instance_name = "",
            .type          = NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .stage         = NGPU_PROGRAM_SHADER_VERT,
            .block         = &s->vert_block,
        }, {
            .name          = "frag",
            .instance_name = "",
            .type          = NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .stage         = NGPU_PROGRAM_SHADER_FRAG,
            .block         = &s->frag_block,
        },
    };

    static const struct ngpu_pgcraft_iovar vert_out_vars[] = {
        {.name = "uv", .type = NGPU_TYPE_VEC2},
    };

    const struct ngpu_pgcraft_params crafter_params = {
        .vert_base        = distmap_vert,
        .frag_base        = distmap_frag,
        .blocks           = crafter_blocks,
        .nb_blocks        = NGLI_ARRAY_NB(crafter_blocks),
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
        .type = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics = {
            .topology     = NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state        = NGPU_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = s->rt->layout,
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

    ngpu_ctx_begin_render_pass(gpu_ctx, s->rt);

    ret = draw_glyphs(s);
    if (ret < 0)
        return ret;

    ngpu_ctx_end_render_pass(gpu_ctx);

    /*
     * Now that the distmap is rendered, the pipeline and other related
     * allocations are not needed anymore, we just have to keep the texture.
     */
    reset_tmp_data(s);

    return 0;
}

struct ngpu_texture *ngli_distmap_get_texture(const struct distmap *s)
{
    return s->texture;
}

void ngli_distmap_get_shape_coords(const struct distmap *s, int32_t shape_id, int32_t *dst)
{
    const struct shape *shape = ngli_darray_get(&s->shapes, shape_id);
    const int32_t col = shape_id % s->nb_cols;
    const int32_t row = shape_id / s->nb_cols;
    const int32_t x0 = col * s->max_shape_padded_w;
    const int32_t y0 = row * s->max_shape_padded_h;
    const int32_t x1 = x0 + 2*s->pad + shape->width + 1;
    const int32_t y1 = y0 + 2*s->pad + shape->height + 1;
    const int32_t coords[] = {x0, y0, x1, y1};
    memcpy(dst, coords, sizeof(coords));
}

void ngli_distmap_get_shape_scale(const struct distmap *s, int32_t shape_id, float *dst)
{
    const struct shape *shape = ngli_darray_get(&s->shapes, shape_id);
    const int32_t dst_w = 2*s->pad + shape->width + 1;
    const int32_t dst_h = 2*s->pad + shape->height + 1;
    const float scale_x = (float)dst_w / (float)shape->width;
    const float scale_y = (float)dst_h / (float)shape->height;
    const float scale[] = {scale_x, scale_y};
    memcpy(dst, scale, sizeof(scale));
}

void ngli_distmap_freep(struct distmap **dp)
{
    struct distmap *s = *dp;
    if (!s)
        return;
    reset_tmp_data(s);

    ngli_darray_reset(&s->shapes);
    ngpu_texture_freep(&s->texture);
    ngli_freep(dp);
}
