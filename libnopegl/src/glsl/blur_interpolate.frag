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
    highp vec4 c0 = texture(tex0, tex_coord);
    c0 = vec4(ngli_srgb2linear(c0.rgb), c0.a);
    highp vec4 c1 = texture(tex1, tex_coord);
    c1 = vec4(ngli_srgb2linear(c1.rgb), c1.a);
    vec4 color = mix(c0, c1, interpolate.lod);
    ngl_out_color = vec4(ngli_linear2srgb(color.rgb), color.a);
}
