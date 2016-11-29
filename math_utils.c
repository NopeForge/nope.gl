/*
 * Copyright 2016 GoPro Inc.
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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "log.h"
#include "math_utils.h"

float ngli_vec3_length(float *v)
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

void ngli_vec3_sub(float *dst, float *v1, float *v2)
{
    dst[0] = v1[0] - v2[0];
    dst[1] = v1[1] - v2[1];
    dst[2] = v1[1] - v2[2];
}

void ngli_vec3_norm(float *dst, float *v)
{
    const float l = 1.0f / ngli_vec3_length(v);

    dst[0] = v[0] * l;
    dst[1] = v[1] * l;
    dst[2] = v[2] * l;
}

void ngli_vec3_cross(float *dst, float *v1, float *v2)
{
    float v[3];

    v[0] = v1[1]*v2[2] - v1[2]*v2[1];
    v[1] = v1[2]*v2[0] - v1[0]*v2[2];
    v[2] = v1[0]*v2[1] - v1[1]*v2[0];

    memcpy(dst, v, sizeof(v));
}

float ngli_vec3_dot(float *v1, float *v2)
{
    return v1[0]*v2[0]
         + v1[1]*v2[1]
         + v1[2]*v2[2];
}

void ngli_mat3_print(const float *m)
{
    for (int i = 0; i < 9; i += 3)
        LOG(INFO, "%g %g %g", m[i + 0], m[i + 1], m[i + 2]);
}

void ngli_mat3_from_mat4(float *dst, const float *m)
{
    memcpy(dst,     m,     3 * sizeof(*m));
    memcpy(dst + 3, m + 4, 3 * sizeof(*m));
    memcpy(dst + 6, m + 8, 3 * sizeof(*m));
}

void ngli_mat3_mul_scalar(float *dst, const float *m, const float s)
{
    float tmp[3*3];

    for (int i = 0; i < 9; i++) {
        tmp[i] = m[i] * s;
    }

    memcpy(dst, tmp, sizeof(tmp));
}

void ngli_mat3_transpose(float *dst, const float *m)
{
    float tmp[3*3];

    tmp[0] = m[0];
    tmp[1] = m[3];
    tmp[2] = m[6];

    tmp[3] = m[1];
    tmp[4] = m[4];
    tmp[5] = m[7];

    tmp[6] = m[2];
    tmp[7] = m[5];
    tmp[8] = m[8];

    memcpy(dst, tmp, sizeof(tmp));
}

float ngli_mat3_determinant(const float *m)
{
    float ret;

    ret  = m[0]*m[4]*m[8] + m[1]*m[5]*m[6] + m[2]*m[3]*m[7];
    ret -= m[2]*m[4]*m[6] + m[1]*m[3]*m[8] + m[0]*m[5]*m[7];

    return ret;
}

void ngli_mat3_adjugate(float *dst, const float* m)
{
    float tmp[3*3];

    tmp[0] = m[4]*m[8] - m[5]*m[7];
    tmp[1] = m[2]*m[7] - m[1]*m[8];
    tmp[2] = m[1]*m[5] - m[2]*m[4];
    tmp[3] = m[5]*m[6] - m[3]*m[8];
    tmp[4] = m[0]*m[8] - m[2]*m[6];
    tmp[5] = m[2]*m[3] - m[0]*m[5];
    tmp[6] = m[3]*m[7] - m[4]*m[6];
    tmp[7] = m[1]*m[6] - m[0]*m[7];
    tmp[8] = m[0]*m[4] - m[1]*m[3];

    memcpy(dst, tmp, sizeof(tmp));
}

void ngli_mat3_inverse(float *dst, const float *m)
{
    float a[3*3];
    float det = ngli_mat3_determinant(m);

    if (det == 0.0) {
        memcpy(dst, m, 3 * 3 * sizeof(*m));
        return;
    }

    ngli_mat3_adjugate(a, m);
    ngli_mat3_mul_scalar(dst, a, 1.0 / det);
}

void ngli_mat4_print(const float *m)
{
    for (int i = 0; i < 16; i += 4) {
        LOG(INFO, "%g %g %g %g", m[i + 0], m[i + 1], m[i + 2], m[i + 3]);
    }
}

void ngli_mat4_mul(float *dst, const float *m1, const float *m2)
{
    float m[4*4];

    m[ 0] = m1[0]*m2[ 0] + m1[4]*m2[ 1] + m1[ 8]*m2[ 2] + m1[12]*m2[ 3];
    m[ 1] = m1[1]*m2[ 0] + m1[5]*m2[ 1] + m1[ 9]*m2[ 2] + m1[13]*m2[ 3];
    m[ 2] = m1[2]*m2[ 0] + m1[6]*m2[ 1] + m1[10]*m2[ 2] + m1[14]*m2[ 3];
    m[ 3] = m1[3]*m2[ 0] + m1[7]*m2[ 1] + m1[11]*m2[ 2] + m1[15]*m2[ 3];

    m[ 4] = m1[0]*m2[ 4] + m1[4]*m2[ 5] + m1[ 8]*m2[ 6] + m1[12]*m2[ 7];
    m[ 5] = m1[1]*m2[ 4] + m1[5]*m2[ 5] + m1[ 9]*m2[ 6] + m1[13]*m2[ 7];
    m[ 6] = m1[2]*m2[ 4] + m1[6]*m2[ 5] + m1[10]*m2[ 6] + m1[14]*m2[ 7];
    m[ 7] = m1[3]*m2[ 4] + m1[7]*m2[ 5] + m1[11]*m2[ 6] + m1[15]*m2[ 7];

    m[ 8] = m1[0]*m2[ 8] + m1[4]*m2[ 9] + m1[ 8]*m2[10] + m1[12]*m2[11];
    m[ 9] = m1[1]*m2[ 8] + m1[5]*m2[ 9] + m1[ 9]*m2[10] + m1[13]*m2[11];
    m[10] = m1[2]*m2[ 8] + m1[6]*m2[ 9] + m1[10]*m2[10] + m1[14]*m2[11];
    m[11] = m1[3]*m2[ 8] + m1[7]*m2[ 9] + m1[11]*m2[10] + m1[15]*m2[11];

    m[12] = m1[0]*m2[12] + m1[4]*m2[13] + m1[ 8]*m2[14] + m1[12]*m2[15];
    m[13] = m1[1]*m2[12] + m1[5]*m2[13] + m1[ 9]*m2[14] + m1[13]*m2[15];
    m[14] = m1[2]*m2[12] + m1[6]*m2[13] + m1[10]*m2[14] + m1[14]*m2[15];
    m[15] = m1[3]*m2[12] + m1[7]*m2[13] + m1[11]*m2[14] + m1[15]*m2[15];

    memcpy(dst, m, sizeof(m));
}

void ngli_mat4_mul_vec4(float *dst, const float *m, const float *v)
{
    dst[0] = m[ 0]*v[0] + m[ 1]*v[1] + m[ 2]*v[2] + m[ 3]*v[3];
    dst[1] = m[ 4]*v[0] + m[ 5]*v[1] + m[ 6]*v[2] + m[ 7]*v[3];
    dst[2] = m[ 8]*v[0] + m[ 9]*v[1] + m[10]*v[2] + m[11]*v[3];
    dst[3] = m[12]*v[0] + m[13]*v[1] + m[14]*v[2] + m[15]*v[3];
}

void ngli_mat4_look_at(float *dst, float *eye, float *center, float *up)
{
    float f[3];
    float s[3];
    float u[3];

    ngli_vec3_sub(f, center, eye);
    ngli_vec3_norm(f, f);

    ngli_vec3_cross(s, f, up);
    ngli_vec3_norm(s, s);

    ngli_vec3_cross(u, s, f);

    dst[ 0] =  s[0];
    dst[ 1] =  u[0];
    dst[ 2] = -f[0];
    dst[ 3] =  0.0;

    dst[ 4] =  s[1];
    dst[ 5] =  u[1];
    dst[ 6] = -f[1];
    dst[ 7] =  0.0;

    dst[ 8] =  s[2];
    dst[ 9] =  u[2];
    dst[10] = -f[2];
    dst[11] =  0.0;

    dst[12] = -ngli_vec3_dot(s, eye);
    dst[13] = -ngli_vec3_dot(u, eye);
    dst[14] =  ngli_vec3_dot(f, eye);
    dst[15] =  1.0;
}

static void mat4_identity(float *dst)
{
    static const float id[4*4] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    memcpy(dst, id, sizeof(id));
}

void ngli_mat4_perspective(float *dst, float fov, float aspect, float near, float far)
{
    const float r = fov / 2 * M_PI / 180.0f;
    const float s = sin(r);
    const float z = far - near;

    mat4_identity(dst);

    if (z == 0 || s == 0 || aspect == 0) {
        return;
    }

    const float c = cos(r) / s;

    dst[ 0] =  c / aspect;
    dst[ 5] =  c;
    dst[10] = -(far + near) / z;
    dst[11] = -1;
    dst[14] = -2 * near * far / z;
    dst[15] =  0;
}
