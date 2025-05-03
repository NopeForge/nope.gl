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

#include "vkutils.h"
#include "nopegl.h"
#include "utils/utils.h"

const char *ngli_vk_res2str(VkResult res)
{
    switch (res) {
    case VK_SUCCESS:                        return "success";
    case VK_NOT_READY:                      return "not ready";
    case VK_TIMEOUT:                        return "timeout";
    case VK_EVENT_SET:                      return "event set";
    case VK_EVENT_RESET:                    return "event reset";
    case VK_INCOMPLETE:                     return "incomplete";
    case VK_ERROR_OUT_OF_HOST_MEMORY:       return "out of host memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "out of device memory";
    case VK_ERROR_INITIALIZATION_FAILED:    return "initialization failed";
    case VK_ERROR_DEVICE_LOST:              return "device lost";
    case VK_ERROR_MEMORY_MAP_FAILED:        return "memory map failed";
    case VK_ERROR_LAYER_NOT_PRESENT:        return "layer not present";
    case VK_ERROR_EXTENSION_NOT_PRESENT:    return "extension not present";
    case VK_ERROR_FEATURE_NOT_PRESENT:      return "feature not present";
    case VK_ERROR_INCOMPATIBLE_DRIVER:      return "incompatible driver";
    case VK_ERROR_TOO_MANY_OBJECTS:         return "too many objects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "format not supported";
    case VK_ERROR_FRAGMENTED_POOL:          return "fragmented pool";
    case VK_ERROR_UNKNOWN:                  return "unknown";
    case VK_ERROR_OUT_OF_POOL_MEMORY:       return "out of pool memory";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:  return "invalid external handle";
    case VK_ERROR_FRAGMENTATION:            return "fragmentation";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
                                            return "invalid opaque capture address";
    case VK_ERROR_SURFACE_LOST_KHR:         return "surface lost";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native window in use";
    case VK_SUBOPTIMAL_KHR:                 return "suboptimal";
    case VK_ERROR_OUT_OF_DATE_KHR:          return "out of date";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "incompatible display";
    case VK_ERROR_VALIDATION_FAILED_EXT:    return "validation failed";
    case VK_ERROR_INVALID_SHADER_NV:        return "invalid shader nv";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
                                            return "invalid DRM format modifier plane layout";
    case VK_ERROR_NOT_PERMITTED_EXT:        return "not permitted";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
                                            return "full screen exclusive mode lost";
    /*
     * Currently unmapped VkResult:
     * - VK_THREAD_IDLE_KHR
     * - VK_THREAD_DONE_KHR
     * - VK_OPERATION_DEFERRED_KHR
     * - VK_OPERATION_NOT_DEFERRED_KHR
     */
    default:                                return "unknown";
    }
}

int ngli_vk_res2ret(VkResult res)
{
    switch (res) {
    case VK_SUCCESS:                        return 0;
    case VK_ERROR_OUT_OF_HOST_MEMORY:       return NGL_ERROR_GRAPHICS_MEMORY;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return NGL_ERROR_GRAPHICS_MEMORY;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:     return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    case VK_ERROR_OUT_OF_POOL_MEMORY:       return NGL_ERROR_GRAPHICS_MEMORY;
    default:                                return NGL_ERROR_GRAPHICS_GENERIC;
    }
}

VkSampleCountFlagBits ngli_ngl_samples_to_vk(uint32_t samples)
{
    switch (samples) {
    case 0:
    case 1:  return VK_SAMPLE_COUNT_1_BIT;
    case 2:  return VK_SAMPLE_COUNT_2_BIT;
    case 4:  return VK_SAMPLE_COUNT_4_BIT;
    case 8:  return VK_SAMPLE_COUNT_8_BIT;
    case 16: return VK_SAMPLE_COUNT_16_BIT;
    case 32: return VK_SAMPLE_COUNT_32_BIT;
    case 64: return VK_SAMPLE_COUNT_64_BIT;
    default:
        ngli_assert(0);
    }
}

uint32_t ngli_vk_samples_to_ngl(VkSampleCountFlags samples)
{
    if (samples & VK_SAMPLE_COUNT_64_BIT) return 64;
    if (samples & VK_SAMPLE_COUNT_32_BIT) return 32;
    if (samples & VK_SAMPLE_COUNT_16_BIT) return 16;
    if (samples & VK_SAMPLE_COUNT_8_BIT)  return  8;
    if (samples & VK_SAMPLE_COUNT_4_BIT)  return  4;
    if (samples & VK_SAMPLE_COUNT_2_BIT)  return  2;
    if (samples & VK_SAMPLE_COUNT_1_BIT)  return  1;
    return 0;
}
