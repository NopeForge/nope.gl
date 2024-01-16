/*
 * Copyright 2024 Clément Bœsch <u pkh.me>
 * Copyright 2024 Nope Forge
 * Copyright 2020 Björn Ottosson
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

const mat3 oklab_rgb2lms = mat3(
    0.4122214708, 0.2119034982, 0.0883024619,
    0.5363325363, 0.6806995451, 0.2817188376,
    0.0514459929, 0.1073969566, 0.6299787005
);

const mat3 oklab_lms2lab = mat3(
    0.2104542553,  1.9779984951,  0.0259040371,
    0.7936177850, -2.4285922050,  0.7827717662,
   -0.0040720468,  0.4505937099, -0.8086757660
);

vec3 ngli_linear2oklab(vec3 c)
{
    vec3 lms = oklab_rgb2lms * c;
    vec3 lms_ = pow(lms, vec3(1.0/3.0));
    return oklab_lms2lab * lms_;
}

vec3 ngli_linear2oklch(vec3 c)
{
    vec3 lab = ngli_linear2oklab(c);
    return vec3(lab.x, length(lab.yz), atan(lab.z, lab.y));
}
