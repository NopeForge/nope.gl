/*
 * Copyright 2023 Nope Project
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
#include "darray.h"
#include "format.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "nopegl.h"
#include "texture.h"
#include "utils.h"

struct atlas {
    struct ngl_ctx *ctx;

    int32_t max_bitmap_w, max_bitmap_h;
    int32_t texture_w, texture_h;
    int32_t nb_rows, nb_cols;

    struct texture *texture;
    struct darray bitmaps; // struct bitmap
};

struct atlas *ngli_atlas_create(struct ngl_ctx *ctx)
{
    struct atlas *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    ngli_darray_init(&s->bitmaps, sizeof(struct bitmap), 0);
    return s;
}

int ngli_atlas_init(struct atlas *s)
{
    return 0;
}

int ngli_atlas_add_bitmap(struct atlas *s, const struct bitmap *bitmap, int32_t *bitmap_id)
{
    if (ngli_darray_count(&s->bitmaps) == INT32_MAX)
        return NGL_ERROR_LIMIT_EXCEEDED;

    uint8_t *buffer = ngli_memdup(bitmap->buffer, bitmap->height * bitmap->stride);
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

    *bitmap_id = (int32_t)ngli_darray_count(&s->bitmaps) - 1;
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

    /*
     * Define texture dimension (mostly squared).
     * TODO bitmaps are assumed to be square when balancing the number of rows
     * and cols, we're not taking into account max_bitmap_[wh] as we should
     */
    const int32_t nb_bitmaps = (int32_t)ngli_darray_count(&s->bitmaps);
    s->nb_rows = (int32_t)lrintf(sqrtf((float)nb_bitmaps));
    s->nb_cols = (int32_t)ceilf((float)nb_bitmaps / (float)s->nb_rows);
    ngli_assert(s->nb_rows * s->nb_cols >= nb_bitmaps);

    s->texture_w = s->max_bitmap_w * s->nb_cols;
    s->texture_h = s->max_bitmap_h * s->nb_rows;

    const struct texture_params tex_params = {
        .type       = NGLI_TEXTURE_TYPE_2D,
        .width      = s->texture_w,
        .height     = s->texture_h,
        .format     = NGLI_FORMAT_R8_UNORM,
        .min_filter = NGLI_FILTER_LINEAR,
        .mag_filter = NGLI_FILTER_NEAREST,
        .usage      = NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT
                    | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT
                    | NGLI_TEXTURE_USAGE_SAMPLED_BIT,
    };

    struct gpu_ctx *gpu_ctx = s->ctx->gpu_ctx;
    s->texture = ngli_texture_create(gpu_ctx);
    if (!s->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngli_texture_init(s->texture, &tex_params);
    if (ret < 0)
        return ret;

    const size_t linesize = s->nb_cols * s->max_bitmap_w;
    if (linesize > INT32_MAX)
        return NGL_ERROR_LIMIT_EXCEEDED;
    void *data = ngli_calloc(s->nb_rows * s->max_bitmap_h, linesize);
    if (!data)
        return NGL_ERROR_MEMORY;

    blend_bitmaps(s, data, linesize);
    ngli_texture_upload(s->texture, data, (int)linesize);
    ngli_freep(&data);

    return 0;
}

struct texture *ngli_atlas_get_texture(const struct atlas *s)
{
    return s->texture;
}

void ngli_atlas_get_dimensions(const struct atlas *s, int32_t *dim)
{
    dim[0] = s->nb_cols;
    dim[1] = s->nb_rows;
}

void ngli_atlas_freep(struct atlas **sp)
{
    struct atlas *s = *sp;
    if (!s)
        return;
    struct bitmap *bitmaps = ngli_darray_data(&s->bitmaps);
    for (size_t i = 0; i < ngli_darray_count(&s->bitmaps); i++) {
        struct bitmap *bitmap = &bitmaps[i];
        ngli_freep(&bitmap->buffer);
    }
    ngli_texture_freep(&s->texture);
    ngli_freep(sp);
}
