/*
 * Copyright 2022 GoPro Inc.
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

#include hdr.glsl
#include helper_misc_utils.glsl

/* ITU-R BT.2100 */
const float pq_m1 = 0.1593017578125;
const float pq_m2 = 78.84375;
const float pq_c1 = 0.8359375;
const float pq_c2 = 18.8515625;
const float pq_c3 = 18.6875;

/* PQ Reference EOTF (linearize: R'G'B' HDR → RGB HDR), ITU-R BT.2100 */
vec3 pq_eotf3(vec3 x)
{
    vec3 p = pow(x, vec3(1.0 / pq_m2));
    vec3 num = max(p - pq_c1, 0.0);
    vec3 den = pq_c2 - pq_c3 * p;
    vec3 Y = pow(num / den, vec3(1.0 / pq_m1));
    return 10000.0 * Y;
}

float pq_eotf(float x)
{
    return pq_eotf3(vec3(x)).x;
}

/* PQ Reference OETF (EOTF¯¹), ITU-R BT.2100 */
float pq_oetf(float x)
{
    float Y = x / 10000.0;
    float Ym = pow(Y, pq_m1);
    return pow((pq_c1 + pq_c2 * Ym) / (1.0 + pq_c3 * Ym), pq_m2);
}

/*
 * Entire PQ encoding luminance range. Could be refined if mastering display
 * Lb/Lw are known.
 */
const float Lb = 0.0;       /* minimum black luminance */
const float Lw = 10000.0;   /* peak white luminance */

/*
 * Target HLG luminance range.
 */
const float Lmin = 0.0;
const float Lmax = 1000.0;

/* EETF (non-linear PQ signal → non-linear PQ signal), ITU-R BT.2408-5 annex 5 */
float pq_eetf(float x)
{
    /* Step 1 */
    float v_min = pq_oetf(Lb);
    float v_max = pq_oetf(Lw);
    float e1 = ngli_linear(v_min, v_max, x);

    float l_min = pq_oetf(Lmin);
    float l_max = pq_oetf(Lmax);
    float min_lum = ngli_linear(v_min, v_max, l_min);
    float max_lum = ngli_linear(v_min, v_max, l_max);

    /* Step 2 */
    float ks = 1.5 * max_lum - 0.5; /* knee start (roll off beginning) */
    float b = min_lum;

    /* Step 4: Hermite spline P(t) */
    float t = ngli_linear(ks, 1.0, e1);
    float t2 = t * t;
    float t3 = t2 * t;
    float p = (2.0 * t3 - 3.0 * t2 + 1.0) * ks
            + (t3 - 2.0 * t2 + t) * (1.0 - ks)
            + (-2.0 * t3 + 3.0 * t2) * max_lum;

    /* Step 3: solve for the EETF (e3) with given end points */
    float e2 = mix(p, e1, step(e1, ks));

    /*
     * Step 4: the following step is supposed to be defined for 0 ≤E₂≤ 1 but no
     * alternative outside is given, so assuming we need to clamp
     */
    e2 = ngli_sat(e2);
    float e3 = e2 + b * pow(1.0 - e2, 4.0);

    /*
     * Step 5: invert the normalization of the PQ values based on the mastering
     * display black and white luminances, Lb and Lw, to obtain the target
     * display PQ values.
     */
    float e4 = mix(v_min, v_max, e3);
    return e4;
}

void main()
{
    vec4 hdr = ngl_texvideo(tex, var_tex_coord);

    /* Linearize the PQ signal and ensure it is in the [0; 10000] range */
    vec3 rgb_linear = pq_eotf3(hdr.rgb);
    rgb_linear = clamp(rgb_linear, 0.0, 10000.0);

    /*
     * Apply the EETF with the maxRGB method to map the PQ signal with a peak
     * luminance of 10000 cd/m² to 1000 cd/m² (HLG), ITU-R BT.2408-5 annex 5
     */
    float m1 = max(rgb_linear.r, max(rgb_linear.g, rgb_linear.b));
    float m2 = pq_eotf(pq_eetf(pq_oetf(m1)));
    rgb_linear *= m2 / m1;

    /* Rescale the PQ signal so [0, 1000] maps to [0, 1] */
    rgb_linear /= 1000.0;

    vec3 sdr = bt2020_to_bt709(tonemap(rgb_linear));
    ngl_out_color = vec4(sdr, hdr.a);
}
