/*
 * Copyright 2022 GoPro Inc.
 * Copyright 2022 Clément Bœsch <u pkh.me>
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

vec4 source_gradient()
{
    vec3 c0 = color0.rgb * opacity0;
    vec3 c1 = color1.rgb * opacity1;

    float t = 0.0;
    if (mode == 0) { /* ramp */
        vec2 pa = uv - pos0;
        vec2 ba = pos1 - pos0;
        pa.x *= aspect;
        ba.x *= aspect;
        t = dot(pa, ba) / dot(ba, ba);
    } else if (mode == 1) { /* radial */
        vec2 pa = uv - pos0;
        vec2 pb = uv - pos1;
        pa.x *= aspect;
        pb.x *= aspect;
        float len_pa = length(pa);
        t = len_pa / (len_pa + length(pb));
    }

    float a = mix(opacity0, opacity1, t);
    if (linear)
        return vec4(ngli_linear2srgb(mix(ngli_srgb2linear(c0), ngli_srgb2linear(c1), t)), a);
    return vec4(mix(c0, c1, t), a);
}
