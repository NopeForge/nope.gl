/*
 * Copyright 2018 GoPro Inc.
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

/* WARNING: this file must only be included once */

#include "glcontext.h"

#define OFFSET(x) offsetof(struct glfunctions, x)
static const struct glfeature {
    const char *name;
    int flag;
    size_t offset;
    int8_t maj_version;
    int8_t min_version;
    int8_t maj_es_version;
    int8_t min_es_version;
    const char **extensions;
    const char **es_extensions;
    const size_t *funcs_offsets;
} glfeatures[] = {
    {
        .name           = "vertex_array_object",
        .flag           = NGLI_FEATURE_VERTEX_ARRAY_OBJECT,
        .maj_version    = 3,
        .min_version    = 0,
        .maj_es_version = 3,
        .min_es_version = 0,
        .extensions     = (const char*[]){"GL_ARB_vertex_array_object", NULL},
        .es_extensions  = (const char*[]){"GL_OES_vertex_array_object", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(GenVertexArrays),
                                           OFFSET(BindVertexArray),
                                           OFFSET(DeleteVertexArrays),
                                           -1}
    }, {
        .name           = "texture3d",
        .flag           = NGLI_FEATURE_TEXTURE_3D,
        .maj_version    = 2,
        .min_version    = 0,
        .maj_es_version = 3,
        .min_es_version = 0,
        .funcs_offsets  = (const size_t[]){OFFSET(TexImage3D),
                                           OFFSET(TexSubImage3D),
                                           -1}
    }, {
        .name           = "texture_storage",
        .flag           = NGLI_FEATURE_TEXTURE_STORAGE,
        .maj_version    = 4,
        .min_version    = 2,
        .maj_es_version = 3,
        .min_es_version = 1,
        .funcs_offsets  = (const size_t[]){OFFSET(TexStorage2D),
                                           OFFSET(TexStorage3D),
                                           -1}
    }, {
        .name           = "compute_shader",
        .flag           = NGLI_FEATURE_COMPUTE_SHADER,
        .maj_version    = 4,
        .min_version    = 3,
        .maj_es_version = 3,
        .min_es_version = 1,
        .extensions     = (const char*[]){"GL_ARB_compute_shader", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(DispatchCompute),
                                           OFFSET(MemoryBarrier),
                                           -1}
    }, {
        .name           = "program_interface_query",
        .flag           = NGLI_FEATURE_PROGRAM_INTERFACE_QUERY,
        .maj_version    = 4,
        .min_version    = 3,
        .maj_es_version = 3,
        .min_es_version = 1,
        .extensions     = (const char*[]){"GL_ARB_program_interface_query", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(GetProgramResourceIndex),
                                           OFFSET(GetProgramResourceiv),
                                           OFFSET(GetProgramResourceLocation),
                                           -1}
    }, {
        .name           = "shader_image_load_store",
        .flag           = NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE,
        .maj_version    = 4,
        .min_version    = 2,
        .maj_es_version = 3,
        .min_es_version = 1,
        .extensions     = (const char*[]){"GL_ARB_shader_image_load_store", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(BindImageTexture),
                                           -1}
    }, {
        .name           = "shader_storage_buffer_object",
        .flag           = NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT,
        .maj_version    = 4,
        .min_version    = 3,
        .maj_es_version = 3,
        .min_es_version = 1,
        .extensions     = (const char*[]){"GL_ARB_shader_storage_buffer_object", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(TexStorage2D),
                                           OFFSET(TexStorage3D),
                                           -1}
    }, {
        .name           = "framebuffer_object",
        .flag           = NGLI_FEATURE_FRAMEBUFFER_OBJECT,
        .maj_version    = 3,
        .min_version    = 0,
        .maj_es_version = 3,
        .min_es_version = 0,
        .extensions     = (const char*[]){"ARB_framebuffer_object", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(RenderbufferStorageMultisample),
                                           -1}
    }, {
        .name           = "internalformat_query",
        .flag           = NGLI_FEATURE_INTERNALFORMAT_QUERY,
        .maj_version    = 4,
        .min_version    = 2,
        .maj_es_version = 3,
        .min_es_version = 0,
        .extensions     = (const char*[]){"ARB_internalformat_query", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(GetInternalformativ),
                                           -1}
    }, {
        .name           = "packed_depth_stencil",
        .flag           = NGLI_FEATURE_PACKED_DEPTH_STENCIL,
        .maj_version    = 3,
        .min_version    = 0,
        .maj_es_version = 3,
        .min_es_version = 0,
    }, {
        .name           = "timer_query",
        .flag           = NGLI_FEATURE_TIMER_QUERY,
        .maj_version    = 3,
        .min_version    = 3,
        .extensions     = (const char*[]){"ARB_timer_query", NULL},
    }, {
        .name           = "ext_disjoint_timer_query",
        .flag           = NGLI_FEATURE_EXT_DISJOINT_TIMER_QUERY,
        .maj_es_version = 2,
        .min_es_version = 0,
        .es_extensions  = (const char*[]){"GL_EXT_disjoint_timer_query", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(BeginQueryEXT),
                                           OFFSET(EndQueryEXT),
                                           OFFSET(GenQueriesEXT),
                                           OFFSET(DeleteQueriesEXT),
                                           OFFSET(GetQueryObjectui64vEXT),
                                           -1}
    }
};
