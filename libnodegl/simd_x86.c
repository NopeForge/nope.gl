/*
 * Copyright 2022 GoPro Inc.
 * Copyright 2022 Clément Bœsch <u pkh.me>
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

#include <immintrin.h>

#include "math_utils.h"

void ngli_mat4_mul_sse(float *dst, const float *m1, const float *m2)
{
    __m128 m1_0 = _mm_load_ps(m1);
    __m128 m1_1 = _mm_load_ps(m1 + 4);
    __m128 m1_2 = _mm_load_ps(m1 + 8);
    __m128 m1_3 = _mm_load_ps(m1 + 12);

    __m128 r0 = _mm_mul_ps(m1_0, _mm_set1_ps(m2[0]));
    __m128 r1 = _mm_mul_ps(m1_0, _mm_set1_ps(m2[4]));
    __m128 r2 = _mm_mul_ps(m1_0, _mm_set1_ps(m2[8]));
    __m128 r3 = _mm_mul_ps(m1_0, _mm_set1_ps(m2[12]));

    r0 = _mm_add_ps(r0, _mm_mul_ps(m1_1, _mm_set1_ps(m2[1 + 0])));
    r1 = _mm_add_ps(r1, _mm_mul_ps(m1_1, _mm_set1_ps(m2[1 + 4])));
    r2 = _mm_add_ps(r2, _mm_mul_ps(m1_1, _mm_set1_ps(m2[1 + 8])));
    r3 = _mm_add_ps(r3, _mm_mul_ps(m1_1, _mm_set1_ps(m2[1 + 12])));

    r0 = _mm_add_ps(r0, _mm_mul_ps(m1_2, _mm_set1_ps(m2[2 + 0])));
    r1 = _mm_add_ps(r1, _mm_mul_ps(m1_2, _mm_set1_ps(m2[2 + 4])));
    r2 = _mm_add_ps(r2, _mm_mul_ps(m1_2, _mm_set1_ps(m2[2 + 8])));
    r3 = _mm_add_ps(r3, _mm_mul_ps(m1_2, _mm_set1_ps(m2[2 + 12])));

    r0 = _mm_add_ps(r0, _mm_mul_ps(m1_3, _mm_set1_ps(m2[3 + 0])));
    r1 = _mm_add_ps(r1, _mm_mul_ps(m1_3, _mm_set1_ps(m2[3 + 4])));
    r2 = _mm_add_ps(r2, _mm_mul_ps(m1_3, _mm_set1_ps(m2[3 + 8])));
    r3 = _mm_add_ps(r3, _mm_mul_ps(m1_3, _mm_set1_ps(m2[3 + 12])));

    _mm_store_ps(dst,      r0);
    _mm_store_ps(dst + 4,  r1);
    _mm_store_ps(dst + 8,  r2);
    _mm_store_ps(dst + 12, r3);
}

void ngli_mat4_mul_vec4_sse(float *dst, const float *m, const float *v)
{
    __m128 m0 = _mm_load_ps(m);
    __m128 m1 = _mm_load_ps(m + 4);
    __m128 m2 = _mm_load_ps(m + 8);
    __m128 m3 = _mm_load_ps(m + 12);

    __m128 r0 = _mm_mul_ps(m0, _mm_set1_ps(v[0]));
    __m128 r1 = _mm_mul_ps(m1, _mm_set1_ps(v[1]));
    __m128 r2 = _mm_mul_ps(m2, _mm_set1_ps(v[2]));
    __m128 r3 = _mm_mul_ps(m3, _mm_set1_ps(v[3]));

    __m128 r = _mm_add_ps(_mm_add_ps(_mm_add_ps(r0, r1), r2), r3);

    _mm_store_ps(dst, r);
}
