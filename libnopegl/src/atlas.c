/*
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

#include <math.h>
#include <string.h>

#include "atlas.h"
#include "internal.h"
#include "log.h"
#include "ngpu/format.h"
#include "ngpu/texture.h"
#include "nopegl.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"

struct atlas {
    struct ngl_ctx *ctx;

    uint32_t max_bitmap_w, max_bitmap_h;
    uint32_t texture_w, texture_h;
    uint32_t nb_rows, nb_cols;

    struct ngpu_texture *texture;
    struct darray bitmaps; // struct bitmap
};

static void free_bitmap(void *user_arg, void *data)
{
    struct bitmap *bitmap = data;
    ngli_freep(&bitmap->buffer);
}

struct atlas *ngli_atlas_create(struct ngl_ctx *ctx)
{
    struct atlas *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    ngli_darray_init(&s->bitmaps, sizeof(struct bitmap), 0);
    ngli_darray_set_free_func(&s->bitmaps, free_bitmap, NULL);
    return s;
}

int ngli_atlas_init(struct atlas *s)
{
    return 0;
}

int ngli_atlas_add_bitmap(struct atlas *s, const struct bitmap *bitmap, uint32_t *bitmap_id)
{
    if (ngli_darray_count(&s->bitmaps) == INT32_MAX)
        return NGL_ERROR_LIMIT_EXCEEDED;

    size_t buffer_size = bitmap->height * bitmap->stride;
    uint8_t *buffer = ngli_memdup(bitmap->buffer, buffer_size);
    if (!buffer)
        return NGL_ERROR_MEMORY;

    const struct bitmap copy = {
        .buffer = buffer,
        .stride = bitmap->stride,
        .width  = bitmap->width,
        .height = bitmap->height,
    };

    if (!ngli_darray_push(&s->bitmaps, &copy)) {
        ngli_freep(&buffer);
        return NGL_ERROR_MEMORY;
    }

    s->max_bitmap_w = NGLI_MAX(s->max_bitmap_w, bitmap->width);
    s->max_bitmap_h = NGLI_MAX(s->max_bitmap_h, bitmap->height);

    *bitmap_id = (uint32_t)ngli_darray_count(&s->bitmaps) - 1;
    return 0;
}

static void blend_bitmaps(struct atlas *s, uint8_t *data, size_t linesize)
{
    const struct bitmap *bitmaps = ngli_darray_data(&s->bitmaps);

    size_t bitmap_id = 0;
    for (size_t y = 0; y < s->nb_rows; y++) {
        for (size_t x = 0; x < s->nb_cols; x++) {
            if (bitmap_id == ngli_darray_count(&s->bitmaps))
                return;

            const struct bitmap *bitmap = &bitmaps[bitmap_id++];
            const size_t texel_x = x * s->max_bitmap_w;
            const size_t texel_y = y * s->max_bitmap_h;

            for (size_t line = 0; line < bitmap->height; line++) {
                uint8_t *dst = &data[(texel_y + line) * linesize + texel_x];
                const uint8_t *src = &bitmap->buffer[line * bitmap->stride];
                memcpy(dst, src, bitmap->width);
            }
        }
    }
}

int ngli_atlas_finalize(struct atlas *s)
{
    if (s->texture) {
        LOG(ERROR, "atlas is already finalized");
        return NGL_ERROR_INVALID_USAGE;
    }

    const int32_t nb_bitmaps = (int32_t)ngli_darray_count(&s->bitmaps);
    if (!nb_bitmaps)
        return 0;

    /*
     * Define texture dimension (mostly squared).
     * TODO bitmaps are assumed to be square when balancing the number of rows
     * and cols, we're not taking into account max_bitmap_[wh] as we should
     */
    s->nb_rows = (uint32_t)lrintf(sqrtf((float)nb_bitmaps));
    s->nb_cols = (uint32_t)ceilf((float)nb_bitmaps / (float)s->nb_rows);
    ngli_assert(s->nb_rows * s->nb_cols >= nb_bitmaps);

    s->texture_w = s->max_bitmap_w * s->nb_cols;
    s->texture_h = s->max_bitmap_h * s->nb_rows;

    const struct ngpu_texture_params tex_params = {
        .type       = NGPU_TEXTURE_TYPE_2D,
        .width      = s->texture_w,
        .height     = s->texture_h,
        .format     = NGPU_FORMAT_R8_UNORM,
        .min_filter = NGPU_FILTER_LINEAR,
        .mag_filter = NGPU_FILTER_NEAREST,
        .usage      = NGPU_TEXTURE_USAGE_TRANSFER_SRC_BIT
                      | NGPU_TEXTURE_USAGE_TRANSFER_DST_BIT
                      | NGPU_TEXTURE_USAGE_SAMPLED_BIT,
    };

    struct ngpu_ctx *gpu_ctx = s->ctx->gpu_ctx;
    s->texture = ngpu_texture_create(gpu_ctx);
    if (!s->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngpu_texture_init(s->texture, &tex_params);
    if (ret < 0)
        return ret;

    const size_t linesize = s->nb_cols * s->max_bitmap_w;
    if (linesize > INT32_MAX)
        return NGL_ERROR_LIMIT_EXCEEDED;
    void *data = ngli_calloc(s->nb_rows * s->max_bitmap_h, linesize);
    if (!data)
        return NGL_ERROR_MEMORY;

    blend_bitmaps(s, data, linesize);
    ret = ngpu_texture_upload(s->texture, data, (int) linesize);
    ngli_freep(&data);

    return ret;
}

struct ngpu_texture *ngli_atlas_get_texture(const struct atlas *s)
{
    return s->texture;
}

void ngli_atlas_get_bitmap_coords(const struct atlas *s, uint32_t bitmap_id, uint32_t *dst)
{
    const struct bitmap *bitmap = ngli_darray_get(&s->bitmaps, bitmap_id);
    const uint32_t col = bitmap_id % s->nb_cols;
    const uint32_t row = bitmap_id / s->nb_cols;
    const uint32_t x0 = col * s->max_bitmap_w;
    const uint32_t y0 = row * s->max_bitmap_h;
    const uint32_t x1 = x0 + bitmap->width;
    const uint32_t y1 = y0 + bitmap->height;
    const uint32_t coords[] = {x0, y0, x1, y1};
    memcpy(dst, coords, sizeof(coords));
}

void ngli_atlas_freep(struct atlas **sp)
{
    struct atlas *s = *sp;
    if (!s)
        return;
    ngli_darray_reset(&s->bitmaps);
    ngpu_texture_freep(&s->texture);
    ngli_freep(sp);
}
