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

#define _gradient4(tl, tr, br, bl, uv) mix(mix(tl, tr, uv.x), mix(bl, br, uv.x), uv.y)

vec4 source_gradient4()
{
    vec3 tl = color_tl * opacity_tl;
    vec3 tr = color_tr * opacity_tr;
    vec3 br = color_br * opacity_br;
    vec3 bl = color_bl * opacity_bl;
    float alpha = _gradient4(opacity_tl, opacity_tr, opacity_br, opacity_bl, uv);
    if (linear)
        return vec4(
            ngli_linear2srgb(
                _gradient4(
                    ngli_srgb2linear(tl),
                    ngli_srgb2linear(tr),
                    ngli_srgb2linear(br),
                    ngli_srgb2linear(bl), uv)),
            alpha);
    return vec4(_gradient4(tl, tr, br, bl, uv), alpha);
}
