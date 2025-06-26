/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Forge
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

#include helper_misc_utils.glsl
#include helper_srgb.glsl

/*
 * dist: distance to the shape (negative outside, positive inside)
 * color: RGB color, opacity stored in the alpha channel (not premultiplied)
 * outline: RGB color for the outline, width stored in the alpha channel
 * glow: RGB color for the glow, intensity stored in the alpha channel
 * blur: blur amount
 */
vec4 get_path_color(float dist, vec4 color, vec4 outline, vec4 glow, float blur, float outline_pos)
{
    float aa = fwidth(dist); // pixel width estimates
    float w = max(aa, blur) * 0.5; // half diffuse width
    vec2 d = dist + mix(vec2(-1,0), vec2(0,1), ngli_sat(outline_pos)) * outline.a; // inner and outer boundaries
    float inner_mask = smoothstep(-w, w, d.x); // cut off between the outline and the outside (whole shape w/ outline)
    float outer_mask = smoothstep(-w, w, d.y); // cut off between the fill color and the outline (whole shape w/o outline)
    float outline_mask = outer_mask - inner_mask;
    vec3 shape_col = ngli_srgb2linear(color.rgb)*inner_mask + ngli_srgb2linear(outline.rgb)*outline_mask;
    vec4 out_color = vec4(shape_col, 1.0) * outer_mask;

    // TODO need to honor blur
    float glow_power = glow.a * exp(min(d.y, 0.0) * 10.0);
    out_color += vec4(ngli_srgb2linear(glow.rgb), 1.0) * glow_power;

    return vec4(ngli_linear2srgb(out_color.rgb), out_color.a) * color.a;
}
