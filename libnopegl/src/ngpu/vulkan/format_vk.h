/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef FORMAT_VK_H
#define FORMAT_VK_H

#include <stdint.h>
#include <vulkan/vulkan.h>

#include "ngpu/format.h"
#include "vkcontext.h"

VkFormat ngpu_format_ngl_to_vk(enum ngpu_format format);
enum ngpu_format ngpu_format_vk_to_ngl(VkFormat format);
VkFormatFeatureFlags ngpu_format_feature_ngl_to_vk(uint32_t features);
uint32_t ngpu_format_feature_vk_to_ngl(VkFormatFeatureFlags features);

#endif
