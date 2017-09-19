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

#include <stdlib.h>
#include <math.h>

#include "utils.h"
#include "math_utils.h"

static void flt_diff(float *dst, const float *a, const float *b, int size)
{
    for (int i = 0; i < size; i++)
        dst[i] = a[i] - b[i];
}

static void flt_check(const float *f, int size)
{
    for (int i = 0; i < size; i++) {
        if (fabsf(f[i]) > 0.00001) {
            fprintf(stderr, "float %d/%d too large\n", i + 1, size);
            exit(1);
        }
    }
    printf("=> OK\n");
}

int main(void)
{
    static const NGLI_ALIGNED_MAT(m1) = {
        0.73016,  0.51184, 0.20930, -7.42311,
       -9.42693,  1.47287, 0.34995,  0.42049,
        0.42603, -1.50442, 1.34210,  3.04868,
        0.53013,  0.68963, 0.25207,  1.96254,
    };

    static const NGLI_ALIGNED_MAT(m2) = {
        0.08222, 0.62387, 0.79754,  0.64541,
        1.70126, 2.24977, 0.05395, -3.00599,
        0.30858, 0.90973, 0.84432, -4.01016,
        6.19681, 5.45165, 0.77647,  0.59262,
    };

    printf("m1:\n" NGLI_FMT_MAT4 "\n", NGLI_ARG_MAT4(m1));
    printf("m2:\n" NGLI_FMT_MAT4 "\n", NGLI_ARG_MAT4(m2));

    if (ngli_mat4_mul_c != ngli_mat4_mul) {
        printf(":: Testing mat4 mul\n");

        NGLI_ALIGNED_MAT(m_ref);
        NGLI_ALIGNED_MAT(m_out) = {0};
        NGLI_ALIGNED_MAT(m_diff);

        ngli_mat4_mul_c(m_ref, m1, m2);
        ngli_mat4_mul(m_out, m1, m2);
        flt_diff(m_diff, m_ref, m_out, 4*4);

        printf("ref:\n"  NGLI_FMT_MAT4 "\n", NGLI_ARG_MAT4(m_ref));
        printf("out:\n"  NGLI_FMT_MAT4 "\n", NGLI_ARG_MAT4(m_out));
        printf("diff:\n" NGLI_FMT_MAT4 "\n", NGLI_ARG_MAT4(m_diff));
        flt_check(m_diff, 4*4);
    }

    if (ngli_mat4_mul_vec4_c != ngli_mat4_mul_vec4) {
        for (int i = 0; i < 4; i++) {
            printf(":: Testing mat4 mul vec4 %d/4\n", i + 1);

            const float *v = &m2[i * 4];

            NGLI_ALIGNED_VEC(v_ref);
            NGLI_ALIGNED_VEC(v_out) = {0};
            NGLI_ALIGNED_VEC(v_diff);

            ngli_mat4_mul_vec4_c(v_ref, m1, v);
            ngli_mat4_mul_vec4(v_out, m1, v);
            flt_diff(v_diff, v_ref, v_out, 4);

            printf("ref:  " NGLI_FMT_VEC4 "\n", NGLI_ARG_VEC4(v_ref));
            printf("out:  " NGLI_FMT_VEC4 "\n", NGLI_ARG_VEC4(v_out));
            printf("diff: " NGLI_FMT_VEC4 "\n", NGLI_ARG_VEC4(v_diff));
            flt_check(v_diff, 4);
        }
    }

    return 0;
}
