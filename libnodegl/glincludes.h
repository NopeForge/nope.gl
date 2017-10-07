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

#ifndef GLINCLUDES_H
#define GLINCLUDES_H

#if __APPLE__
# include <TargetConditionals.h>
# if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
#  define NGL_GLES2_COMPAT_INCLUDES 1
#  define NGL_CS_COMPAT_INCLUDES 1
# elif TARGET_OS_MAC
#  include <OpenGL/gl3.h>
#  include <OpenGL/glext.h>
#  define NGL_CS_COMPAT_INCLUDES 1
# endif
#endif

#if __ANDROID__
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>
# define NGL_GLES2_COMPAT_INCLUDES 1
# define NGL_CS_COMPAT_INCLUDES 1
# define GL_BGRA GL_BGRA_EXT
#endif

#if __linux__ && !__ANDROID__
# define GL_GLEXT_PROTOTYPES
# include <GL/glcorearb.h>
# include <GL/glext.h>
#endif

#if _WIN32
# include <GL/gl.h>
# include <GL/glcorearb.h>
# include <GL/glext.h>
#endif

#if NGL_GLES2_COMPAT_INCLUDES
# define GL_MAJOR_VERSION                      0x821B
# define GL_MINOR_VERSION                      0x821C
# define GL_NUM_EXTENSIONS                     0x821D
# define GL_RED                                GL_LUMINANCE
# define GL_R32F                               0x822E
# define GL_READ_ONLY                          0x88B8
# define GL_WRITE_ONLY                         0x88B9
# define GL_READ_WRITE                         0x88BA
# define GL_STREAM_READ                        0x88E1
# define GL_STREAM_COPY                        0x88E2
# define GL_STATIC_READ                        0x88E5
# define GL_STATIC_COPY                        0x88E6
# define GL_DYNAMIC_READ                       0x88E9
# define GL_DYNAMIC_COPY                       0x88EA
# define GL_INVALID_INDEX                      0xFFFFFFFFU
#endif

#if NGL_CS_COMPAT_INCLUDES
# define GL_COMPUTE_SHADER                     0x91B9
# define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS 0x90EB
# define GL_MAX_COMPUTE_WORK_GROUP_COUNT       0x91BE
# define GL_MAX_COMPUTE_WORK_GROUP_SIZE        0x91BF
# define GL_COMPUTE_WORK_GROUP_SIZE            0x8267
# define GL_SHADER_STORAGE_BUFFER              0x90D2
# define GL_SHADER_STORAGE_BUFFER_BINDING      0x90D3
# define GL_SHADER_STORAGE_BUFFER_START        0x90D4
# define GL_SHADER_STORAGE_BUFFER_SIZE         0x90D5
# define GL_SHADER_STORAGE_BLOCK               0x92E6
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
# define GL_ALL_BARRIER_BITS                   0xFFFFFFFF
#endif

#endif /* GLINCLUDES_H */
