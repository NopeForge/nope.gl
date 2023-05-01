/*
 * Copyright 2019-2022 GoPro Inc.
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
#include <math.h>

#include "image.h"
#include "colorconv.h"

static const struct {
    int val;
    const char *name;
} ranges[] = {
    {NMD_COL_RNG_LIMITED, "limited range"},
    {NMD_COL_RNG_FULL,    "full range"},
};

static const struct {
    int val;
    const char *name;
} spaces[] = {
    {NMD_COL_SPC_BT470BG,    "bt601"},
    {NMD_COL_SPC_BT709,      "bt709"},
    {NMD_COL_SPC_BT2020_NCL, "bt2020"},
};

static const float expected_colormatrices[2][3][4 * 4]= {
    [0] = {
        [0] = {
                       85.f/73.f,                85.f/73.f,            85.f/73.f, 0.f,
                             0.f,     -1287801.f/3287200.f,      22593.f/11200.f, 0.f,
                 35751.f/22400.f,   -10689549.f/13148800.f,                  0.f, 0.f,
              -167519.f/191625.f,   59804057.f/112483875.f,   -208034.f/191625.f, 1.f,
        },
        [1] = {
                       85.f/73.f,                85.f/73.f,            85.f/73.f, 0.f,
                             0.f,  -28469543.f/133504000.f,    236589.f/112000.f, 0.f,
               200787.f/112000.f,  -71145527.f/133504000.f,                  0.f, 0.f,
              -932203.f/958125.f,   34431883.f/114208500.f,  -1085941.f/958125.f, 1.f,
        },
        [2] = {
                       85.f/73.f,                85.f/73.f,            85.f/73.f, 0.f,
                             0.f,  -94831967.f/506240000.f,    479757.f/224000.f, 0.f,
               376023.f/224000.f, -329270807.f/506240000.f,                  0.f, 0.f,
            -1754687.f/1916250.f,  250791201.f/721787500.f, -2200133.f/1916250.f, 1.f,
        },
    },
    [1] = {
        [0] = {
                             1.f,                      1.f,                  1.f, 0.f,
                             0.f,         -25251.f/73375.f,          443.f/250.f, 0.f,
                     701.f/500.f,       -209599.f/293500.f,                  0.f, 0.f,
                -22432.f/31875.f,     9939296.f/18710625.f,     -28352.f/31875.f, 1.f,
        },
        [1] = {
                             1.f,                      1.f,                  1.f, 0.f,
                             0.f,     -1674679.f/8940000.f,        4639.f/2500.f, 0.f,
                   3937.f/2500.f,     -4185031.f/8940000.f,                  0.f, 0.f,
              -125984.f/159375.f,     4687768.f/14248125.f,   -148448.f/159375.f, 1.f,
        },
        [2] = {
                             1.f,                      1.f,                  1.f, 0.f,
                             0.f,    -5578351.f/33900000.f,        9407.f/5000.f, 0.f,
                   7373.f/5000.f,   -19368871.f/33900000.f,                  0.f, 0.f,
              -117968.f/159375.f,   99788888.f/270140625.f,   -150512.f/159375.f, 1.f,
        },
    },
};

static int compare_matrices(const float *a, const float *b)
{
    int fail = 0;
    float diff[4 * 4];
    for (int i = 0; i < NGLI_ARRAY_NB(diff); i++) {
        diff[i] = fabsf(a[i] - b[i]);
        fail += diff[i] > 1e-6;
    }
    printf("diff:\n" NGLI_FMT_MAT4 "\n\n", NGLI_ARG_MAT4(diff));
    return fail ? -fail : 0;
}

int main(void)
{
    int fail = 0;
    float mat[4 * 4];
    struct color_info cinfo = NGLI_COLOR_INFO_DEFAULTS;

    for (int r = 0; r < NGLI_ARRAY_NB(ranges); r++) {
        cinfo.range = ranges[r].val;
        for (int s = 0; s < NGLI_ARRAY_NB(spaces); s++) {
            cinfo.space = spaces[s].val;
            if (ngli_colorconv_get_ycbcr_to_rgb_color_matrix(mat, &cinfo, 1.f) < 0)
                return 1;
            printf("%s %s:\n" NGLI_FMT_MAT4 "\n\n", spaces[s].name, ranges[r].name, NGLI_ARG_MAT4(mat));
            if (compare_matrices(mat, expected_colormatrices[r][s]) < 0) {
                printf(">>>> DIFF IS TOO HIGH <<<<\n\n");
                fail++;
            }
        }
    }
    return fail;
}
