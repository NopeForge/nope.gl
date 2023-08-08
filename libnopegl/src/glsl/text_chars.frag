/*
 * Copyright 2023 Nope Project
 * Copyright 2019-2022 GoPro Inc.
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

void main()
{
    /*
     * uv is a normalized [0;1] quad coordinate which we map to the atlas
     * element coordinate boundaries
     */
    vec2 chr_uv = mix(coords.xy, coords.zw, uv);

    /*
     * The half texel clamping is here to prevent texture bleeding when the
     * texture has bilinear filtering: we're preventing the filter from reading
     * the neighbour atlas elements. The small delta is here to prevent the 0.5
     * ambiguity and float inaccuracies. It could become an issue only in the
     * case of a huge atlas.
     */
    vec2 half_texel = 0.5 / vec2(textureSize(tex, 0)) + 1e-8;
    vec2 clamp_uv = clamp(chr_uv, coords.xy + half_texel, coords.zw - half_texel);

    float v = texture(tex, clamp_uv).r;
    float opacity = color.a;
    ngl_out_color = vec4(color.rgb, 1.0) * opacity * v;
}
