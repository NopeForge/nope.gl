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

#include <stddef.h>

#include "gpu_ctx_vk.h"
#include "log.h"
#include "memory.h"
#include "vkutils.h"
#include "ycbcr_sampler_vk.h"

struct ycbcr_sampler_vk *ngli_ycbcr_sampler_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct ycbcr_sampler_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->refcount = 1;
    s->gpu_ctx = gpu_ctx;
    return s;
}

VkResult ngli_ycbcr_sampler_vk_init(struct ycbcr_sampler_vk *s, const struct ycbcr_sampler_vk_params *params)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    s->params = *params;

    const VkExternalFormatANDROID external_format = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
        .externalFormat = params->android_external_format,
    };

    const VkSamplerYcbcrConversionCreateInfoKHR sampler_ycbcr_info = {
        .sType         = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO_KHR,
        .pNext         = &external_format,
        .format        = params->format,
        .ycbcrModel    = params->ycbcr_model,
        .ycbcrRange    = params->ycbcr_range,
        .components    = params->components,
        .xChromaOffset = params->x_chroma_offset,
        .yChromaOffset = params->y_chroma_offset,
        .chromaFilter  = params->filter,
        .forceExplicitReconstruction = VK_FALSE,
    };

    VkResult res = vk->CreateSamplerYcbcrConversionKHR(vk->device, &sampler_ycbcr_info, 0, &s->conv);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not create sampler YCbCr conversion: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    const VkSamplerYcbcrConversionInfoKHR sampler_ycbcr_conv_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR,
        .conversion = s->conv,
    };

    const VkSamplerCreateInfo sampler_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = &sampler_ycbcr_conv_info,
        .magFilter               = params->filter,
        .minFilter               = params->filter,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_NEVER,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    res = vkCreateSampler(vk->device, &sampler_info, 0, &s->sampler);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "could not create sampler: %s", ngli_vk_res2str(res));
        return NGL_ERROR_GRAPHICS_GENERIC;
    }

    return VK_SUCCESS;
}

static int ycbcr_sampler_vk_params_eq(const struct ycbcr_sampler_vk_params *p0,
                                      const struct ycbcr_sampler_vk_params *p1)
{
    return p0->android_external_format == p1->android_external_format &&
           p0->format                  == p1->format &&
           p0->ycbcr_model             == p1->ycbcr_model &&
           p0->ycbcr_range             == p1->ycbcr_range &&
           p0->components.r            == p1->components.r &&
           p0->components.g            == p1->components.g &&
           p0->components.b            == p1->components.b &&
           p0->components.a            == p1->components.a &&
           p0->x_chroma_offset         == p1->x_chroma_offset &&
           p0->y_chroma_offset         == p1->y_chroma_offset &&
           p0->filter                  == p1->filter;
}

int ngli_ycbcr_sampler_vk_is_compat(const struct ycbcr_sampler_vk *s,
                                    const struct ycbcr_sampler_vk_params *params)
{
    return ycbcr_sampler_vk_params_eq(&s->params, params);
}

struct ycbcr_sampler_vk *ngli_ycbcr_sampler_vk_ref(struct ycbcr_sampler_vk *s)
{
    s->refcount++;
    return s;
}

void ngli_ycbcr_sampler_vk_unrefp(struct ycbcr_sampler_vk **sp)
{
    struct ycbcr_sampler_vk *s = *sp;
    if (!s)
        return;

    if (s->refcount-- == 1) {
        struct gpu_ctx *gpu_ctx = s->gpu_ctx;
        struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)gpu_ctx;
        struct vkcontext *vk = gpu_ctx_vk->vkcontext;
        vkDestroySampler(vk->device, s->sampler, NULL);
        vk->DestroySamplerYcbcrConversionKHR(vk->device, s->conv, NULL);
        ngli_free(s);
    }

    *sp = NULL;
}
