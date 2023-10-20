/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include helper_linear2srgb.glsl
#include helper_srgb2linear.glsl

void main()
{
    highp vec4 color = vec4(0.0);
    highp vec2 size = vec2(textureSize(tex, 0));
    for (int i = 0; i < kernel.nb_weights; i++) {
        highp vec2 offset = direction.direction * kernel.weights[i].x / size;
        highp float weight = kernel.weights[i].y;
        highp vec4 value = texture(tex, tex_coord + offset);
        color += vec4(ngli_srgb2linear(value.rgb), value.a) * weight;
    }
    ngl_out_color = vec4(ngli_linear2srgb(color.rgb), color.a);
}
