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

void main()
{
    vec2 size = vec2(textureSize(tex, 0));
    vec2 direction = (tex_coord - center) * size;
    vec2 step = normalize(direction) / size;
    float radius = length(direction) * amount;
    int radius_i = int(ceil(radius));

    /*
     * Use the 3-sigma rule to approximate sigma, see:
     * - https://en.wikipedia.org/wiki/Talk%3AGaussian_blur#Radius_again
     * - https://en.wikipedia.org/wiki/68%E2%80%9395%E2%80%9399.7_rule
     */
    float sigma = radius / 3.0;

    /*
     * Fast approximation of the gaussian function using the incremental gaussian
     * algorithm adapted from GPU Gem 3.
     */
    float g0 = 1.0;
    float g1 = sigma > 1e-4 ? exp(-1.0 / (2.0 * sigma * sigma)) : 0.0;
    float g2 = g1 * g1;

    vec4 color = vec4(0.0);
    float total_weight = 0.0;

    int nb_samples = max(radius_i, 2);
    for (int i = 0; i < nb_samples; i = i + 2) {
        /* Take advantage of hw filtering to reduce the number of texture fetches */
        float w1 = g0;
        g0 *= g1;
        g1 *= g2;
        float w2 = g0;
        g0 *= g1;
        g1 *= g2;
        float w = w1 + w1;
        vec2 offset = ((float(i) * w1 + (float(i) + 1.0) * w2) / w) * step;
        vec4 value = texture(tex, tex_coord - offset);
        color += vec4(ngli_srgb2linear(value.rgb), value.a) * w;
        total_weight += w;
    }
    color /= total_weight;
    ngl_out_color = vec4(ngli_linear2srgb(color.rgb), color.a);
}
