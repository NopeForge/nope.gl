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

#ifndef FEATURE_GL_H
#define FEATURE_GL_H

#define NGLI_FEATURE_GL_TEXTURE_STORAGE                            (1ULL << 2)
#define NGLI_FEATURE_GL_COMPUTE_SHADER                             (1ULL << 3)
#define NGLI_FEATURE_GL_PROGRAM_INTERFACE_QUERY                    (1ULL << 4)
#define NGLI_FEATURE_GL_SHADER_IMAGE_LOAD_STORE                    (1ULL << 5)
#define NGLI_FEATURE_GL_SHADER_STORAGE_BUFFER_OBJECT               (1ULL << 6)
#define NGLI_FEATURE_GL_INTERNALFORMAT_QUERY                       (1ULL << 8)
#define NGLI_FEATURE_GL_TIMER_QUERY                                (1ULL << 10)
#define NGLI_FEATURE_GL_EXT_DISJOINT_TIMER_QUERY                   (1ULL << 11)
#define NGLI_FEATURE_GL_INVALIDATE_SUBDATA                         (1ULL << 15)
#define NGLI_FEATURE_GL_OES_EGL_EXTERNAL_IMAGE                     (1ULL << 16)
#define NGLI_FEATURE_GL_OES_EGL_IMAGE                              (1ULL << 19)
#define NGLI_FEATURE_GL_EGL_IMAGE_BASE_KHR                         (1ULL << 20)
#define NGLI_FEATURE_GL_EGL_EXT_IMAGE_DMA_BUF_IMPORT               (1ULL << 21)
#define NGLI_FEATURE_GL_YUV_TARGET                                 (1ULL << 23)
#define NGLI_FEATURE_GL_SOFTWARE                                   (1ULL << 28)
#define NGLI_FEATURE_GL_EGL_ANDROID_GET_IMAGE_NATIVE_CLIENT_BUFFER (1ULL << 30)
#define NGLI_FEATURE_GL_KHR_DEBUG                                  (1ULL << 31)
#define NGLI_FEATURE_GL_SHADER_IMAGE_SIZE                          (1ULL << 33)
#define NGLI_FEATURE_GL_SHADING_LANGUAGE_420PACK                   (1ULL << 34)
#define NGLI_FEATURE_GL_COLOR_BUFFER_FLOAT                         (1ULL << 36)
#define NGLI_FEATURE_GL_COLOR_BUFFER_HALF_FLOAT                    (1ULL << 37)
#define NGLI_FEATURE_GL_BUFFER_STORAGE                             (1ULL << 39)
#define NGLI_FEATURE_GL_EXT_BUFFER_STORAGE                         (1ULL << 40)
#define NGLI_FEATURE_GL_EGL_MESA_QUERY_DRIVER                      (1ULL << 41)
#define NGLI_FEATURE_GL_TEXTURE_NORM16                             (1ULL << 42)
#define NGLI_FEATURE_GL_TEXTURE_FLOAT_LINEAR                       (1ULL << 43)
#define NGLI_FEATURE_GL_FLOAT_BLEND                                (1ULL << 44)

#define NGLI_FEATURE_GL_COMPUTE_SHADER_ALL (NGLI_FEATURE_GL_COMPUTE_SHADER           | \
                                            NGLI_FEATURE_GL_PROGRAM_INTERFACE_QUERY  | \
                                            NGLI_FEATURE_GL_SHADER_IMAGE_LOAD_STORE  | \
                                            NGLI_FEATURE_GL_SHADER_IMAGE_SIZE        | \
                                            NGLI_FEATURE_GL_SHADER_STORAGE_BUFFER_OBJECT)

#endif
