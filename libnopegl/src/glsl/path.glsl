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

float border(float d, float blur)
{
    return blur > 0.0 ? ngli_linearstep(-blur/2.0, blur/2.0, d) : ngli_aa(d);
}

/*
 * dist: distance to the shape and the outline (negative outside, positive inside)
 * color: RGB color, opacity stored in the alpha channel (not premultiplied)
 * outline: RGB color for the outline, width stored in the alpha channel
 * glow: RGB color for the glow, intensity stored in the alpha channel
 * blur: blur amount
 */
vec4 get_path_color(vec2 dist, vec4 color, vec4 outline, vec4 glow, float blur)
{
    float opacity = color.a; // overall opacity
    float outline_width = outline.a;

    float fill_d = dist.x;
    float stroke_d = outline_width - abs(dist.y);

    float d = max(fill_d, stroke_d);
    float a = border(d, blur);

    // Transition between the stroke outline and the fill in color
    vec3 out_color3 = color.rgb;
    if (outline_width != 0.0)
        out_color3 = mix(out_color3, outline.rgb, border(stroke_d, blur));

    vec4 out_color = vec4(out_color3, 1.0) * opacity * a;

    if (glow.a > 0.0) {
        // TODO need to honor blur
        float glow_a = pow(1.0 - ngli_sat(abs(d)), 1.0 / glow.a);
        out_color += vec4(glow.rgb, 1.0) * glow_a;
    }

    return out_color;
}
