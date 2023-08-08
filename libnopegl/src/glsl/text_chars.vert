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

const vec2 uvs[] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0));

void main()
{
    vec2 ref_uv = uvs[ngl_vertex_index];

    /* y-flip UV for the texture sampling */
    uv = vec2(ref_uv.x, 1.0 - ref_uv.y);

    /*
     * These are the normalized top-left and bottom-right coordinates of the
     * character in the atlas.
     */
    coords = atlas_coords;

    vec4 position = user_transform * transform * vec4(ref_uv, 1.0, 1.0);
    ngl_out_pos = projection_matrix * modelview_matrix * position;

    color   = frag_color;
}
