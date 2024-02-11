/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

vec4 ngli_blur_hexagonal(sampler2D tex, vec2 tex_coord, sampler2D map, vec2 map_coord, vec2 direction, float scale, float lod, int nb_samples)
{
    nb_samples = max(nb_samples, 1);

    float use_coc = nb_samples > 1 ? 1.0 : 0.0;
    float offset = 0.5 * use_coc;

    vec2 tex_step = 1.0 / vec2(textureSize(tex, 0).xy);
    vec2 tex_direction = tex_step * direction;

    tex_coord += tex_direction * offset;
    tex_direction *= scale;

    vec4 color = vec4(0.0);
    float amount = 0.0;
    for (int i = 0; i < nb_samples; i++) {
        vec2 coord_offset = float(i) * tex_direction;
        vec4 value = textureLod(tex, tex_coord + coord_offset, lod);
        float coc = texture(map, map_coord + coord_offset).r;
        coc = mix(1.0, coc, use_coc);
        color += vec4(ngli_srgb2linear(value.rgb), value.a) * coc;
        amount += coc;
    }
    return color / amount;
}
