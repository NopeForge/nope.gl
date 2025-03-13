/*
 * Copyright 2025 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2016-2022 GoPro Inc.
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

#ifndef GLINCLUDES_H
#define GLINCLUDES_H

#include "config.h"

#ifdef __ANDROID__
# include <GLES3/gl31.h>
# include <GLES3/gl3ext.h>
#endif

#if defined(__linux__) && !defined(__ANDROID__)
# define GL_GLEXT_PROTOTYPES
# include <GL/glcorearb.h>
# include <GL/glext.h>
#endif

#ifdef _WIN32
# include <Windows.h>
# include <GL/gl.h>
# include <GL/glcorearb.h>
# include <GL/glext.h>
#endif

#ifdef _WIN32
#define NGLI_GL_APIENTRY WINAPI
#else
#define NGLI_GL_APIENTRY
#endif

#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

#if !defined(GL_VERSION_4_3) && !defined(GL_ES_VERSION_3_2)
typedef void (NGLI_GL_APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user_param);
#endif

/* Debug */
# define GL_DEBUG_OUTPUT                       0x92E0
# define GL_DEBUG_OUTPUT_SYNCHRONOUS           0x8242
# define GL_DEBUG_SOURCE_API                   0x8246
# define GL_DEBUG_SOURCE_WINDOW_SYSTEM         0x8247
# define GL_DEBUG_SOURCE_SHADER_COMPILER       0x8248
# define GL_DEBUG_SOURCE_THIRD_PARTY           0x8249
# define GL_DEBUG_SOURCE_APPLICATION           0x824A
# define GL_DEBUG_SOURCE_OTHER                 0x824B
# define GL_DEBUG_TYPE_ERROR                   0x824C
# define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR     0x824D
# define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR      0x824E
# define GL_DEBUG_TYPE_PORTABILITY             0x824F
# define GL_DEBUG_TYPE_PERFORMANCE             0x8250
# define GL_DEBUG_TYPE_OTHER                   0x8251
# define GL_DEBUG_TYPE_MARKER                  0x8268
# define GL_DEBUG_TYPE_PUSH_GROUP              0x8269
# define GL_DEBUG_TYPE_POP_GROUP               0x826A
# define GL_DEBUG_SEVERITY_HIGH                0x9146
# define GL_DEBUG_SEVERITY_MEDIUM              0x9147
# define GL_DEBUG_SEVERITY_LOW                 0x9148
# define GL_DEBUG_SEVERITY_NOTIFICATION        0x826B

/* OES external images */
# define GL_TEXTURE_EXTERNAL_OES               0x8D65

/* Desktop formats */
# define GL_STENCIL_INDEX                      0x1901
# define GL_R16                                0x822A
# define GL_RG16                               0x822C
# define GL_RGB16                              0x8054
# define GL_RGBA16                             0x805B
# define GL_R16_SNORM                          0x8F98
# define GL_RG16_SNORM                         0x8F99
# define GL_RGB16_SNORM                        0x8F9A
# define GL_RGBA16_SNORM                       0x8F9B
# define GL_BGRA                               0x80E1
# define GL_BGRA_INTEGER                       0x8D9B

/* Desktop features */
# define GL_TIMESTAMP                          0x8E28
# define GL_TEXTURE_CUBE_MAP_SEAMLESS          0x884F
# define GL_FRONT_LEFT                         0x0400

/* Buffer storage */
# define GL_MAP_PERSISTENT_BIT                 0x0040
# define GL_MAP_COHERENT_BIT                   0x0080
# define GL_DYNAMIC_STORAGE_BIT                0x0100
# define GL_CLIENT_STORAGE_BIT                 0x0200
# define GL_BUFFER_IMMUTABLE_STORAGE           0x821F
# define GL_BUFFER_STORAGE_FLAGS               0x8220
# define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT   0x00004000

#endif /* GLINCLUDES_H */
