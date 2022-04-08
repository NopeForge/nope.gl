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

const vec3 cmap0 = vec3(0.0, 0.5, 1.0);
const vec3 cmap1 = vec3(1.0, 0.5, 0.0);
const vec3 luma_weights = vec3(.2126, .7152, .0722); // BT.709

vec3 lin2srgb(vec3 c) { return mix(c * 12.92, 1.055 * pow(c, vec3(1. / 2.4)) - .055, step(vec3(0.0031308), c)); }
vec3 srgb2lin(vec3 c) { return mix(c / 12.92, pow((c + .055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }
vec3 lin_mix(vec3 c0, vec3 c1, float v) { return lin2srgb(mix(srgb2lin(c0), srgb2lin(c1), v)); }

vec4 color_diff(vec4 c0, vec4 c1)
{
    float n = float(show_r) + float(show_g) + float(show_b) + float(show_a);
    vec4 diff = c0 - c1;
    vec4 diff2 = diff * diff * vec4(show_r, show_g, show_b, show_a); // filtered squared difference
    float sum = diff2.r + diff2.g + diff2.b + diff2.a;
    float err = sqrt(sum / n);
    float amp = (err - threshold) / (1 - threshold); // remap err from [0;1] to [thres;1]
    vec3 grad = lin_mix(cmap0, cmap1, pow(amp, 1.0 / 3.0)); // power 1/3 to boost the differences of the small amplitude

    /* Apply gradient only if there is an actual mismatch (honoring threshold) */
    return vec4(grad * float(err > threshold), 1.0);
}

vec4 color_split(vec4 c0, vec4 c1)
{
    /* Checkerboard (as a background layer in case there is no alpha) */
    vec2 chkf = floor(uv * ngl_resolution / 10.0);
    float chk = mod(chkf.x + chkf.y, 2.0);
    vec4 dst = vec4(mix(vec3(1.0 / 3.0), vec3(2.0 / 3.0), chk), 1.0);

    /* Apply channel masking */
    vec3 mask = vec3(show_r, show_g, show_b);
    c0.rgb *= mask;
    c1.rgb *= mask;
    if (!show_a) {
        /*
         * If alpha needs to be ignored, we will force it to 1 instead of 0
         * like other color channels. This avoids unexpected behaviour such has
         * having the checkerboard showing up when we decide to ignore the
         * alpha channel.
         */
        c0.a = 1.0;
        c1.a = 1.0;
    }

    /* Final blending assumes premultiplied */
    if (!premultiplied) {
        c0 *= c0.a;
        c1 *= c1.a;
    }

    /* Select color according to split side */
    vec2 splitdiff = uv - split;
    float splitdist = vertical_split ? splitdiff.x : splitdiff.y;
    vec4 color = mix(c0, c1, step(0.0, splitdist));

    /*
     * Split line: we get the average lightness (in linear space) between the 2
     * colors and invert it in order to increase our chances to get a visible
     * line whatever the colors.
     */
    vec3 avg_color = (srgb2lin(c0.rgb) + srgb2lin(c1.rgb)) / 2.0;
    float avg_light = dot(luma_weights, avg_color);
    vec3 split_color = vec3(step(avg_light - 0.5, 0.0));

    /* Mix the split line with our color, in linear space */
    float res = vertical_split ? ngl_resolution.x : ngl_resolution.y;
    float split_line = max(1.0 - abs(splitdist * res), 0.0);
    color.rgb = mix(srgb2lin(color.rgb), split_color, split_line);

    /* Simple source-over blending */
    color = dst * (1.0 - color.a) + color;
    return vec4(lin2srgb(color.rgb), color.a);
}

void main()
{
    vec4 c0 = ngl_texvideo(tex0, tex0_coord);
    vec4 c1 = ngl_texvideo(tex1, tex1_coord);
    ngl_out_color = diff_mode ? color_diff(c0, c1) : color_split(c0, c1);
}
