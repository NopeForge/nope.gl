/*
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

#ifdef __APPLE__
# include <TargetConditionals.h>
# if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#  include <OpenGLES/ES3/gl.h>
#  include <OpenGLES/ES3/glext.h>
# elif TARGET_OS_MAC
#  include <OpenGL/gl3.h>
#  include <OpenGL/glext.h>
# endif
#endif

#ifdef __ANDROID__
# include <GLES3/gl3.h>
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

/* Desktop formats and features */
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
# define GL_READ_ONLY                          0x88B8
# define GL_WRITE_ONLY                         0x88B9
# define GL_READ_WRITE                         0x88BA
# define GL_TIMESTAMP                          0x8E28
# define GL_TEXTURE_CUBE_MAP_SEAMLESS          0x884F
# define GL_FRONT_LEFT                         0x0400

/* Compute shaders */
# define GL_COMPUTE_SHADER                     0x91B9
# define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS 0x90EB
# define GL_MAX_COMPUTE_WORK_GROUP_COUNT       0x91BE
# define GL_MAX_COMPUTE_WORK_GROUP_SIZE        0x91BF
# define GL_MAX_COMPUTE_SHARED_MEMORY_SIZE     0x8262
# define GL_COMPUTE_WORK_GROUP_SIZE            0x8267
# define GL_SHADER_STORAGE_BUFFER              0x90D2
# define GL_SHADER_STORAGE_BUFFER_BINDING      0x90D3
# define GL_SHADER_STORAGE_BUFFER_START        0x90D4
# define GL_SHADER_STORAGE_BUFFER_SIZE         0x90D5
# define GL_MAX_SHADER_STORAGE_BLOCK_SIZE      0x90DE
# define GL_UNIFORM_BLOCK                      0x92E2
# define GL_SHADER_STORAGE_BLOCK               0x92E6
# define GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT 0x90DF
# define GL_BUFFER_BINDING                     0x9302
# define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT    0x00000001
# define GL_ELEMENT_ARRAY_BARRIER_BIT          0x00000002
# define GL_UNIFORM_BARRIER_BIT                0x00000004
# define GL_TEXTURE_FETCH_BARRIER_BIT          0x00000008
# define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT    0x00000020
# define GL_COMMAND_BARRIER_BIT                0x00000040
# define GL_PIXEL_BUFFER_BARRIER_BIT           0x00000080
# define GL_TEXTURE_UPDATE_BARRIER_BIT         0x00000100
# define GL_BUFFER_UPDATE_BARRIER_BIT          0x00000200
# define GL_FRAMEBUFFER_BARRIER_BIT            0x00000400
# define GL_TRANSFORM_FEEDBACK_BARRIER_BIT     0x00000800
# define GL_ATOMIC_COUNTER_BARRIER_BIT         0x00001000
# define GL_SHADER_STORAGE_BARRIER_BIT         0x00002000
# define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT   0x00004000
# define GL_ALL_BARRIER_BITS                   0xFFFFFFFF
# define GL_IMAGE_2D                           0x904D
# define GL_ACTIVE_RESOURCES                   0x92F5
# define GL_MAX_IMAGE_UNITS                    0x8F38
# define GL_DYNAMIC_STORAGE_BIT                0x0100

/* Persistent buffer mapping */
# define GL_MAP_PERSISTENT_BIT                 0x0040
# define GL_MAP_COHERENT_BIT                   0x0080
# define GL_DYNAMIC_STORAGE_BIT                0x0100
# define GL_CLIENT_STORAGE_BIT                 0x0200

#endif /* GLINCLUDES_H */
