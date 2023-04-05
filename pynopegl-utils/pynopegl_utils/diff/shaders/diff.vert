/*
 * Copyright 2022 GoPro Inc.
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
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    uv = ngl_uvcoord;

    /* Re-center space from UV in [0;1] to geometric [-1;1] (+ y-axis flip) */
    vec2 st = vec2(uv.x, 1.0 - uv.y) * 2.0 - 1.0;

    /*
     * Apply reframing; we need to inverse the reframing because we are
     * enlarging the coordinate space but still fitting it into the same
     * geometry.
     */
    float scale = 1.0 / reframing_scale;
    vec2 off = -reframing_off * scale;
    st = st * scale + off;

    /* Restore space to [0;1] (+ y-axis flip) */
    st = vec2(st.x + 1.0, 1.0 - st.y) / 2.0;

    tex0_coord = (tex0_coord_matrix * vec4(st, 0.0, 1.0)).xy;
    tex1_coord = (tex1_coord_matrix * vec4(st, 0.0, 1.0)).xy;
}
