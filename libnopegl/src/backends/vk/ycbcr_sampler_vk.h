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

#ifndef YCBCR_SAMPLER_VK_H
#define YCBCR_SAMPLER_VK_H

#include <stdint.h>
#include <vulkan/vulkan.h>

struct gpu_ctx;

struct ycbcr_sampler_vk_params {
    /* Conversion params */
    uint64_t android_external_format;
    VkFormat format;
    VkSamplerYcbcrModelConversion ycbcr_model;
    VkSamplerYcbcrRange ycbcr_range;
    VkComponentMapping components;
    VkChromaLocation x_chroma_offset;
    VkChromaLocation y_chroma_offset;
    /* Sampler params */
    VkFilter filter;
};

struct ycbcr_sampler_vk {
    int refcount;
    struct gpu_ctx *gpu_ctx;
    struct ycbcr_sampler_vk_params params;
    VkSamplerYcbcrConversion conv;
    VkSampler sampler;
};

struct ycbcr_sampler_vk *ngli_ycbcr_sampler_vk_create(struct gpu_ctx *gpu_ctx);
VkResult ngli_ycbcr_sampler_vk_init(struct ycbcr_sampler_vk *s,
                                    const struct ycbcr_sampler_vk_params *params);
int ngli_ycbcr_sampler_vk_is_compat(const struct ycbcr_sampler_vk *s,
                                    const struct ycbcr_sampler_vk_params *params);
struct ycbcr_sampler_vk *ngli_ycbcr_sampler_vk_ref(struct ycbcr_sampler_vk *s);
void ngli_ycbcr_sampler_vk_unrefp(struct ycbcr_sampler_vk **sp);

#endif
