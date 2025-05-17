/*
 * Copyright 2020-2022 GoPro Inc.
 * Copyright 2020-2022 Clément Bœsch <u pkh.me>
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

#include "log.h"
#include "math_utils.h"
#include "path.h"
#include "utils/darray.h"
#include "utils/memory.h"

#define STEP_FLAG_DISCONTINUITY (1 << 0) /* a discontinuity happens after this step */

struct path_step {
    float position[3];
    int segment_id;
    uint32_t flags;
};

enum path_state {
    PATH_STATE_DEFAULT,
    PATH_STATE_FINALIZED,
    PATH_STATE_INITIALIZED,
};

struct path {
    int32_t precision;
    enum path_state state;
    int current_arc;            /* cached arc index */
    int *arc_to_segment;        /* map arc indexes to segment indexes */
    struct darray segments;     /* array of struct path_segment */
    struct darray steps;        /* array of struct path_step */
    struct darray steps_dist;   /* array of floats */
    float origin[3];            /* temporary origin for the current sub-path */
    float cursor[3];            /* temporary cursor used during path construction */
    uint32_t segment_flags;     /* temporary segment flags used during path construction */
};

struct path *ngli_path_create(void)
{
    struct path *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->state = PATH_STATE_DEFAULT;
    ngli_darray_init(&s->steps, sizeof(struct path_step), 0);
    ngli_darray_init(&s->steps_dist, sizeof(float), 0);
    ngli_darray_init(&s->segments, sizeof(struct path_segment), 0);
    return s;
}

static void poly_from_line(float *dst, float p0, float p1)
{
    dst[0 /* a */] = 0.f;
    dst[1 /* b */] = 0.f;
    dst[2 /* c */] = -p0 + p1;
    dst[3 /* d */] =  p0;
}

/*
 * Convert from original bézier quadratic form:
 *   B₂(t) = (1-t)² p0 + 2(1-t)t p1 + t² p2
 * To polynomial form:
 *   B₂(t) = at² + bt + c
 */
static void poly_from_bezier2(float *dst, float p0, float p1, float p2)
{
    dst[0 /* a */] = 0.f;
    dst[1 /* b */] =      p0 - 2.f*p1 + p2;
    dst[2 /* c */] = -2.f*p0 + 2.f*p1;
    dst[3 /* d */] =      p0;
}

/*
 * Convert from original bézier cubic form:
 *   B₃(t) = (1-t)³ p0 + 3(1-t)²t p1 + 3(1-t)t² p2 + t³ p3
 * To polynomial form:
 *   B₃(t) = at³ + bt² + ct + d
 */
static void poly_from_bezier3(float *dst, float p0, float p1, float p2, float p3)
{
    dst[0 /* a */] =     -p0 + 3.f*p1 - 3.f*p2 + p3;
    dst[1 /* b */] =  3.f*p0 - 6.f*p1 + 3.f*p2;
    dst[2 /* c */] = -3.f*p0 + 3.f*p1;
    dst[3 /* d */] =      p0;
}

static int add_segment_and_move(struct path *s, const struct path_segment *segment, const float *to)
{
    ngli_assert(s->state == PATH_STATE_DEFAULT);
    if (!ngli_darray_push(&s->segments, segment))
        return NGL_ERROR_MEMORY;
    memcpy(s->cursor, to, sizeof(s->cursor));
    s->segment_flags &= ~NGLI_PATH_SEGMENT_FLAG_NEW_ORIGIN;
    return 0;
}

static void set_segment_closing_flag(struct path *s)
{
    struct path_segment *last = ngli_darray_tail(&s->segments);

    /*
     * The flag check prevents the case where 2 successive moves would set the
     * closing flag.
     */
    if (!last || (last->flags & (NGLI_PATH_SEGMENT_FLAG_CLOSING | NGLI_PATH_SEGMENT_FLAG_OPEN_END)))
        return;

    const int origin_is_dst = !memcmp(s->origin, s->cursor, sizeof(s->cursor));
    last->flags |= origin_is_dst ? NGLI_PATH_SEGMENT_FLAG_CLOSING : NGLI_PATH_SEGMENT_FLAG_OPEN_END;
}

int ngli_path_move_to(struct path *s, const float *to)
{
    set_segment_closing_flag(s);
    memcpy(s->origin, to, sizeof(s->origin));
    memcpy(s->cursor, to, sizeof(s->cursor));
    s->segment_flags |= NGLI_PATH_SEGMENT_FLAG_NEW_ORIGIN;
    return 0;
}

int ngli_path_line_to(struct path *s, const float *to)
{
    if (!memcmp(s->cursor, to, sizeof(s->cursor)))
        return 0;

    const struct path_segment segment = {
        .degree = 1,
        .bezier_x = {s->cursor[0], to[0]},
        .bezier_y = {s->cursor[1], to[1]},
        .bezier_z = {s->cursor[2], to[2]},
        .flags  = s->segment_flags,
    };
    return add_segment_and_move(s, &segment, to);
}

int ngli_path_bezier2_to(struct path *s, const float *ctl, const float *to)
{
    if (!memcmp(s->cursor, ctl, sizeof(s->cursor)) &&
        !memcmp(s->cursor, to, sizeof(s->cursor)))
        return 0;

    const struct path_segment segment = {
        .degree = 2,
        .bezier_x = {s->cursor[0], ctl[0], to[0]},
        .bezier_y = {s->cursor[1], ctl[1], to[1]},
        .bezier_z = {s->cursor[2], ctl[2], to[2]},
        .flags = s->segment_flags,
    };
    return add_segment_and_move(s, &segment, to);
}

int ngli_path_bezier3_to(struct path *s, const float *ctl0, const float *ctl1, const float *to)
{
    if (!memcmp(s->cursor, ctl0, sizeof(s->cursor)) &&
        !memcmp(s->cursor, ctl1, sizeof(s->cursor)) &&
        !memcmp(s->cursor, to, sizeof(s->cursor)))
        return 0;

    const struct path_segment segment = {
        .degree = 3,
        .bezier_x = {s->cursor[0], ctl0[0], ctl1[0], to[0]},
        .bezier_y = {s->cursor[1], ctl0[1], ctl1[1], to[1]},
        .bezier_z = {s->cursor[2], ctl0[2], ctl1[2], to[2]},
        .flags = s->segment_flags,
    };
    return add_segment_and_move(s, &segment, to);
}

int ngli_path_close(struct path *s)
{
    int ret = ngli_path_line_to(s, s->origin);
    if (ret < 0)
        return ret;

    struct path_segment *last = ngli_darray_tail(&s->segments);
    ngli_assert(last);
    last->flags |= NGLI_PATH_SEGMENT_FLAG_CLOSING;
    return 0;
}

static const char *strip_separators(const char *s)
{
    while (*s && strchr(" ,\t\r\n", *s)) /* comma + whitespaces */
        s++;
    return s;
}

static const char *load_coords(float *dst, const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        s = strip_separators(s);
        dst[i] = strtof(s, (char **)&s);
    }
    return s;
}

static const char *cmd_get_coords(float *dst, const char *s, char lcmd)
{
    switch (lcmd) {
        case 'm':
        case 'l': return load_coords(dst, s, 2); // x,y
        case 'v':
        case 'h': return load_coords(dst, s, 1); // x or y
        case 'q': return load_coords(dst, s, 2 * 2); // x0,y0 x1,y1
        case 'c': return load_coords(dst, s, 3 * 2); // x0,y0 x1,y1 x2,y2
    }
    return NULL;
}

int ngli_path_add_svg_path(struct path *s, const char *str)
{
    char cmd = 0;

    for (;;) {
        int ret = 0;

        str = strip_separators(str);
        if (!*str)
            break;

        if (strchr("mMvVhHlLqQcCzZ", *str)) {
            cmd = *str++;
        } else if (strchr("sStTaA", *str)) {
            // TODO
            LOG(ERROR, "SVG path command '%c' is currently unsupported", *str);
            return NGL_ERROR_UNSUPPORTED;
        } else if (!cmd) {
            return NGL_ERROR_INVALID_DATA;
        }

        const char lcmd = cmd | 0x20; // lower case
        if (lcmd == 'z') { // closing current (sub-)path
            ret = ngli_path_close(s);
            if (ret < 0)
                return ret;
            continue;
        }

        const int relative = cmd == lcmd;
        const float off_x = relative ? s->cursor[0] : 0.f;
        const float off_y = relative ? s->cursor[1] : 0.f;

        float coords[3 * 2]; // maximum number of coordinates (bezier cubic)
        const char *p = cmd_get_coords(coords, str, lcmd);
        if (!p || p == str) // bail out in case of error or if the pointer didn't advance
            return NGL_ERROR_INVALID_DATA;
        str = p;

        if (lcmd == 'm') { // move
            const float to[] = {coords[0] + off_x, coords[1] + off_y, 0.f};
            ret = ngli_path_move_to(s, to);
        } else if (lcmd == 'l') { // line
            const float to[] = {coords[0] + off_x, coords[1] + off_y, 0.f};
            ret = ngli_path_line_to(s, to);
        } else if (lcmd == 'v') { // vertical line
            const float to[] = {off_x, coords[0] + off_y, 0.f};
            ret = ngli_path_line_to(s, to);
        } else if (lcmd == 'h') { // horizontal line
            const float to[] = {coords[0] + off_x, off_y, 0.f};
            ret = ngli_path_line_to(s, to);
        } else if (lcmd == 'q') { // quadratic bezier
            const float ctl[] = {coords[0] + off_x, coords[1] + off_y, 0.f};
            const float to[]  = {coords[2] + off_x, coords[3] + off_y, 0.f};
            ret = ngli_path_bezier2_to(s, ctl, to);
        } else if (lcmd == 'c') { // cubic bezier
            const float ctl1[] = {coords[0] + off_x, coords[1] + off_y, 0.f};
            const float ctl2[] = {coords[2] + off_x, coords[3] + off_y, 0.f};
            const float to[]   = {coords[4] + off_x, coords[5] + off_y, 0.f};
            ret = ngli_path_bezier3_to(s, ctl1, ctl2, to);
        } else {
            ngli_assert(0);
        }
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ngli_path_add_path(struct path *s, const struct path *path)
{
    ngli_assert(s->state == PATH_STATE_DEFAULT);

    const struct path_segment *segments = ngli_darray_data(&path->segments);
    for (size_t i = 0; i < ngli_darray_count(&path->segments); i++) {
        const struct path_segment *segment = &segments[i];
        if (!ngli_darray_push(&s->segments, segment))
            return NGL_ERROR_MEMORY;
    }

    memcpy(s->origin, path->origin, sizeof(s->origin));
    memcpy(s->cursor, path->cursor, sizeof(s->cursor));
    s->segment_flags = path->segment_flags;

    return 0;
}

/*
 * Interpolate a 3D point using the polynomials
 */
static void poly_eval(float *dst, const struct path_segment *segment, float t)
{
    const float *x = segment->poly_x;
    const float *y = segment->poly_y;
    const float *z = segment->poly_z;
    dst[0] = NGLI_POLY3(x[0], x[1], x[2], x[3], t);
    dst[1] = NGLI_POLY3(y[0], y[1], y[2], y[3], t);
    dst[2] = NGLI_POLY3(z[0], z[1], z[2], z[3], t);
}

void ngli_path_transform(struct path *s, const float *matrix)
{
    /*
     * We could be more strict here and only accept the default state, but it's
     * actually fine to do the transformation after the finalization as well because
     * the polynomials are not yet derived from the bezier points.
     */
    ngli_assert(s->state != PATH_STATE_INITIALIZED);

    struct path_segment *segments = ngli_darray_data(&s->segments);
    for (size_t i = 0; i < ngli_darray_count(&s->segments); i++) {
        struct path_segment *segment = &segments[i];

        const float *x = segment->bezier_x;
        const float *y = segment->bezier_y;
        const float *z = segment->bezier_z;

        NGLI_ALIGNED_VEC(p0) = {x[0], y[0], z[0], 1.f};
        NGLI_ALIGNED_VEC(p1) = {x[1], y[1], z[1], 1.f};
        NGLI_ALIGNED_VEC(p2) = {x[2], y[2], z[2], 1.f};
        NGLI_ALIGNED_VEC(p3) = {x[3], y[3], z[3], 1.f};

        ngli_mat4_mul_vec4(p0, matrix, p0);
        ngli_mat4_mul_vec4(p1, matrix, p1);
        ngli_mat4_mul_vec4(p2, matrix, p2);
        ngli_mat4_mul_vec4(p3, matrix, p3);

        const float xt[4] = {p0[0], p1[0], p2[0], p3[0]};
        const float yt[4] = {p0[1], p1[1], p2[1], p3[1]};
        const float zt[4] = {p0[2], p1[2], p2[2], p3[2]};

        memcpy(segment->bezier_x, xt, sizeof(xt));
        memcpy(segment->bezier_y, yt, sizeof(yt));
        memcpy(segment->bezier_z, zt, sizeof(zt));
    }
}

int ngli_path_finalize(struct path *s)
{
    ngli_assert(s->state == PATH_STATE_DEFAULT);
    set_segment_closing_flag(s);
    s->state = PATH_STATE_FINALIZED;
    return 0;
}

/*
 * Lexicon:
 *
 * - path: a set of segments, eventually with discontinuities (in case a move
 *   event occurred during the path construction)
 * - segment: the curve between 2 points forms a segment. Each segment can be a
 *   line, a quadratic bézier (1 control point) or a cubic bézier curve (2
 *   control points), all of them respectively expressed as a polynomials of
 *   the 1st, 2nd and 3rd degree. The segments form a chain where the end
 *   coordinate of one segment overlaps with the starting point of the next
 *   segment.
 * - step: a step is a coordinate on the curve; every segment is divided
 *   into an arbitrary number of `precision` steps.
 * - dist: growing distance between the origin of the path up to a given step:
 *   those are approximations of an arc length.
 * - arc: 2 steps form an arc, it represents a (usually small) chunk of a
 *   segment which can be approximated into a small straight line instead of a
 *   curved one.
 * - time: the "time" does *NOT* correspond to the time of a potential
 *   animation, it corresponds to the parameter passed down to the polynomial
 *   evaluation. With curves, this time is *NOT* correlated with the real clock
 *   time at all. See ngli_path_evaluate() for more information.
 */
int ngli_path_init(struct path *s, int32_t precision)
{
    ngli_assert(s->state == PATH_STATE_FINALIZED);

    if (precision < 1) {
        LOG(ERROR, "precision must be 1 or superior");
        return NGL_ERROR_INVALID_ARG;
    }
    s->precision = precision;

    const size_t nb_segments = ngli_darray_count(&s->segments);
    if (nb_segments < 1) {
        LOG(ERROR, "at least one segment must be defined");
        return NGL_ERROR_INVALID_ARG;
    }

    /* Compute polynomial forms from bézier points */
    struct path_segment *segments = ngli_darray_data(&s->segments);
    for (size_t i = 0; i < nb_segments; i++) {
        struct path_segment *segment = &segments[i];

        const float *x = segment->bezier_x;
        const float *y = segment->bezier_y;
        const float *z = segment->bezier_z;
        switch (segment->degree) {
        case 1:
            poly_from_line(segment->poly_x, x[0], x[1]);
            poly_from_line(segment->poly_y, y[0], y[1]);
            poly_from_line(segment->poly_z, z[0], z[1]);
            break;
        case 2:
            poly_from_bezier2(segment->poly_x, x[0], x[1], x[2]);
            poly_from_bezier2(segment->poly_y, y[0], y[1], y[2]);
            poly_from_bezier2(segment->poly_z, z[0], z[1], z[2]);
            break;
        case 3:
            poly_from_bezier3(segment->poly_x, x[0], x[1], x[2], x[3]);
            poly_from_bezier3(segment->poly_y, y[0], y[1], y[2], y[3]);
            poly_from_bezier3(segment->poly_z, z[0], z[1], z[2], z[3]);
            break;
        default:
            ngli_assert(0);
        }
    }

    /*
     * Build a temporary lookup table of data points ("steps") that will be
     * used for estimating the length (growing distances more specifically) of
     * the curve.
     */
    for (size_t i = 0; i < nb_segments; i++) {
        struct path_segment *segment = &segments[i];

        /*
         * Compared to curves, straight lines do not need to be divided into
         * small chunks because their length can be calculated exactly.
         */
        const int32_t segment_precision = segment->degree == 1 ? 1 : s->precision;

        /*
         * We're not using 1/(P-1) but 1/P for the scale because each segment is
         * composed of P+1 step points.
         */
        segment->step_start = (int)ngli_darray_count(&s->steps);
        segment->time_scale = 1.f / (float)segment_precision;

        /*
         * This loop only calculates P step coordinates per segment instead of
         * P+1 because the last step of a segment (at t=1) overlaps with the
         * first step of the next segment (t=0). The two exceptions to this are
         * handled in the next block.
         */
        for (int32_t k = 0; k < segment_precision; k++) {
            const float t = (float)k * segment->time_scale;
            struct path_step step = {.segment_id=(int)i};
            poly_eval(step.position, segment, t);
            if (!ngli_darray_push(&s->steps, &step))
                return NGL_ERROR_MEMORY;
        }

        /*
         * We check if we are at the very last step of the last segment, or in
         * the situation where a move order occurred between the current
         * segment and the next one. These 2 scenarios imply that we compute
         * the last point coordinate (t=1) of the current segment because there
         * won't be an overlap with the next segment (if any).
         */
        if (i == nb_segments - 1 || (segments[i + 1].flags & NGLI_PATH_SEGMENT_FLAG_NEW_ORIGIN)) {
            struct path_step step = {.segment_id=(int)i, .flags = STEP_FLAG_DISCONTINUITY};
            poly_eval(step.position, segment, 1.f);
            if (!ngli_darray_push(&s->steps, &step))
                return NGL_ERROR_MEMORY;
        }
    }

    /*
     * Build the growing distance (from step 0) of steps (including step 0).
     */
    float total_length = 0.f;

    if (!ngli_darray_push(&s->steps_dist, &total_length))
        return NGL_ERROR_MEMORY;

    const struct path_step *steps = ngli_darray_data(&s->steps);
    for (size_t i = 1; i < ngli_darray_count(&s->steps); i++) {
        const struct path_step *prv_step = &steps[i - 1];
        const struct path_step *cur_step = &steps[i];

        const int skip_distance = (prv_step->flags & STEP_FLAG_DISCONTINUITY);
        if (!skip_distance) {
            const float arc_vec[3] = NGLI_VEC3_SUB(cur_step->position, prv_step->position);
            const float arc_length = ngli_vec3_length(arc_vec);
            total_length += arc_length;
        }
        if (!ngli_darray_push(&s->steps_dist, &total_length))
            return NGL_ERROR_MEMORY;
    }

    /*
     * We have the same number of steps and distances between these steps
     * because the first step starts with a distance of 0.
     */
    ngli_assert(ngli_darray_count(&s->steps) == ngli_darray_count(&s->steps_dist));

    /*
     * Sanity check for get_vector_id(). We have it here to avoid having the
     * assert called redundantly in the inner loop.
     */
    ngli_assert((int)ngli_darray_count(&s->steps_dist) - 1 >= 1); // checks if number of arcs >= 1

    /* Normalize distances (relative to the total length of the path) */
    float *steps_dist = ngli_darray_data(&s->steps_dist);
    const float scale = total_length != 0.f ? 1.f / total_length : 0.f;
    for (size_t i = 0; i < ngli_darray_count(&s->steps_dist); i++)
        steps_dist[i] *= scale;

    /* Build a lookup table associating an arc to its segment */
    const size_t nb_arcs = ngli_darray_count(&s->steps) - 1;
    s->arc_to_segment = ngli_calloc(nb_arcs, sizeof(*s->arc_to_segment));
    if (!s->arc_to_segment)
        return NGL_ERROR_MEMORY;
    for (size_t i = 0; i < nb_arcs; i++)
        s->arc_to_segment[i] = steps[i].segment_id;

    /* We don't need to store all the intermediate positions anymore */
    ngli_darray_reset(&s->steps);

    s->state = PATH_STATE_INITIALIZED;
    return 0;
}

/*
 * Return the index of the vector where `value` belongs, starting the search
 * from index `*cache` (and looping back to 0 if necessary). A vector is
 * defined by 2 consecutive points in the `values` array, with `values`
 * composed of monotonically increasing values.
 *
 * The range of the returned index is within [0,nb_values-2].
 *
 * Example:
 *   values:  2 3 5 8 9
 *   indexes: |0|1|2|3|
 *
 *    input   | output  |
 *    value   | index   | comment
 *   -------- | ------- | -------
 *       6    |   2     | value is between 5 and 8
 *       1    |   0     | before start value, clamped to index 0
 *      15    |   3     | after end value, clamped to last index
 *
 */
static int get_vector_id(const float *values, int nb_values, int *cache, float value)
{
    int ret = -1;
    const int nb_indexes = nb_values - 1;
    const int start = *cache;

    for (int i = start; i < nb_indexes; i++) {
        if (values[i] > value)
            break;
        ret = i;
    }
    if (ret < 0) {
        for (int i = 0; i < nb_indexes; i++) {
            if (values[i] > value)
                break;
            ret = i;
        }
    }
    /*
     * We only need to clamp the negative boundary because ret can never reach
     * nb_indexes, meaning the maximum value is nb_indexes-1, or nb_values-2.
     */
    ret = NGLI_MAX(ret, 0);
    *cache = ret;
    return ret;
}

/* Remap x from [c,d] to [a,b] */
static float remap(float a, float b, float c, float d, float x)
{
    const float ratio = NGLI_LINEAR_NORM(c, d, x);
    return NGLI_MIX_F32(a, b, ratio);
}

/*
 * Convert the requested path distance into a segment curve "time" using the
 * estimated arc lengths, and use it to evaluate the polynomial of the current
 * segment.
 *
 * We want the time parameter of B(t) to be correlated to the traveled distance
 * on the bézier curves. Not doing this remapping would cause a non-linear
 * traveling of a point on that curve: the more a curve is twisted, the more
 * steps it contains, and thus the slower the movement would get.
 *
 * Unfortunately, there is no magic formula to get the length of a bézier
 * curve, so we have to rely on a simple approximation by dividing the segments
 * into many small arcs.
 *
 * https://pomax.github.io/bezierinfo/#arclength
 * https://pomax.github.io/bezierinfo/#arclengthapprox
 * https://pomax.github.io/bezierinfo/#tracing
 */
void ngli_path_evaluate(struct path *s, float *dst, float distance)
{
    const float *distances = ngli_darray_data(&s->steps_dist);
    const int nb_dists = (int)ngli_darray_count(&s->steps_dist);
    const int arc_id = get_vector_id(distances, nb_dists, &s->current_arc, distance);
    const int segment_id = s->arc_to_segment[arc_id];
    const struct path_segment *segments = ngli_darray_data(&s->segments);
    const struct path_segment *segment = &segments[segment_id];
    const int step0 = arc_id;
    const int step1 = arc_id + 1;
    const float t0 = (float)(step0 - segment->step_start) * segment->time_scale;
    const float t1 = (float)(step1 - segment->step_start) * segment->time_scale;
    const float d0 = distances[step0];
    const float d1 = distances[step1];
    const float t = remap(t0, t1, d0, d1, distance);
    poly_eval(dst, segment, t);
}

const struct darray *ngli_path_get_segments(const struct path *s)
{
    ngli_assert(s->state == PATH_STATE_INITIALIZED || s->state == PATH_STATE_FINALIZED);
    return &s->segments;
}

void ngli_path_clear(struct path *s)
{
    s->state = PATH_STATE_DEFAULT;
    s->precision = 0;
    s->current_arc = 0;
    ngli_freep(&s->arc_to_segment);
    ngli_darray_clear(&s->segments);
    ngli_darray_clear(&s->steps);
    ngli_darray_clear(&s->steps_dist);
    memset(s->origin, 0, sizeof(*s->origin));
    memset(s->cursor, 0, sizeof(*s->cursor));
    s->segment_flags = 0;
}

void ngli_path_freep(struct path **sp)
{
    struct path *s = *sp;
    if (!s)
        return;
    ngli_path_clear(s);
    ngli_darray_reset(&s->segments);
    ngli_darray_reset(&s->steps);
    ngli_darray_reset(&s->steps_dist);
    ngli_freep(sp);
}
