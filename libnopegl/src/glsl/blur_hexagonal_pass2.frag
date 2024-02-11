/*
 * Copyright 2023-2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023-2024 Nope Forge
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

#include helper_srgb.glsl
#include helper_blur.glsl

const vec2 down_left = vec2(0.8660254037844387, -0.5); // cos(-PI/6), sin(-PI/6)
const vec2 down_right = vec2(-0.8660254037844387, -0.5); // cos(-5*PI/6), sin(-5*PI/6)

/*
 * Hexagonal blur pass 2 (down-left, down-right + combine)
 * Adapted from WHITE, John, and BARRÉ-BRISEBOIS, Colin. More Performance! Five
 * Rendering Ideas From Battlefield 3 and Need For Speed: The Run, Advances in
 * Real-Time Rendering in Games, SIGGRAPH 2011:
 * https://www.slideshare.net/DICEStudio/five-rendering-ideas-from-battlefield-3-need-for-speed-the-run
 */
void main()
{
    float coc = texture(map, map_coord).r;
    int radius = int(float(blur.radius) * coc);

    float lod = 0.0;
    float scale = 1.0;
    int nb_samples = max(radius, 1);
    if (radius > blur.nb_samples) {
        scale = float(radius) / float(blur.nb_samples);
        nb_samples = blur.nb_samples;
        lod = log(scale);
    }

    vec4 color = ngli_blur_hexagonal(tex0, tex_coord, map, map_coord, down_left, scale, lod, nb_samples);
    vec4 color2 = ngli_blur_hexagonal(tex1, tex_coord, map, map_coord, down_right, scale, lod, nb_samples);

    vec4 out_color = mix(color, color2, 0.5);
    ngl_out_color = vec4(ngli_linear2srgb(out_color.rgb), out_color.a);
}
