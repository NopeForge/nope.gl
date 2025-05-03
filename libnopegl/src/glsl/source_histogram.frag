/*
 * Copyright 2022-2023 Clément Bœsch <u pkh.me>
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

float dist_segment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

/* Build the 3 coordinates for Outline */
float distance_to_closest_segment(vec3 x3, vec3 y3)
{
    vec2 p0 = vec2(x3.x, y3.x);
    vec2 p1 = vec2(x3.y, y3.y);
    vec2 p2 = vec2(x3.z, y3.z);
    float d0 = dist_segment(uv, p0, p1);
    float d1 = dist_segment(uv, p1, p2);
    return min(d0, d1);
}

float vertical_line(float x, float y, vec3 amp3)
{
    int max_depth = int(stats.depth) - 1;
    float bin_pos = x * float(max_depth);
    int bin_mid = int(round(bin_pos));
    ivec2 bin = clamp(ivec2(floor(bin_pos), floor(bin_pos) + 1.0), 0, max_depth); // find surrounding bins
    vec2 amps = bin_mid == bin.x ? vec2(amp3.y, amp3.z) : vec2(amp3.x, amp3.y); // pick left or right rounded segment
    float amp = mix(amps.x, amps.y, fract(bin_pos)); // interpolate an amplitude between the amplitudes of the 2 bins
    return step(y, amp) * (1.0 - step(y, 0.0)); // y <= amp && y > 0.0
}

float outline(float dist)
{
    float r = 1.0 / (float(stats.depth) * 2.0); // maximum thickness (as radius) of the outline (for 2 segments)
    return max(0.5 * (1.0 - abs(dist / r)), 0.0); // triangle around 0
}

vec3 color_from_amplitude(vec3 amp3, vec3 color_comp, vec3 x3)
{
    float v_line = vertical_line(uv.x, uv.y, amp3);
    vec3 color = mix(vec3(1.0/6.0), vec3(1.0/3.0), color_comp) * v_line;
    float dist = distance_to_closest_segment(x3, amp3);
    return color + outline(dist);
}

vec4 source_histogram()
{
    /*
     * Outline
     *
     *     :   :   :
     *     o   :   :
     *    /:\  :   :
     *   / : \ :X  : /
     *     :  \:   :/
     *     :   o---o
     *     :   :   :
     *   0   1   2   3
     *
     * Point X could be closest to segment 1 while being in the "column range"
     * of segment 2.
     *
     * The following code tries to identify the 2 closest segments (by picking
     * 3 points) and choose the closest one. We make sure the thickness of the
     * outline is not larger than half a step, so that we only need to evaluate
     * the distance to 2 segments.
     */

    /* Build 3 histogram bin indexes (corresponding to 2 segments) */
    int max_depth = int(stats.depth) - 1;
    int bin_mid = int(round(uv.x * float(max_depth)));
    ivec3 bin3 = clamp(bin_mid + ivec3(-1, 0, 1), 0, max_depth);
    vec3 x3 = vec3(bin3) / float(max_depth);

    vec4 scale = 1.0 / vec4(vec3(stats.max_rgb.y), stats.max_luma.y);
    vec4 amp0_rgby = vec4(stats.summary[bin3.x]) * scale;
    vec4 amp1_rgby = vec4(stats.summary[bin3.y]) * scale;
    vec4 amp2_rgby = vec4(stats.summary[bin3.z]) * scale;

    if (mode == 0) { /* mixed */
        vec3 amp_r = vec3(amp0_rgby.r, amp1_rgby.r, amp2_rgby.r);
        vec3 amp_g = vec3(amp0_rgby.g, amp1_rgby.g, amp2_rgby.g);
        vec3 amp_b = vec3(amp0_rgby.b, amp1_rgby.b, amp2_rgby.b);

        vec3 color_r = color_from_amplitude(amp_r, vec3(1.0, 0.0, 0.0), x3);
        vec3 color_g = color_from_amplitude(amp_g, vec3(0.0, 1.0, 0.0), x3);
        vec3 color_b = color_from_amplitude(amp_b, vec3(0.0, 0.0, 1.0), x3);

        return vec4(color_r + color_g + color_b, 1.0);
    } else if (mode == 1) { /* parade */
        float color_comp = min(floor(uv.y * 3.0), 2.0);

        /* (1,0,0), (0,1,0) or (0,0,1) according to current color component */
        vec3 filt = 1.0 - abs(sign(color_comp - vec3(2.0, 1.0, 0.0)));

        /* Extract current component amplitude */
        vec3 amp3 = vec3(
            dot(amp0_rgby.rgb * filt, vec3(1.0)),
            dot(amp1_rgby.rgb * filt, vec3(1.0)),
            dot(amp2_rgby.rgb * filt, vec3(1.0)));

        /* Color column */
        vec2 range = vec2(color_comp, color_comp + 1.0) / 3.0;
        float y_comp = (uv.y - range.s) / (range.t - range.s);
        float v_line = vertical_line(uv.x, y_comp, amp3);
        vec3 color = mix(vec3(0.25), vec3(0.50), filt) * v_line;

        vec3 y3 = (color_comp + amp3) / 3.0;
        float dist = distance_to_closest_segment(x3, y3);
        color += outline(dist);
        return vec4(color, 1.0);
    } else { /* luma only */
        vec3 y3 = vec3(amp0_rgby.w, amp1_rgby.w, amp2_rgby.w);
        float v_line = vertical_line(uv.x, uv.y, y3);
        vec3 color = vec3(0.5) * v_line;
        float dist = distance_to_closest_segment(x3, y3);
        color += outline(dist);
        return vec4(color, 1.0);
    }
}
