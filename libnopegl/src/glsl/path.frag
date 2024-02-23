/*
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

#include path.glsl

void main()
{
    /*
     * uv_* are normalized [0,1] quad coordinate which we map to the atlas
     * element coordinate boundaries
     */
    vec2 uv_fill    = mix(coords_fill.xy, coords_fill.zw, uv);
    vec2 uv_outline = mix(coords_outline.xy, coords_outline.zw, uv);

    /*
     * The half texel clamping is here to prevent texture bleeding when the
     * texture has bilinear filtering: we're preventing the filter from reading
     * the neighbour atlas elements. The small delta is here to prevent the 0.5
     * ambiguity and float inaccuracies. It could become an issue only in the
     * case of a huge atlas.
     */
    vec2 half_texel = 0.5 / vec2(textureSize(tex, 0)) + 1e-8;
    vec2 clamp_uv_fill    = clamp(uv_fill,    coords_fill.xy    + half_texel, coords_fill.zw    - half_texel);
    vec2 clamp_uv_outline = clamp(uv_outline, coords_outline.xy + half_texel, coords_outline.zw - half_texel);

    vec2 dist = vec2(
        texture(tex, clamp_uv_fill).r,
        texture(tex, clamp_uv_outline).r
    );
    ngl_out_color = get_path_color(dist, vec4(color, opacity), vec4(outline_color, outline), vec4(glow_color, glow), blur);
}
