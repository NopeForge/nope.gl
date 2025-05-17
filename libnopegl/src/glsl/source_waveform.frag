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

vec3 color_from_amplitude(float x, vec3 comp)
{
    float y = 1.0 - pow(1.0 - x, 6.0);  // arbitrarily boosted curve for the tinted component
    return mix(vec3(x), vec3(y), comp); // select either boosted curve (y) or linear curve (x)
}

vec3 wavecolor()
{
    float x = mode == 1 ? mod(uv.x, 1.0 / 3.0) * 3.0 : uv.x;
    float y = uv.y;

    /* data buffer has depth horizontally and the image spatiality vertically */
    vec2 pos = vec2(y, x) * vec2(stats.depth - 1U, stats.length_minus1);
    uint spatial_pos = clamp(uint(pos.y), 0U, stats.length_minus1);
    uint data_offset = spatial_pos * stats.depth;

    /* vertical smoothing on the y-axis (depth) */
    uvec2 depth_pos = clamp(uvec2(pos.x, pos.x + 1.0), 0U, stats.depth - 1U);
    vec4 c0 = vec4(stats.data[data_offset + depth_pos.x]);
    vec4 c1 = vec4(stats.data[data_offset + depth_pos.y]);
    vec4 scale = 1.0 / vec4(vec3(stats.max_rgb.x), stats.max_luma.x);
    vec4 c = mix(c0, c1, fract(pos.x)) * scale;

    if (mode == 0) { /* mixed */
        vec3 color_r = color_from_amplitude(c.r, vec3(1.0, 0.0, 0.0));
        vec3 color_g = color_from_amplitude(c.g, vec3(0.0, 1.0, 0.0));
        vec3 color_b = color_from_amplitude(c.b, vec3(0.0, 0.0, 1.0));
        return color_r + color_g + color_b;
    } else if (mode == 1) { /* parade */
        float color_comp = min(floor(uv.x * 3.0), 2.0);
        /* (1,0,0), (0,1,0) or (0,0,1) according to current color component */
        vec3 filt = 1.0 - abs(sign(color_comp - vec3(0.0, 1.0, 2.0)));

        float amp = dot(c.rgb * filt, vec3(1.0)); // extract current component amplitude
        return color_from_amplitude(amp, filt);
    } else { /* luma only */
        return color_from_amplitude(c.w, vec3(1.0, 1.0, 1.0));
    }
}

vec4 source_waveform()
{
    vec3 c = pow(wavecolor(), vec3(1.0 / 2.2)); // cheap sRGB encode
    return vec4(c, 1.0);
}
