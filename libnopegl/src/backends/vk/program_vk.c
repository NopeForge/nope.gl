/*
 * Copyright 2019-2022 GoPro Inc.
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

#include <stdlib.h>
#include <string.h>

#include "glslang_utils.h"
#include "gpu_ctx_vk.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "program_vk.h"
#include "utils.h"
#include "vkutils.h"

struct program *ngli_program_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct program_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct program *)s;
}

int ngli_program_vk_init(struct program *s, const struct program_params *params)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct program_vk *s_priv = (struct program_vk *)s;

    const struct {
        int stage;
        const char *src;
    } shaders[] = {
        {NGLI_PROGRAM_SHADER_VERT, params->vertex},
        {NGLI_PROGRAM_SHADER_FRAG, params->fragment},
        {NGLI_PROGRAM_SHADER_COMP, params->compute},
    };

    for (size_t i = 0; i < NGLI_ARRAY_NB(shaders); i++) {
        if (!shaders[i].src)
            continue;

        void *data = NULL;
        size_t size = 0;
        int ret = ngli_glslang_compile(shaders[i].stage, shaders[i].src, &data, &size);
        if (ret < 0) {
            char *s_with_numbers = ngli_numbered_lines(shaders[i].src);
            if (s_with_numbers) {
                LOG(ERROR, "failed to compile shader \"%s\":\n%s",
                    params->label ? params->label : "", s_with_numbers);
                ngli_free(s_with_numbers);
            }
            return ret;
        }

        const VkShaderModuleCreateInfo shader_module_create_info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = size,
            .pCode    = data,
        };
        VkResult res = vkCreateShaderModule(vk->device, &shader_module_create_info, NULL, &s_priv->shaders[i]);
        ngli_freep(&data);
        if (res != VK_SUCCESS) {
            char *s_with_numbers = ngli_numbered_lines(shaders[i].src);
            if (s_with_numbers) {
                LOG(ERROR, "failed to compile shader \"%s\":\n%s",
                    params->label ? params->label : "", s_with_numbers);
                ngli_free(s_with_numbers);
            }
            return ngli_vk_res2ret(res);
        }
    }

    return 0;
}

void ngli_program_vk_freep(struct program **sp)
{
    struct program *s = *sp;
    if (!s)
        return;

    struct program_vk *s_priv = (struct program_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    for (size_t i = 0; i < NGLI_ARRAY_NB(s_priv->shaders); i++)
        vkDestroyShaderModule(vk->device, s_priv->shaders[i], NULL);
    ngli_freep(sp);
}
