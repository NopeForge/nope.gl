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

#include <glslang/build_info.h>
#include <glslang/Include/glslang_c_interface.h>

#include "glslang_utils.h"
#include "log.h"
#include "memory.h"
#include "nopegl.h"
#include "program.h"
#include "pthread_compat.h"

/*
 * resource_limits_c.h which declares glslang_default_resource() is currently
 * not distributed, so forward declare it.
 * See: https://github.com/KhronosGroup/glslang/issues/2822
 */
const glslang_resource_t *glslang_default_resource(void);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int refcount = 0;

int ngli_glslang_init(void)
{
    int ret = 0;
    pthread_mutex_lock(&lock);
    if (!refcount++) {
        if (!glslang_initialize_process()) {
            refcount--;
            ret = NGL_ERROR_EXTERNAL;
        }
    }
    pthread_mutex_unlock(&lock);

    return ret;
}

int ngli_glslang_compile(int stage, const char *src, void **datap, size_t *sizep)
{
    static const glslang_stage_t stages[] = {
        [NGLI_PROGRAM_SHADER_VERT] = GLSLANG_STAGE_VERTEX,
        [NGLI_PROGRAM_SHADER_FRAG] = GLSLANG_STAGE_FRAGMENT,
        [NGLI_PROGRAM_SHADER_COMP] = GLSLANG_STAGE_COMPUTE,
    };

    /*
     * TODO: specify optimization level when available.
     * See: https://github.com/KhronosGroup/glslang/issues/2907
     */
    const glslang_input_t glslc_input = {
        .language                          = GLSLANG_SOURCE_GLSL,
        .stage                             = stages[stage],
        .client                            = GLSLANG_CLIENT_VULKAN,
        .client_version                    = GLSLANG_TARGET_VULKAN_1_1,
        .target_language_version           = GLSLANG_TARGET_SPV_1_3,
        .target_language                   = GLSLANG_TARGET_SPV,
        .code                              = src,
        .default_version                   = 450,
        .default_profile                   = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible                = false,
        .messages                          = GLSLANG_MSG_DEFAULT_BIT,
        .resource                          = glslang_default_resource(),
    };

    glslang_shader_t *shader = glslang_shader_create(&glslc_input);
    if (!shader)
        return NGL_ERROR_MEMORY;

    int ret = glslang_shader_preprocess(shader, &glslc_input);
    if (!ret) {
        LOG(ERROR, "unable to preprocess shader:\n%s",
            glslang_shader_get_info_log(shader));
        glslang_shader_delete(shader);
        return NGL_ERROR_EXTERNAL;
    }

    ret = glslang_shader_parse(shader, &glslc_input);
    if (!ret) {
        LOG(ERROR, "unable to parse shader:\n%s",
            glslang_shader_get_info_log(shader));
        glslang_shader_delete(shader);
        return NGL_ERROR_EXTERNAL;
    }

    glslang_program_t *program = glslang_program_create();
    if (!program) {
        glslang_shader_delete(shader);
        return NGL_ERROR_MEMORY;
    }
    glslang_program_add_shader(program, shader);

    ret = glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT);
    if (!ret) {
        LOG(ERROR, "unable to link shader:\n%s",
            glslang_shader_get_info_log(shader));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return NGL_ERROR_EXTERNAL;
    }

    glslang_program_SPIRV_generate(program, glslc_input.stage);
    const char *messages = glslang_program_SPIRV_get_messages(program);
    if (messages)
        LOG(WARNING, "%s", messages);

    const size_t size = glslang_program_SPIRV_get_size(program) * sizeof(unsigned int);
    unsigned int *data = ngli_malloc(size);
    if (!data) {
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return NGL_ERROR_MEMORY;
    }
    glslang_program_SPIRV_get(program, data);

    *datap = data;
    *sizep = size;

    glslang_shader_delete(shader);
    glslang_program_delete(program);

    return 0;
}

void ngli_glslang_uninit(void)
{
    pthread_mutex_lock(&lock);
    if (refcount && (refcount-- == 1))
        glslang_finalize_process();
    pthread_mutex_unlock(&lock);
}
