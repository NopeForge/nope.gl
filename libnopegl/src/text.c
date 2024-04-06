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

#include <string.h>

#include "darray.h"
#include "gpu_ctx.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "text.h"
#include "transforms.h"
#include "utils.h"

extern const struct text_cls ngli_text_builtin;
extern const struct text_cls ngli_text_external;

struct box_stats {
    enum writing_mode writing_mode;
    struct darray linelens;         // int32_t, length of each line (not necessarily horizontal)
    int32_t max_linelen;            // maximum value in the linelens array
    int32_t linemin, linemax;       // current line min/max
    int32_t xmin, xmax;             // current box min/max on x-axis
    int32_t ymin, ymax;             // current box min/max on y-axis
};

static void box_stats_init(struct box_stats *s, enum writing_mode writing_mode)
{
    s->writing_mode = writing_mode;
    ngli_darray_init(&s->linelens, sizeof(int32_t), 0);
    s->max_linelen = INT32_MIN;
    s->linemin = s->xmin = s->ymin = INT32_MAX;
    s->linemax = s->xmax = s->ymax = INT32_MIN;
}

static int box_stats_register_eol(struct box_stats *s)
{
    const int32_t len = s->linemax == INT32_MIN ? 0 : s->linemax - s->linemin;
    if (!ngli_darray_push(&s->linelens, &len))
        return NGL_ERROR_MEMORY;
    s->max_linelen = NGLI_MAX(s->max_linelen, len);
    s->linemin = INT32_MAX;
    s->linemax = INT32_MIN;
    return 0;
}

static void box_stats_register_chr(struct box_stats *s, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (s->writing_mode == NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB) {
        s->linemin = NGLI_MIN(s->linemin, x);
        s->linemax = NGLI_MAX(s->linemax, x + w);
    } else {
        s->linemin = NGLI_MIN(s->linemin, y);
        s->linemax = NGLI_MAX(s->linemax, y + h);
    }
    s->xmin = NGLI_MIN(s->xmin, x);
    s->xmax = NGLI_MAX(s->xmax, x + w);
    s->ymin = NGLI_MIN(s->ymin, y);
    s->ymax = NGLI_MAX(s->ymax, y + h);
}

static void box_stats_reset(struct box_stats *s)
{
    ngli_darray_reset(&s->linelens);
    memset(s, 0, sizeof(*s));
}

static int build_stats(struct box_stats *stats, struct darray *chars_array, enum writing_mode writing_mode)
{
    box_stats_init(stats, writing_mode);

    const struct char_info_internal *chars_internal = ngli_darray_data(chars_array);
    for (size_t i = 0; i < ngli_darray_count(chars_array); i++) {
        const struct char_info_internal *chr = &chars_internal[i];
        if (chr->tags & NGLI_TEXT_CHAR_TAG_GLYPH)
            box_stats_register_chr(stats, chr->x, chr->y, chr->w, chr->h);

        if (chr->tags & NGLI_TEXT_CHAR_TAG_LINE_BREAK) {
            int ret = box_stats_register_eol(stats);
            if (ret < 0)
                return ret;
        }
    }

    /* We simulate an EOF to make sure the last line length is taken into account */
    int ret = box_stats_register_eol(stats);
    if (ret < 0)
        return ret;

    return 0;
}

struct text *ngli_text_create(struct ngl_ctx *ctx)
{
    struct text *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_text_init(struct text *s, const struct text_config *cfg)
{
    s->config = *cfg;

    ngli_darray_init(&s->chars, sizeof(struct char_info), 0);
    ngli_darray_init(&s->chars_internal, sizeof(struct char_info_internal), 0);

    s->effects = ngli_calloc(cfg->nb_effect_nodes, sizeof(*s->effects));
    if (!s->effects)
        return NGL_ERROR_MEMORY;

    s->cls = cfg->font_faces ? &ngli_text_external : &ngli_text_builtin;
    if (s->cls->priv_size) {
        s->priv_data = ngli_calloc(1, s->cls->priv_size);
        if (!s->priv_data)
            return NGL_ERROR_MEMORY;
    }
    return s->cls->init(s);
}

/* Apply the new defaults to the user exposed data */
static void reset_chars_data_to_defaults(struct text *s)
{
    memcpy(s->chars_data, s->chars_data_default, s->chars_copy_size);
}

static struct text_data_pointers get_chr_data_pointers(float *base, size_t nb_chars)
{
    struct text_data_pointers ptrs = {0};
    ptrs.pos_size     = base;
    ptrs.atlas_coords = ptrs.pos_size     + nb_chars * 4;
    ptrs.transform    = ptrs.atlas_coords + nb_chars * 4;
    ptrs.color        = ptrs.transform    + nb_chars * 4 * 4;
    ptrs.outline      = ptrs.color        + nb_chars * 4;
    ptrs.glow         = ptrs.outline      + nb_chars * 4;
    ptrs.blur         = ptrs.glow         + nb_chars * 4;
    return ptrs;
}

struct default_data {
    float pos_size[4];
    float atlas_coords[4];
    float transform[4 * 4];
    float color[4];
    float outline[4];
    float glow[4];
    float blur;
};

static void set_geometry_data(struct text *s, struct text_data_pointers ptrs)
{
    /* Text/Box ratio */
    const struct ngli_box box = s->config.box;
    const struct viewport viewport = ngli_gpu_ctx_get_viewport(s->ctx->gpu_ctx);
    const int32_t ar[] = {viewport.width, viewport.height};
    const float box_ratio = (float)ar[0] * box.w / ((float)ar[1] * box.h);
    const float text_ratio = (float)s->width / (float)s->height;

    /* Apply aspect ratio and font scaling */
    float width  = box.w * s->config.font_scale;
    float height = box.h * s->config.font_scale;
    float ratio_w, ratio_h;
    if (s->config.scale_mode == NGLI_TEXT_SCALE_MODE_FIXED) {
        const float tw = (float)s->width / (float)viewport.width;
        const float th = (float)s->height / (float)viewport.height;
        ratio_w = tw / box.w;
        ratio_h = th / box.h;
    } else {
        if (text_ratio < box_ratio) {
            ratio_w = text_ratio / box_ratio;
            ratio_h = 1.0;
        } else {
            ratio_w = 1.0;
            ratio_h = box_ratio / text_ratio;
        }
    }
    width  *= ratio_w;
    height *= ratio_h;

    /* Adjust text position according to alignment settings */
    const float align_padw = box.w - width;
    const float align_padh = box.h - height;

    const float spx = (s->config.halign == NGLI_TEXT_HALIGN_CENTER ? .5f :
                       s->config.halign == NGLI_TEXT_HALIGN_RIGHT  ? 1.f :
                       0.f);
    const float spy = (s->config.valign == NGLI_TEXT_VALIGN_CENTER ? .5f :
                       s->config.valign == NGLI_TEXT_VALIGN_TOP    ? 1.f :
                       0.f);

    const float corner_x = box.x + align_padw * spx;
    const float corner_y = box.y + align_padh * spy;

    const struct char_info *chars = ngli_darray_data(&s->chars);

    /* character dimension and position */
    for (size_t n = 0; n < ngli_darray_count(&s->chars); n++) {
        const struct char_info *chr = &chars[n];
        const float chr_width  = width  * chr->geom.w;
        const float chr_height = height * chr->geom.h;
        const float chr_corner_x = corner_x + width  * chr->geom.x;
        const float chr_corner_y = corner_y + height * chr->geom.y;
        const float transform[] = {chr_corner_x, chr_corner_y, chr_width, chr_height};
        memcpy(ptrs.pos_size + 4 * n, transform, sizeof(transform));
    }

    /* register atlas identifier */
    for (size_t n = 0; n < ngli_darray_count(&s->chars); n++) {
        const struct char_info *chr = &chars[n];
        memcpy(ptrs.atlas_coords + 4 * n, chr->atlas_coords, sizeof(chr->atlas_coords));
    }
}

/* Fill default buffers (1 row per character) with the default data. */
static void fill_default_data_buffers(struct text *s, size_t nb_chars)
{
    /* Default data for each character */
    const struct default_data default_data = {
        .transform = NGLI_MAT4_IDENTITY,
        .color     = {NGLI_ARG_VEC3(s->config.defaults.color), s->config.defaults.opacity},
        .outline   = {1.f, .7f, 0.f, 0.f},
        .glow      = {1.f, 1.f, 1.f, 0.f},
        .blur      = 0.f,
    };

    const struct text_data_pointers defaults_ptr = get_chr_data_pointers(s->chars_data_default, nb_chars);

    set_geometry_data(s, defaults_ptr);

    // Loop is repeated to make memory accesses contiguous.
    for (size_t i = 0; i < nb_chars; i++) memcpy(defaults_ptr.transform + i * 4 * 4, default_data.transform, sizeof(default_data.transform));
    for (size_t i = 0; i < nb_chars; i++) memcpy(defaults_ptr.color     + i * 4,     default_data.color,     sizeof(default_data.color));
    for (size_t i = 0; i < nb_chars; i++) memcpy(defaults_ptr.outline   + i * 4,     default_data.outline,   sizeof(default_data.outline));
    for (size_t i = 0; i < nb_chars; i++) memcpy(defaults_ptr.glow      + i * 4,     default_data.glow,      sizeof(default_data.glow));
    for (size_t i = 0; i < nb_chars; i++) memcpy(defaults_ptr.blur      + i,         &default_data.blur,     sizeof(default_data.blur));
}

void ngli_text_update_effects_defaults(struct text *s, const struct text_effects_defaults *defaults)
{
    s->config.defaults = *defaults;

    const size_t nb_chars = ngli_darray_count(&s->chars);
    if (nb_chars)
        fill_default_data_buffers(s, nb_chars);
}

void ngli_text_refresh_geometry_data(struct text *s)
{
    const size_t nb_chars = ngli_darray_count(&s->chars);
    if (!nb_chars)
        return;

    const struct text_data_pointers defaults_ptr = get_chr_data_pointers(s->chars_data_default, nb_chars);
    set_geometry_data(s, defaults_ptr);

    for (size_t i = 0; i < nb_chars; i++) memcpy(s->data_ptrs.pos_size,     defaults_ptr.pos_size,     nb_chars * 4 * sizeof(float));
    for (size_t i = 0; i < nb_chars; i++) memcpy(s->data_ptrs.atlas_coords, defaults_ptr.atlas_coords, nb_chars * 4 * sizeof(float));
}

static int set_value_from_node(float *dst, struct ngl_node *node, double t)
{
    int ret = ngli_node_update(node, t);
    if (ret < 0)
        return ret;
    const struct variable_info *v = node->priv_data;
    memcpy(dst, v->data, v->data_size);
    return 0;
}

static int set_f32_value(float *dst, struct ngl_node *node, float value, double t)
{
    if (node)
        return set_value_from_node(dst, node, t);
    if (value < 0.f)
        return 0;
    *dst = value;
    return 0;
}

static int set_vec3_value(float *dst, struct ngl_node *node, const float *value, double t)
{
    if (node)
        return set_value_from_node(dst, node, t);
    if (value[0] < 0.f)
        return 0;
    memcpy(dst, value, 3 * sizeof(*dst));
    return 0;
}

static int set_transform(float *dst, struct ngl_node *node, double t)
{
    if (!node)
        return 0;
    int ret = ngli_node_update(node, t);
    if (ret < 0)
        return ret;
    ngli_transform_chain_compute(node, dst);
    return 0;
}

static void segment_chars(const struct text *s, struct effect_segmentation *effect)
{
    size_t char_id = 0;
    size_t position = 0;

    const struct char_info_internal *chars = ngli_darray_data(&s->chars_internal);
    for (size_t i = 0; i < ngli_darray_count(&s->chars_internal); i++) {
        const struct char_info_internal *c = &chars[i];
        if (c->tags & NGLI_TEXT_CHAR_TAG_GLYPH)
            effect->positions[char_id++] = position;
        position++; // account for all the hidden characters as if they existed
    }
    ngli_assert(char_id == ngli_darray_count(&s->chars));
    effect->total_segments = position;
}

static void segment_chars_nospace(const struct text *s, struct effect_segmentation *effect)
{
    for (size_t i = 0; i < ngli_darray_count(&s->chars); i++)
        effect->positions[i] = i;
    effect->total_segments = ngli_darray_count(&s->chars);
}

static void segment_separator(const struct text *s, struct effect_segmentation *effect, uint32_t mask)
{
    int inside_target_element = 0;

    size_t char_id = 0;
    size_t position = 0;
    const struct char_info_internal *chars = ngli_darray_data(&s->chars_internal);
    for (size_t i = 0; i < ngli_darray_count(&s->chars_internal); i++) {
        const struct char_info_internal *c = &chars[i];

        if (c->tags & mask) {
            if (inside_target_element) {
                position++;
                inside_target_element = 0;
            }
        } else if (!inside_target_element) {
            effect->total_segments++;
            inside_target_element = 1;
        }

        if (!(c->tags & NGLI_TEXT_CHAR_TAG_GLYPH))
            continue;

        effect->positions[char_id++] = position;
    }
    ngli_assert(char_id == ngli_darray_count(&s->chars));
}

static void segment_words(const struct text *s, struct effect_segmentation *effect)
{
    segment_separator(s, effect, NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR | NGLI_TEXT_CHAR_TAG_LINE_BREAK);
}

static void segment_lines(const struct text *s, struct effect_segmentation *effect)
{
    segment_separator(s, effect, NGLI_TEXT_CHAR_TAG_LINE_BREAK);
}

static void segment_text(const struct text *s, struct effect_segmentation *effect)
{
    for (size_t i = 0; i < ngli_darray_count(&s->chars); i++)
        effect->positions[i] = 0;
    effect->total_segments = 1;
}

/*
 * SplitMix64, public domain code from Sebastiano Vigna (2015)
 * See https://xoshiro.di.unimi.it/splitmix64.c
 */
static uint64_t prng64_next(uint64_t *state)
{
    uint64_t z = *state;
    *state += 0x9e3779b97f4a7c15;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

static void shuffle(uint64_t *rng_state, size_t *positions, size_t n)
{
    for (size_t i = 0; i < n - 1; i++) {
        const size_t r = i + (size_t)prng64_next(rng_state) % (n - i);
        NGLI_SWAP(size_t, positions[i], positions[r]);
    }
}

static int build_effects_segmentation(struct text *s)
{
    for (size_t i = 0; i < s->config.nb_effect_nodes; i++) {
        struct ngl_node *effect_node = s->config.effect_nodes[i];
        struct texteffect_opts *effect_opts = effect_node->opts;
        struct effect_segmentation *effect = &s->effects[i];

        const size_t nb_chars = ngli_darray_count(&s->chars);
        size_t *new_positions = ngli_realloc(effect->positions, nb_chars, sizeof(*new_positions));
        if (!new_positions)
            return NGL_ERROR_MEMORY;
        effect->positions = new_positions;

        switch (effect_opts->target) {
        case NGLI_TEXT_EFFECT_CHAR:         segment_chars(s, effect);         break;
        case NGLI_TEXT_EFFECT_CHAR_NOSPACE: segment_chars_nospace(s, effect); break;
        case NGLI_TEXT_EFFECT_WORD:         segment_words(s, effect);         break;
        case NGLI_TEXT_EFFECT_LINE:         segment_lines(s, effect);         break;
        case NGLI_TEXT_EFFECT_TEXT:         segment_text(s, effect);          break;
        default:
            ngli_assert(0);
        }

        /*
         * This is not supposed to happen because of various early check making
         * sure there are characters printable and thus imply at least one
         * segment with all targets. This is not an assert due to the relatively
         * low confidence with regards to Unicode and fonts expectations in
         * general.
         */
        if (effect->total_segments == 0) {
            LOG(ERROR, "text segmentation failed, no segment found");
            return NGL_ERROR_BUG;
        }

        if (effect_opts->random) {
            /* Build a shuffle map associating a position with another one */
            size_t *shuffle_map = ngli_calloc(effect->total_segments, sizeof(*shuffle_map));
            if (!shuffle_map)
                return NGL_ERROR_MEMORY;
            for (size_t j = 0; j < effect->total_segments; j++)
                shuffle_map[j] = j;

            // TODO hash a seed when random_seed is 0 so that each random is unique per effect
            uint64_t rng_state = (uint64_t)effect_opts->random_seed;
            shuffle(&rng_state, shuffle_map, effect->total_segments);

            /* Apply the shuffle map */
            for (size_t j = 0; j < nb_chars; j++)
                effect->positions[j] = shuffle_map[effect->positions[j]];

            ngli_freep(&shuffle_map);
        }
    }

    return 0;
}

static void destroy_effects_data(struct text *s)
{
    /*
     * text.effects is not destroyed since its size depends on the number of
     * effects (which doesn't change). On the other hand, effects[i].positions
     * depends on the number of characters, so we're freeing them here.
     */
    if (s->effects)
        for (size_t i = 0; i < s->config.nb_effect_nodes; i++)
            ngli_freep(&s->effects[i].positions);

    ngli_freep(&s->chars_data_default);
    s->chars_data = NULL; // allocation is shared with chars_data_default
    s->chars_data_size = 0;

    memset(&s->data_ptrs, 0, sizeof(s->data_ptrs)); // user may still be reading them
}

static size_t next_pow2(size_t x)
{
    size_t p = 1;
    while (p < x)
        p *= 2;
    return p;
}

int ngli_text_set_string(struct text *s, const char *str)
{
    struct box_stats stats = {0};

    ngli_darray_clear(&s->chars);
    ngli_darray_clear(&s->chars_internal);

    int ret = s->cls->set_string(s, str, &s->chars_internal);
    if (ret < 0)
        goto end;

    /* Build bounding box statistics for the layout logic */
    build_stats(&stats, &s->chars_internal, s->config.writing_mode);

    /* Make sure it doesn't explode if the string is empty or only contains line breaks */
    if (stats.max_linelen <= 0) {
        s->width = 0;
        s->height = 0;
        destroy_effects_data(s);
        goto end;
    }

    const int32_t padding = NGLI_I32_TO_I26D6(s->config.padding);
    s->width  = NGLI_I26D6_TO_I32_TRUNCATED(stats.xmax - stats.xmin + 2 * padding);
    s->height = NGLI_I26D6_TO_I32_TRUNCATED(stats.ymax - stats.ymin + 2 * padding);

    /* Honor layout */
    int32_t line = 0;
    const int32_t *linelens = ngli_darray_data(&stats.linelens);
    struct char_info_internal *chars_internal = ngli_darray_data(&s->chars_internal);
    for (size_t i = 0; i < ngli_darray_count(&s->chars_internal); i++) {
        struct char_info_internal *chr = &chars_internal[i];

        if (chr->tags & NGLI_TEXT_CHAR_TAG_LINE_BREAK)
            line++;

        if (!(chr->tags & NGLI_TEXT_CHAR_TAG_GLYPH))
            continue;

        /* Recenter text so that it starts at (0,0) */
        chr->x -= stats.xmin;
        chr->y -= stats.ymin;

        chr->x += padding;
        chr->y += padding;

        /* Honor the alignment setting for each line */
        const int32_t space = stats.max_linelen - linelens[line];
        if (s->config.writing_mode == NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB) {
            if      (s->config.halign == NGLI_TEXT_HALIGN_CENTER) chr->x += space / 2;
            else if (s->config.halign == NGLI_TEXT_HALIGN_RIGHT)  chr->x += space;
        } else {
            if      (s->config.valign == NGLI_TEXT_VALIGN_CENTER) chr->y -= space / 2;
            else if (s->config.valign == NGLI_TEXT_VALIGN_BOTTOM) chr->y -= space;
        }
    }

    /* Expose characters publicly */
    for (size_t i = 0; i < ngli_darray_count(&s->chars_internal); i++) {
        const struct char_info_internal *chr_internal = &chars_internal[i];

        if (!(chr_internal->tags & NGLI_TEXT_CHAR_TAG_GLYPH))
            continue;

        /* Honor requested geometry scaling (anchor is at the center of the quad) */
        const float x = NGLI_I26D6_TO_F32(chr_internal->x);
        const float y = NGLI_I26D6_TO_F32(chr_internal->y);
        const float w = NGLI_I26D6_TO_F32(chr_internal->w);
        const float h = NGLI_I26D6_TO_F32(chr_internal->h);
        const float nw = w * chr_internal->scale[0];
        const float nh = h * chr_internal->scale[1];
        const float offx = (w - nw) / 2.f;
        const float offy = (h - nh) / 2.f;
        const struct ngli_box xywh = {x + offx, y + offy, nw, nh};

        const struct char_info chr = {
            .geom = {
                .x = xywh.x / (float)s->width,
                .y = xywh.y / (float)s->height,
                .w = xywh.w / (float)s->width,
                .h = xywh.h / (float)s->height,
            },
            .atlas_coords = {
                (float)chr_internal->atlas_coords[0] / (float)s->atlas_texture->params.width,
                (float)chr_internal->atlas_coords[1] / (float)s->atlas_texture->params.height,
                (float)chr_internal->atlas_coords[2] / (float)s->atlas_texture->params.width,
                (float)chr_internal->atlas_coords[3] / (float)s->atlas_texture->params.height,
            },
        };

        if (!ngli_darray_push(&s->chars, &chr)) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }
    }

    /*
     * Reallocate characters data if the number of characters changed. We could
     * use a "<" instead of "!=" but we don't want to keep large amount of
     * memory allocated in case one live change event set a large string which
     * is later trimed down. To reduce the number of reallocations when the
     * lengths of the successive strings updates are in the same vicinity, we
     * stitch the number of characters to the next power of two.
     */
    const size_t nb_chars = ngli_darray_count(&s->chars);
    const size_t alloc_count = next_pow2(nb_chars);
    const size_t needed_size = alloc_count * sizeof(struct default_data);
    if (s->chars_data_size != needed_size) {
        /*
         * The x2 is because we duplicate the data for the defaults,
         * which is the reference data we use to reset all the characters
         * properties at every frame. The default data is positioned
         * first for a more predictible read/write memory access in
         * reset_chars_data_to_defaults()
         */
        float *new_chars_data_default = ngli_realloc(s->chars_data_default, 2, needed_size);
        if (!new_chars_data_default)
            return NGL_ERROR_MEMORY;
        s->chars_data_default = new_chars_data_default;
        s->chars_data_size = needed_size;
        s->chars_data = (void *)((uint8_t *)s->chars_data_default + s->chars_data_size);
    }

    const size_t copy_size = nb_chars * sizeof(struct default_data);
    if (copy_size != s->chars_copy_size) {
        /* We don't need to copy the rounded size, only the actual number of characters */
        s->chars_copy_size = copy_size;
        s->data_ptrs = get_chr_data_pointers(s->chars_data, nb_chars);
    }

    /*
     * This is done unconditionally to make sure the geometry defaults are
     * updated, even if the number of characters didn't change (for example
     * because they might be different characters with different dimensions)
     */
    fill_default_data_buffers(s, nb_chars);

    reset_chars_data_to_defaults(s);

    /* Assign each character to an effect */
    ret = build_effects_segmentation(s);
    if (ret < 0)
        return ret;

end:
    box_stats_reset(&stats);
    return ret;
}

int ngli_text_set_time(struct text *s, double t)
{
    if (!ngli_darray_count(&s->chars))
        return 0;

    reset_chars_data_to_defaults(s);

    for (size_t i = 0; i < s->config.nb_effect_nodes; i++) {
        struct ngl_node *effect_node = s->config.effect_nodes[i];
        struct texteffect_opts *effect_opts = effect_node->opts;

        const double end_time = effect_opts->end_time < 0.0 ? s->ctx->scene->params.duration : effect_opts->end_time;
        if (t < effect_opts->start_time || t > end_time)
            continue;

        /* Remap scene time to effect time */
        const double effect_t = NGLI_LINEAR_NORM(effect_opts->start_time, end_time, t);

        /* Update the range-selector nodes using the effect time */
        int ret;
        float start_pos = 0.f, end_pos = 1.f, overlap = 0.f; // set a default to avoid uninitialized value in case of negative parameter value
        if ((ret = set_f32_value(&start_pos, effect_opts->start_pos_node, effect_opts->start_pos, effect_t)) < 0 ||
            (ret = set_f32_value(&end_pos,   effect_opts->end_pos_node,   effect_opts->end_pos,   effect_t)) < 0 ||
            (ret = set_f32_value(&overlap,   effect_opts->overlap_node,   effect_opts->overlap,   effect_t)) < 0)
            return ret;

        const struct effect_segmentation *effect = &s->effects[i];

        const size_t nb_elems = effect->total_segments;
        const double duration  = 1.f / ((double)nb_elems - overlap * (double)(nb_elems - 1));
        const double timescale = (1.f - overlap) * duration;

        /* Apply effect on the selected range of characters */
        for (size_t c = 0; c < ngli_darray_count(&s->chars); c++) {
            const size_t pos = effect->positions[c];

            /* Recenter the position in the middle of the character (similar to texture sampling) */
            const float pos_f = ((float)pos + 0.5f) / (float)nb_elems;

            /* Spacially filter out characters that do not land into the user specified range */
            if (pos_f < start_pos || pos_f > end_pos)
                continue;

            /* Interpolate the time of the target, taking into account the overlap */
            const double prev_t = timescale * (double)pos;
            const double next_t = prev_t + duration;
            const double target_t = NGLI_LINEAR_NORM(prev_t, next_t, effect_t);

            if ((ret = set_transform( s->data_ptrs.transform  + c * 4 * 4, effect_opts->transform_chain,                                target_t)) < 0 ||
                (ret = set_vec3_value(s->data_ptrs.color      + c * 4,     effect_opts->color_node,         effect_opts->color,         target_t)) < 0 ||
                (ret = set_f32_value( s->data_ptrs.color      + c * 4 + 3, effect_opts->opacity_node,       effect_opts->opacity,       target_t)) < 0 ||
                (ret = set_vec3_value(s->data_ptrs.outline    + c * 4,     effect_opts->outline_color_node, effect_opts->outline_color, target_t)) < 0 ||
                (ret = set_f32_value( s->data_ptrs.outline    + c * 4 + 3, effect_opts->outline_node,       effect_opts->outline,       target_t)) < 0 ||
                (ret = set_vec3_value(s->data_ptrs.glow       + c * 4,     effect_opts->glow_color_node,    effect_opts->glow_color,    target_t)) < 0 ||
                (ret = set_f32_value( s->data_ptrs.glow       + c * 4 + 3, effect_opts->glow_node,          effect_opts->glow,          target_t)) < 0 ||
                (ret = set_f32_value( s->data_ptrs.blur       + c,         effect_opts->blur_node,          effect_opts->blur,          target_t)) < 0)
                return ret;
        }
    }

    return 0;
}

void ngli_text_freep(struct text **sp)
{
    struct text *s = *sp;
    if (s->cls->reset)
        s->cls->reset(s);
    ngli_freep(&s->priv_data);
    destroy_effects_data(s);
    ngli_freep(&s->effects);
    ngli_darray_reset(&s->chars);
    ngli_darray_reset(&s->chars_internal);
    ngli_freep(sp);
}
