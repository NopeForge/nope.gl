/*
 * Copyright 2020-2022 GoPro Inc.
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
#include <stdio.h>
#include <string.h>

#include "path.h"
#include "utils.h"

#define NB_REFS (16 + 2)
#define MAX_ERR 1e-5

static int check_value(struct path *path, float t, const float *ref)
{
    float value[3];
    ngli_path_evaluate(path, value, t);
    if (!ref) {
        fprintf(stderr, "got:("NGLI_FMT_VEC3")\n", NGLI_ARG_VEC3(value));
        return 1;
    }
    const float err[3] = {
        fabsf(value[0] - ref[0]),
        fabsf(value[1] - ref[1]),
        fabsf(value[2] - ref[2]),
    };
    int ret = err[0] > MAX_ERR || err[1] > MAX_ERR || err[2] > MAX_ERR ||
              isnan(err[0]) || isnan(err[1]) || isnan(err[2]);
    fprintf(stderr, "%c t:%9f ref:("NGLI_FMT_VEC3") got:("NGLI_FMT_VEC3") err:("NGLI_FMT_VEC3")\n",
            ret ? '!' : ' ', t, NGLI_ARG_VEC3(ref), NGLI_ARG_VEC3(value), NGLI_ARG_VEC3(err));
    return ret;
}

/* Set refs=NULL to get the references if you don't have any yet */
static int evaluate_points(struct path *path, const float *refs, const char *title)
{
    printf("test: %s\n", title);

    int ret = 0;
    for (int i = 0; i < NB_REFS; i++) {
        /* We make sure t starts before 0 and ends after 1 to check for
         * outbounds */
        const float t = (i - 1) / ((NB_REFS - 2) - 1.f);
        ret |= check_value(path, t, refs ? &refs[i * 3] : NULL);
    }

    if (ret) {
        fprintf(stderr, "%s failed\n", title);
        return -1;
    }

    return 0;
}

static int test_bezier3_vec3(void)
{
    struct path *path = ngli_path_create();
    if (!path)
        return -1;

    static const float points[][3] = {
        {-0.7f,  0.0f,  0.3f},
        { 0.8f,  0.1f, -0.1f},
    };

    static const float controls[][3] = {
        {-0.2f, -0.3f,  0.2f},
        { 0.2f,  0.8f,  0.4f},
    };

    static const float refs[] = {
       -0.819247f,  0.0975617f,   0.329433f,
            -0.7f,        0.0f,        0.3f,
       -0.584421f, -0.0462202f,   0.281569f,
       -0.471648f, -0.0502734f,   0.271273f,
       -0.360823f, -0.0213343f,   0.266244f,
       -0.251085f,  0.0314227f,   0.263616f,
       -0.151888f,  0.0921073f,   0.260911f,
      -0.0559586f,   0.155526f,   0.255954f,
       0.0409618f,   0.217673f,   0.246764f,
        0.139444f,   0.272461f,   0.231438f,
        0.240059f,     0.3138f,   0.208073f,
        0.328808f,   0.333964f,    0.18007f,
        0.417249f,   0.336013f,    0.14468f,
        0.508256f,   0.317196f,   0.100088f,
        0.602161f,   0.273958f,  0.0451851f,
        0.699298f,   0.202744f, -0.0211406f,
             0.8f,        0.1f,       -0.1f,
        0.904601f, -0.0378311f,  -0.192504f,
    };

    int ret;
    if ((ret = ngli_path_move_to(path, points[0])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[0], controls[1], points[1])) < 0)
        goto end;

    ret = ngli_path_init(path, 3);
    if (ret < 0)
        goto end;

    ret = evaluate_points(path, refs, "3D cubic bezier");

end:
    ngli_path_freep(&path);
    return ret;
}

static int test_poly_bezier3(void)
{
    static const float points[][3] = {
        {-0.70f,  0.08f, 0.0f},
        {-0.15f,  0.06f, 0.0f},
        {-0.24f,  0.52f, 0.0f},
        { 0.23f,  0.15f, 0.0f},
        { 0.05f, -0.25f, 0.0f},
    };

    static const float controls[][3] = {
        { 0.45f, -0.59f, 0.0},
        {-1.1f,  -0.47f, 0.0},
        { 0.25f,  0.29f, 0.0},
        {-0.19f, -1.1f,  0.0},
        {-0.25f,  1.1f,  0.0},
        { 0.19f, -0.75f, 0.0},
        { 0.0f,   0.96f, 0.0},
        { 1.1f,  -0.86f, 0.0},
    };

    static const float refs[] = {
         -1.1044f,   0.292227f, 0.0f,
            -0.7f,       0.08f, 0.0f,
       -0.396405f,   -0.12542f, 0.0f,
       -0.339952f,  -0.380683f, 0.0f,
        -0.39831f,  -0.117848f, 0.0f,
      -0.0945053f,  0.0833274f, 0.0f,
      -0.0141996f,  -0.206802f, 0.0f,
       -0.190295f, -0.0496436f, 0.0f,
       -0.230724f,   0.314766f, 0.0f,
       -0.192237f,   0.608198f, 0.0f,
      -0.0468001f,   0.271875f, 0.0f,
            0.11f, -0.0588156f, 0.0f,
        0.223503f,   0.173489f, 0.0f,
        0.331497f,   0.245757f, 0.0f,
        0.486813f, -0.0851905f, 0.0f,
        0.390109f,   -0.37754f, 0.0f,
            0.05f,  -0.250001f, 0.0f,
        -0.35465f,  0.0274324f, 0.0f,
    };

    struct path *path = ngli_path_create();
    if (!path)
        return -1;

    int ret;
    if ((ret = ngli_path_move_to(path, points[0])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[0], controls[1], points[1])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[2], controls[3], points[2])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[4], controls[5], points[3])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[6], controls[7], points[4])) < 0)
        goto end;

    ret = ngli_path_init(path, 64);
    if (ret < 0)
        goto end;

    ret = evaluate_points(path, refs, "cubic poly-bezier");

end:
    ngli_path_freep(&path);
    return ret;
}

static int test_composition(void)
{
    static const float points[][3] = {
        {-0.6f,  0.2f, 0.0f},
        {-0.2f,  0.7f, 0.0f},
        { 0.5f,  0.3f, 0.0f},
        {-0.3f,  0.1f, 0.0f},
        { 0.1f, -0.2f, 0.0f},
        {-0.2f, -0.4f, 0.0f},
        { 0.3f, -0.6f, 0.0f},
        { 0.7f, -0.2f, 0.0f},
        {-0.4f, -0.1f, 0.0f},
        {-0.8f, -0.1f, 0.0f},
        {-0.6f,  0.2f, 0.0f},
    };

    static const float controls[][3] = {
        { 0.4f,  0.8f, 0.0f},
        { 0.0f,  0.1f, 0.0f},
        {-0.1f,  0.2f, 0.0f},
        { 0.6f, -0.6f, 0.0f},
        {-0.6f, -0.2f, 0.0f},
        {-0.8f,  0.4f, 0.0f},
        {-1.2f,  0.5f, 0.0f},
    };

    static const float refs[] = {
        -0.812889f,   -0.066111f, 0.0f,
             -0.6f,         0.2f, 0.0f,
        -0.387111f,    0.466111f, 0.0f,
        -0.158955f,     0.70579f, 0.0f,
         0.139265f,    0.592457f, 0.0f,
          0.27384f,    0.284733f, 0.0f,
        -0.197802f,     0.11846f, 0.0f,
        0.0472765f,    -0.10324f, 0.0f,
       -0.0918672f,   -0.327911f, 0.0f,
      -0.00425009f,     -0.4783f, 0.0f,
         0.313111f,   -0.599806f, 0.0f,
         0.599048f,    -0.44062f, 0.0f,
        -0.476377f,   -0.107769f, 0.0f,
        -0.741631f,    0.100257f, 0.0f,
         -0.86002f, -0.00575656f, 0.0f,
        -0.921486f,    0.295537f, 0.0f,
             -0.6f,         0.2f, 0.0f,
        -0.224002f,  -0.0146044f, 0.0f,
    };

    struct path *path = ngli_path_create();
    if (!path)
        return -1;

    int ret;
    if ((ret = ngli_path_move_to(path, points[0])) < 0 ||
        (ret = ngli_path_line_to(path, points[1])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[0], controls[1], points[2])) < 0 ||
        (ret = ngli_path_move_to(path, points[3])) < 0 ||
        (ret = ngli_path_bezier2_to(path, controls[2], points[4])) < 0 ||
        (ret = ngli_path_line_to(path, points[5])) < 0 ||
        (ret = ngli_path_line_to(path, points[6])) < 0 ||
        (ret = ngli_path_bezier2_to(path, controls[3], points[7])) < 0 ||
        (ret = ngli_path_move_to(path, points[8])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[4], controls[5], points[9])) < 0 ||
        (ret = ngli_path_bezier2_to(path, controls[6], points[10])) < 0)
        goto end;

    ret = ngli_path_init(path, 64);
    if (ret < 0)
        goto end;

    ret = evaluate_points(path, refs, "lines/bezier2/bezier3 with discontinuities");

end:
    ngli_path_freep(&path);
    return ret;
}

int main(int ac, char **av)
{
    if (test_bezier3_vec3() < 0 ||
        test_poly_bezier3() < 0 ||
        test_composition() < 0)
        return 1;
    return 0;
}
