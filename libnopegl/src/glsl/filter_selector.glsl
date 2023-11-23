/*
 * Copyright 2024 Clément Bœsch <u pkh.me>
 * Copyright 2024 Nope Forge
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

vec4 filter_selector(vec4 color, vec2 coords, vec2 range, int component, int drop_mode, int output_mode, int smoothedges)
{
    vec3 rgb = ngli_srgb2linear(color.rgb);
    vec3 lch = ngli_linear2oklch(rgb);
    float value = lch[component];

    bool within; // whether we are within the range or not
    if (component == 2) {
        // If we selected the hue, bring both value and reference in the same [0;𝜏] range
        range = mod(range, ngli_tau);
        value = mod(value, ngli_tau);
        if (range.x > range.y) {
            // The hue is circular, so sometimes happens across boundaries
            within = value >= range.x || value <= range.y;
        } else {
            within = value >= range.x && value <= range.y;
        }
    } else {
        within = value >= range.x && value <= range.y;
    }
    float alpha = bool(drop_mode) ^^ within ? 1.0 : 0.0;
    if (smoothedges != 0)
        alpha = ngli_aa(alpha);
    return output_mode == 0 ? color * alpha : vec4(alpha);
}
