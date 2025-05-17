/*
 * Copyright 2020-2022 GoPro Inc.
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

#ifndef VKUTILS_H
#define VKUTILS_H

#include <stdint.h>
#include <vulkan/vulkan.h>

const char *ngli_vk_res2str(VkResult res);
int ngli_vk_res2ret(VkResult res);

VkSampleCountFlagBits ngli_ngl_samples_to_vk(uint32_t samples);
uint32_t ngli_vk_samples_to_ngl(VkSampleCountFlags samples);

#endif
