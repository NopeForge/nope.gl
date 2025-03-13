/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
 * Copyright 2018-2022 GoPro Inc.
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

#include <stddef.h>
#include <stdint.h>

#include "glcontext.h"

#define OFFSET(x) offsetof(struct glfunctions, x)
static const struct glfeature {
    const char *name;
    uint64_t flag;
    size_t offset;
    int version;
    int es_version;
    const char **extensions;
    const char **es_extensions;
    const size_t *funcs_offsets;
} glfeatures[] = {
    {
        .name           = "timer_query",
        .flag           = NGLI_FEATURE_GL_TIMER_QUERY,
        .version        = 330,
        .extensions     = (const char*[]){"ARB_timer_query", NULL},
    }, {
        .name           = "ext_disjoint_timer_query",
        .flag           = NGLI_FEATURE_GL_EXT_DISJOINT_TIMER_QUERY,
        .es_extensions  = (const char*[]){"GL_EXT_disjoint_timer_query", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(BeginQueryEXT),
                                           OFFSET(EndQueryEXT),
                                           OFFSET(GenQueriesEXT),
                                           OFFSET(DeleteQueriesEXT),
                                           OFFSET(QueryCounterEXT),
                                           OFFSET(GetQueryObjectui64vEXT),
                                           SIZE_MAX}
    }, {
        .name           = "oes_egl_external_image",
        .flag           = NGLI_FEATURE_GL_OES_EGL_EXTERNAL_IMAGE,
        .es_extensions  = (const char*[]){"GL_OES_EGL_image_external",
                                          "GL_OES_EGL_image_external_essl3",
                                          NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(EGLImageTargetTexture2DOES),
                                           SIZE_MAX}
    }, {
        .name           = "oes_egl_image",
        .flag           = NGLI_FEATURE_GL_OES_EGL_IMAGE,
        .extensions     = (const char*[]){"GL_OES_EGL_image", NULL},
        .es_extensions  = (const char*[]){"GL_OES_EGL_image", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(EGLImageTargetTexture2DOES),
                                           SIZE_MAX}
    }, {
        .name           = "yuv_target",
        .flag           = NGLI_FEATURE_GL_YUV_TARGET,
        .es_extensions  = (const char*[]){"GL_EXT_YUV_target", NULL}
    }, {
        .name           = "khr_debug",
        .flag           = NGLI_FEATURE_GL_KHR_DEBUG,
        .version        = 430,
        .es_version     = 320,
        .es_extensions  = (const char*[]){"GL_KHR_debug", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(DebugMessageCallback),
                                        SIZE_MAX}
    }, {
        .name           = "color_buffer_float",
        .flag           = NGLI_FEATURE_GL_COLOR_BUFFER_FLOAT,
        .version        = 300,
        .es_version     = 320,
        .es_extensions  = (const char*[]){"EXT_color_buffer_float", NULL},
    }, {
        .name           = "color_buffer_half_float",
        .flag           = NGLI_FEATURE_GL_COLOR_BUFFER_HALF_FLOAT,
        .version        = 300,
        .es_version     = 320,
        .es_extensions  = (const char*[]){"EXT_color_buffer_half_float", NULL},
    }, {
        .name           = "buffer_storage",
        .flag           = NGLI_FEATURE_GL_BUFFER_STORAGE,
        .version        = 440,
        .extensions     = (const char*[]){"GL_ARB_buffer_storage", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(BufferStorage),
                                           SIZE_MAX}
    }, {
        .name           = "ext_buffer_storage",
        .flag           = NGLI_FEATURE_GL_EXT_BUFFER_STORAGE,
        .es_extensions  = (const char*[]){"EXT_buffer_storage", NULL},
        .funcs_offsets  = (const size_t[]){OFFSET(BufferStorageEXT),
                                           SIZE_MAX}
    }, {
        .name           = "texture_norm16",
        .flag           = NGLI_FEATURE_GL_TEXTURE_NORM16,
        .version        = 300,
        .es_extensions  = (const char*[]){"EXT_texture_norm16", NULL},
    }, {
        .name           = "texture_float_linear",
        .flag           = NGLI_FEATURE_GL_TEXTURE_FLOAT_LINEAR,
        .version        = 300,
        .es_version     = 320,
        .es_extensions  = (const char*[]){"OES_texture_float_linear", NULL},
    }, {
        .name           = "float_blend",
        .flag           = NGLI_FEATURE_GL_FLOAT_BLEND,
        .version        = 300,
        .es_version     = 320,
        .es_extensions  = (const char*[]){"EXT_float_blend", NULL},
    },
};
