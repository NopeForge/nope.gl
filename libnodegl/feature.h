/*
 * Copyright 2016 GoPro Inc.
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

#ifndef FEATURE_H
#define FEATURE_H

#define NGLI_FEATURE_COMPUTE_SHADER_ALL                (1 << 0)
#define NGLI_FEATURE_INSTANCED_DRAW                    (1 << 1)
#define NGLI_FEATURE_FRAMEBUFFER_OBJECT                (1 << 2)
#define NGLI_FEATURE_SHADER_TEXTURE_LOD                (1 << 3)
#define NGLI_FEATURE_SOFTWARE                          (1 << 4)
#define NGLI_FEATURE_TEXTURE_3D                        (1 << 5)
#define NGLI_FEATURE_TEXTURE_CUBE_MAP                  (1 << 6)
#define NGLI_FEATURE_TEXTURE_NPOT                      (1 << 7)
#define NGLI_FEATURE_UINT_UNIFORMS                     (1 << 8)
#define NGLI_FEATURE_UNIFORM_BUFFER_OBJECT             (1 << 9)
#define NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT      (1 << 10)

#endif
