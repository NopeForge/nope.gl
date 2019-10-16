/*
 * Copyright 2019 GoPro Inc.
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

#include "colorconv.h"

/* BT.709 (limited range) */
static const NGLI_ALIGNED_MAT(ycrb_to_rgb_bt709) = {
    1.164,     1.164,    1.164,   0.0,
    0.0,      -0.213,    2.112,   0.0,
    1.787,    -0.531,    0.0,     0.0,
   -0.96625,   0.29925, -1.12875, 1.0
};

int ngli_colorconv_get_ycbcr_to_rgb_color_matrix(float *dst, const struct color_info *info)
{
    memcpy(dst, ycrb_to_rgb_bt709, sizeof(ycrb_to_rgb_bt709));
    return 0;
}
