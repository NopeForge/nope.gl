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

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float ngli_vec3_length(float *v);
void ngli_vec3_sub(float *dst, float *v1, float *v2);
void ngli_vec3_norm(float *dst, float *v);
void ngli_vec3_cross(float *dst, float *v1, float *v2);
float ngli_vec3_dot(float *v1, float *v2);
void ngli_vec3_normalvec(float *dst, float *a, float *b, float *c);

void ngli_mat3_from_mat4(float *dst, const float *m);
void ngli_mat3_mul_scalar(float *dst, const float *m, const float s);
void ngli_mat3_transpose(float *dst, const float *m);
float ngli_mat3_determinant(const float *m);
void ngli_mat3_adjugate(float *dst, const float* m);
void ngli_mat3_inverse(float *dst, const float *m);

void ngli_mat4_identity(float *dst);
void ngli_mat4_mul_c(float *dst, const float *m1, const float *m2);
void ngli_mat4_mul_vec4_c(float *dst, const float *m, const float *v);
void ngli_mat4_look_at(float *dst, float *eye, float *center, float *up);
void ngli_mat4_perspective(float *dst, float fov, float aspect, float near, float far);

/* Arch specific versions */

#ifdef ARCH_AARCH64
# define ngli_mat4_mul          ngli_mat4_mul_aarch64
# define ngli_mat4_mul_vec4     ngli_mat4_mul_vec4_aarch64
#else
# define ngli_mat4_mul          ngli_mat4_mul_c
# define ngli_mat4_mul_vec4     ngli_mat4_mul_vec4_c
#endif

void ngli_mat4_mul_aarch64(float *dst, const float *m1, const float *m2);
void ngli_mat4_mul_vec4_aarch64(float *dst, const float *m, const float *v);

#endif
