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

#define ngli_sat(x) clamp(x, 0.0, 1.0)
#define ngli_linear(a, b, x) (((x) - (a)) / ((b) - (a)))

const vec3 ngli_luma_weights = vec3(.2126, .7152, .0722); // BT.709
const float ngli_pi = 3.14159265358979323846;
