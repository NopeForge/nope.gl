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

vec3 ngli_srgb2linear(vec3 color)
{
    return mix(
        color / 12.92,
        pow((max(color, 0.0) + .055) / 1.055, vec3(2.4)),
        step(vec3(0.04045), color)
    );
}

vec3 ngli_linear2srgb(vec3 color)
{
    return mix(
        color * 12.92,
        1.055 * pow(max(color, 0.0), vec3(1. / 2.4)) - .055,
        step(vec3(0.0031308), color)
    );
}

vec3 ngli_srgbmix(vec3 c0, vec3 c1, float v)
{
    vec3 rgb = mix(ngli_srgb2linear(c0), ngli_srgb2linear(c1), v);
    return ngli_linear2srgb(rgb);
}

vec4 ngli_srgbmix(vec4 c0, vec4 c1, float v)
{
    vec4 rgba = mix(
        vec4(ngli_srgb2linear(c0.rgb), c0.a),
        vec4(ngli_srgb2linear(c1.rgb), c1.a),
    v);
    return vec4(ngli_linear2srgb(rgba.rgb), rgba.a);
}
