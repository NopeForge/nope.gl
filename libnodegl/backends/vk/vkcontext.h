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

#ifndef VKCONTEXT_H
#define VKCONTEXT_H

#include <stdlib.h>

#include <vulkan/vulkan.h>

#include "config.h"

#include "darray.h"
#include "nodegl.h"
#include "rendertarget.h"
#include "texture.h"

#define VK_FUNC(name) PFN_vk##name
#define VK_DECLARE_FUNC(name) VK_FUNC(name) name

#define VK_LOAD_FUNC(instance, name) \
    VK_DECLARE_FUNC(name) = (VK_FUNC(name))vkGetInstanceProcAddr(instance, "vk" #name);

struct vkcontext {
    uint32_t api_version;
    VkInstance instance;
    VkLayerProperties *layers;
    uint32_t nb_layers;
    VkExtensionProperties *extensions;
    uint32_t nb_extensions;
    VkDebugUtilsMessengerEXT debug_callback;
    VkSurfaceKHR surface;

    VkExtensionProperties *device_extensions;
    uint32_t nb_device_extensions;

#if defined(TARGET_LINUX)
    int own_x11_display;
    void *x11_display;
#endif

    VkPhysicalDevice *phy_devices;
    uint32_t nb_phy_devices;
    VkPhysicalDevice phy_device;
    VkPhysicalDeviceProperties phy_device_props;
    uint32_t graphics_queue_index;
    uint32_t present_queue_index;
    VkQueue graphic_queue;
    VkQueue present_queue;
    VkDevice device;

    int preferred_depth_format;
    int preferred_depth_stencil_format;

    VkPhysicalDeviceFeatures dev_features;
    VkPhysicalDeviceMemoryProperties phydev_mem_props;
    VkPhysicalDeviceLimits phydev_limits;

    VkSurfaceCapabilitiesKHR surface_caps;
    VkSurfaceFormatKHR *surface_formats;
    uint32_t nb_surface_formats;
    VkPresentModeKHR *present_modes;
    uint32_t nb_present_modes;
    int support_present_mode_immediate;

    /* Device functions */
    VK_DECLARE_FUNC(CreateSamplerYcbcrConversionKHR);
    VK_DECLARE_FUNC(DestroySamplerYcbcrConversionKHR);
    VK_DECLARE_FUNC(GetMemoryFdKHR);
    VK_DECLARE_FUNC(GetMemoryFdPropertiesKHR);
    VK_DECLARE_FUNC(GetRefreshCycleDurationGOOGLE);
    VK_DECLARE_FUNC(GetPastPresentationTimingGOOGLE);
};

struct vkcontext *ngli_vkcontext_create(void);
VkResult ngli_vkcontext_init(struct vkcontext *s, const struct ngl_config *config);
void *ngli_vkcontext_get_proc_addr(struct vkcontext *s, const char *name);
int ngli_vkcontext_has_extension(const struct vkcontext *s, const char *name, int device);
VkFormat ngli_vkcontext_find_supported_format(struct vkcontext *s, const VkFormat *formats,
                                              VkImageTiling tiling, VkFormatFeatureFlags features);
int ngli_vkcontext_find_memory_type(struct vkcontext *s, uint32_t type, VkMemoryPropertyFlags props);
void ngli_vkcontext_freep(struct vkcontext **sp);

#endif /* VKCONTEXT_H */
