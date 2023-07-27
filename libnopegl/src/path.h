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

#ifndef PATH_H
#define PATH_H

#include <stdint.h>

struct path;

#define NGLI_PATH_SEGMENT_FLAG_NEW_ORIGIN (1 << 0) /* the current segment does not overlap with the previous one */
#define NGLI_PATH_SEGMENT_FLAG_CLOSING    (1 << 1) /* the current segment is closing the sub-path */
#define NGLI_PATH_SEGMENT_FLAG_OPEN_END   (1 << 2) /* the current segment is ending a sub-path openly */

struct path_segment {
    int32_t degree;
    float poly_x[4];
    float poly_y[4];
    float poly_z[4];
    int step_start;
    float time_scale;
    uint32_t flags;
};

struct path *ngli_path_create(void);

/* Manual path construction primitive functions */
int ngli_path_move_to(struct path *s, const float *to);
int ngli_path_line_to(struct path *s, const float *to);
int ngli_path_bezier2_to(struct path *s, const float *ctl, const float *to);
int ngli_path_bezier3_to(struct path *s, const float *ctl0, const float *ctl1, const float *to);
int ngli_path_close(struct path *s);

/* Finalize construction: must be called at the end of the construction */
int ngli_path_finalize(struct path *s);

/*
 * Initialize a path. It is only required if one wants to evaluate the path at a
 * given point (calling ngli_path_evaluate()).
 */
int ngli_path_init(struct path *s, int32_t precision);

/* Evaluate an initialized path */
void ngli_path_evaluate(struct path *s, float *dst, float distance);

/*
 * Read back every segment. Require the path to be initialized or at least
 * finalized.
 */
const struct darray *ngli_path_get_segments(const struct path *s);

/*
 * Clear the segments. It is possible to re-use the same path to construct
 * another one, but it will require a new initialization.
 */
void ngli_path_clear(struct path *s);

void ngli_path_freep(struct path **sp);

#endif
