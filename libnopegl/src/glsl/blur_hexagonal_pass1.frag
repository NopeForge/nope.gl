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

const vec2 up = vec2(0.0, 1.0); // cos(PI/2), sin(PI/2)
const vec2 down_left = vec2(0.8660254037844387, -0.5); // cos(-PI/6), sin(-PI/6)

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

    vec4 color = ngli_blur_hexagonal(tex, tex_coord, map, map_coord, up, scale, lod, nb_samples);
    vec4 color2 = ngli_blur_hexagonal(tex, tex_coord, map, map_coord, down_left, scale, lod, nb_samples);

    ngl_out_color[0] = vec4(ngli_linear2srgb(color.rgb), color.a);
    ngl_out_color[1] = vec4(ngli_linear2srgb(color.rgb + color2.rgb), color.a + color2.a);
}
