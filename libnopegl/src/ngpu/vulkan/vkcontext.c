/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "config.h"

#if defined(TARGET_LINUX)
# define VK_USE_PLATFORM_XLIB_KHR
# define VK_USE_PLATFORM_WAYLAND_KHR
#elif defined(TARGET_ANDROID)
# define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(TARGET_WINDOWS)
# define VK_USE_PLATFORM_WIN32_KHR
#elif defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
# include <MoltenVK/mvk_vulkan.h>
# include "wsi_apple.h"
#endif

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "log.h"
#include "ngpu/format.h"
#include "ngpu/vulkan/vkcontext.h"
#include "ngpu/vulkan/vkutils.h"
#include "nopegl.h"
#include "utils/bstr.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *cb_data,
                                                     void *user_data)
{
    /*
     * Silence VUID-VkSwapchainCreateInfoKHR-imageExtent-01274 as it is considered
     * a false positive.
     * See: https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/1340
     */
    if (cb_data->messageIdNumber == 0x7cd0911d)
        return VK_FALSE;

    enum ngl_log_level level = NGL_LOG_INFO;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)        level = NGL_LOG_ERROR;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) level = NGL_LOG_WARNING;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    level = NGL_LOG_INFO;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) level = NGL_LOG_VERBOSE;

    const char *msg_type = "GENERAL";
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)       msg_type = "VALIDATION";
    else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) msg_type = "PERFORMANCE";

    size_t msg_len = strlen(cb_data->pMessage);
    while (msg_len > 0 && strchr(" \r\n", cb_data->pMessage[msg_len - 1]))
        msg_len--;
    if (msg_len > INT_MAX)
        return VK_TRUE;
    ngli_log_print(level, __FILE__, __LINE__, "debug_callback", "%s: %.*s", msg_type, (int)msg_len, cb_data->pMessage);

    /* Make the Vulkan call fail if the validation layer has returned an error */
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) &&
        (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT))
        return VK_TRUE;

    return VK_FALSE;
}

static int has_layer(struct vkcontext *s, const char *name)
{
    for (uint32_t i = 0; i < s->nb_layers; i++)
        if (!strcmp(s->layers[i].layerName, name))
            return 1;
    return 0;
}

static const char *platform_ext_names[] = {
    [NGL_PLATFORM_XLIB]    = "VK_KHR_xlib_surface",
    [NGL_PLATFORM_ANDROID] = "VK_KHR_android_surface",
    [NGL_PLATFORM_MACOS]   = "VK_MVK_macos_surface",
    [NGL_PLATFORM_IOS]     = "VK_MVK_ios_surface",
    [NGL_PLATFORM_WINDOWS] = "VK_KHR_win32_surface",
    [NGL_PLATFORM_WAYLAND] = "VK_KHR_wayland_surface",
};

static VkResult create_instance(struct vkcontext *s, enum ngl_platform_type platform, int debug)
{
    s->api_version = VK_API_VERSION_1_0;

    VK_LOAD_FUNC(NULL, EnumerateInstanceVersion);
    if (EnumerateInstanceVersion) {
        VkResult res = EnumerateInstanceVersion(&s->api_version);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not enumerate Vulkan instance version");
        }
    }

    LOG(DEBUG, "available instance version: %d.%d.%d",
        (int)VK_VERSION_MAJOR(s->api_version),
        (int)VK_VERSION_MINOR(s->api_version),
        (int)VK_VERSION_PATCH(s->api_version));

    if (s->api_version < VK_API_VERSION_1_1) {
        LOG(ERROR, "instance API version (%d.%d.%d) is lower than the minimum supported version (%d.%d.%d)",
                    (int)VK_VERSION_MAJOR(s->api_version),
                    (int)VK_VERSION_MINOR(s->api_version),
                    (int)VK_VERSION_PATCH(s->api_version),
                    (int)VK_VERSION_MAJOR(VK_API_VERSION_1_1),
                    (int)VK_VERSION_MINOR(VK_API_VERSION_1_1),
                    (int)VK_VERSION_PATCH(VK_API_VERSION_1_1));
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    VkResult res = vkEnumerateInstanceLayerProperties(&s->nb_layers, NULL);
    if (res != VK_SUCCESS)
        return res;

    s->layers = ngli_calloc(s->nb_layers, sizeof(*s->layers));
    if (!s->layers)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumerateInstanceLayerProperties(&s->nb_layers, s->layers);
    if (res != VK_SUCCESS)
        return res;

    LOG(DEBUG, "available layers:");
    for (uint32_t i = 0; i < s->nb_layers; i++)
        LOG(DEBUG, "  %d/%d: %s", i+1, s->nb_layers, s->layers[i].layerName);

    res = vkEnumerateInstanceExtensionProperties(NULL, &s->nb_extensions, NULL);
    if (res != VK_SUCCESS)
        return res;

    s->extensions = ngli_calloc(s->nb_extensions, sizeof(*s->extensions));
    if (!s->extensions)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumerateInstanceExtensionProperties(NULL, &s->nb_extensions, s->extensions);
    if (res != VK_SUCCESS)
        return res;

    LOG(DEBUG, "available instance extensions:");
    for (uint32_t i = 0; i < s->nb_extensions; i++) {
        LOG(DEBUG, "  %d/%d: %s v%d", i+1, s->nb_extensions,
            s->extensions[i].extensionName, s->extensions[i].specVersion);
    }

    if (platform < 0 || platform >= NGLI_ARRAY_NB(platform_ext_names)) {
        LOG(ERROR, "unsupported platform: %d", platform);
        return VK_ERROR_UNKNOWN;
    }

    const char *surface_extension_name = platform_ext_names[platform];
    if (!surface_extension_name) {
        LOG(ERROR, "unsupported platform: %d", platform);
        return VK_ERROR_UNKNOWN;
    }

    const char *mandatory_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surface_extension_name,
#if defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_IOS_MVK)
        "VK_EXT_metal_surface",
#endif
    };

    struct darray extensions;
    ngli_darray_init(&extensions, sizeof(const char **), 0);

    struct darray layers;
    ngli_darray_init(&layers, sizeof(const char **), 0);

    for (size_t i = 0; i < NGLI_ARRAY_NB(mandatory_extensions); i++) {
        if (!ngli_darray_push(&extensions, &mandatory_extensions[i])) {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto end;
        }
    }

    static const char *debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    const int has_debug_extension = ngli_vkcontext_has_extension(s, debug_ext, 0);
    if (debug) {
        if (has_debug_extension && !ngli_darray_push(&extensions, &debug_ext)) {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto end;
        }

        static const char *debug_layer = "VK_LAYER_KHRONOS_validation";
        const int has_validation_layer = has_layer(s, debug_layer);
        if (has_validation_layer && !ngli_darray_push(&layers, &debug_layer)) {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto end;
        }

        if (!has_validation_layer)
            LOG(WARNING, "missing validation layer: %s", debug_layer);
    }

    const VkApplicationInfo app_info = {
        .sType         = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName   = "nope.gl",
        .engineVersion = NGL_VERSION_INT,
        .apiVersion    = s->api_version,
    };

    const VkInstanceCreateInfo instance_create_info = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &app_info,
        .enabledExtensionCount   = (uint32_t)ngli_darray_count(&extensions),
        .ppEnabledExtensionNames = ngli_darray_data(&extensions),
        .enabledLayerCount       = (uint32_t)ngli_darray_count(&layers),
        .ppEnabledLayerNames     = ngli_darray_data(&layers),
    };

    res = vkCreateInstance(&instance_create_info, NULL, &s->instance);
    if (res != VK_SUCCESS)
        goto end;

    if (debug && has_debug_extension) {
        VK_LOAD_FUNC(s->instance, CreateDebugUtilsMessengerEXT);
        if (!CreateDebugUtilsMessengerEXT)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        const VkDebugUtilsMessengerCreateInfoEXT info = {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
            .pUserData       = NULL,
        };

        res = CreateDebugUtilsMessengerEXT(s->instance, &info, NULL, &s->debug_callback);
        if (res != VK_SUCCESS)
            goto end;
    }

end:
    ngli_darray_reset(&extensions);
    ngli_darray_reset(&layers);

    return res;
}

static VkResult create_window_surface(struct vkcontext *s, const struct ngl_config *config)
{
    if (config->offscreen)
        return VK_SUCCESS;

    if (!config->window)
        return VK_ERROR_UNKNOWN;

    const enum ngl_platform_type platform = config->platform;
    if (platform == NGL_PLATFORM_XLIB) {
#if defined(TARGET_LINUX)
        s->x11_display = (Display *)config->display;
        if (!s->x11_display) {
            s->x11_display = XOpenDisplay(NULL);
            if (!s->x11_display) {
                LOG(ERROR, "could not open X11 display");
                return VK_ERROR_UNKNOWN;
            }
            s->own_x11_display = 1;
        }

        const VkXlibSurfaceCreateInfoKHR surface_create_info = {
            .sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy    = s->x11_display,
            .window = config->window,
        };

        VK_LOAD_FUNC(s->instance, CreateXlibSurfaceKHR);
        if (!CreateXlibSurfaceKHR) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateXlibSurfaceKHR(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    } else if (platform == NGL_PLATFORM_ANDROID) {
#if defined(TARGET_ANDROID)
        const VkAndroidSurfaceCreateInfoKHR surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .window = (struct ANativeWindow *)config->window,
        };

        VK_LOAD_FUNC(s->instance, CreateAndroidSurfaceKHR);
        if (!CreateAndroidSurfaceKHR) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateAndroidSurfaceKHR(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    } else if (platform == NGL_PLATFORM_MACOS || platform == NGL_PLATFORM_IOS) {
#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
        const void *view = (const void *)config->window;
        const CAMetalLayer *layer = ngpu_window_get_metal_layer(view);
        if (!layer)
            return VK_ERROR_UNKNOWN;

        VkMetalSurfaceCreateInfoEXT surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = layer,
        };

        VK_LOAD_FUNC(s->instance, CreateMetalSurfaceEXT);
        if (!CreateMetalSurfaceEXT) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateMetalSurfaceEXT(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#endif
    } else if (platform == NGL_PLATFORM_WINDOWS) {
#if defined(TARGET_WINDOWS)
        const VkWin32SurfaceCreateInfoKHR surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hwnd = (HWND)config->window,
        };

        VK_LOAD_FUNC(s->instance, CreateWin32SurfaceKHR);
        if (!CreateWin32SurfaceKHR) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateWin32SurfaceKHR(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#endif
    } else if (platform == NGL_PLATFORM_WAYLAND) {
#if defined(HAVE_WAYLAND)
        const VkWaylandSurfaceCreateInfoKHR surface_create_info = {
            .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = (struct wl_display *)config->display,
            .surface = (struct wl_surface *)config->window,
        };

        VK_LOAD_FUNC(s->instance, CreateWaylandSurfaceKHR);
        if (!CreateWaylandSurfaceKHR) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateWaylandSurfaceKHR(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    } else {
        ngli_assert(0);
    }

    return VK_SUCCESS;
}

static VkResult enumerate_physical_devices(struct vkcontext *s, const struct ngl_config *config)
{
    VkResult res = vkEnumeratePhysicalDevices(s->instance, &s->nb_phy_devices, NULL);
    if (res != VK_SUCCESS)
        return res;

    if (!s->nb_phy_devices)
        return VK_ERROR_DEVICE_LOST;

    s->phy_devices = ngli_calloc(s->nb_phy_devices, sizeof(*s->phy_devices));
    if (!s->phy_devices)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    return vkEnumeratePhysicalDevices(s->instance, &s->nb_phy_devices, s->phy_devices);
}

static void get_memory_property_flags_str(struct bstr *bstr, VkMemoryPropertyFlags flags)
{
    static const struct {
        VkMemoryPropertyFlagBits property;
        const char *name;
    } vk_mem_prop_map[] = {
        {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,        "device_local"},
        {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,        "host_visible"},
        {VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,       "host_coherent"},
        {VK_MEMORY_PROPERTY_HOST_CACHED_BIT,         "host_cached"},
        {VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,    "lazy_allocated"},
        {VK_MEMORY_PROPERTY_PROTECTED_BIT,           "protected"},
        {VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD, "device_coherent_amd"},
        {VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD, "device_uncached_amd"},
        {VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV,     "rdma_capable_nv"},
    };

    int nb_props = 0;
    for (size_t i = 0; i < NGLI_ARRAY_NB(vk_mem_prop_map); i++) {
        if (vk_mem_prop_map[i].property & flags) {
            ngli_bstr_printf(bstr, "%s%s", nb_props == 0 ? "" : "|", vk_mem_prop_map[i].name);
            nb_props++;
        }
    }
}

static VkResult select_physical_device(struct vkcontext *s, const struct ngl_config *config)
{
    static const struct {
        const char *name;
        uint32_t priority;
    } types[] = {
        [VK_PHYSICAL_DEVICE_TYPE_OTHER]          = {"other",      1},
        [VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU] = {"integrated", 4},
        [VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU]   = {"discrete",   5},
        [VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU]    = {"virtual",    3},
        [VK_PHYSICAL_DEVICE_TYPE_CPU]            = {"cpu",        2},
    };

    uint32_t priority = 0;
    for (uint32_t i = 0; i < s->nb_phy_devices; i++) {
        VkPhysicalDevice phy_device = s->phy_devices[i];

        VkPhysicalDeviceProperties dev_props;
        vkGetPhysicalDeviceProperties(phy_device, &dev_props);

        VkPhysicalDeviceFeatures dev_features;
        vkGetPhysicalDeviceFeatures(phy_device, &dev_features);

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(phy_device, &mem_props);

        if (dev_props.deviceType >= NGLI_ARRAY_NB(types)) {
            LOG(ERROR, "device %s has unknown type: 0x%x, skipping", dev_props.deviceName, dev_props.deviceType);
            continue;
        }

        uint32_t qfamily_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &qfamily_count, NULL);
        VkQueueFamilyProperties *qfamily_props = ngli_calloc(qfamily_count, sizeof(*qfamily_props));
        if (!qfamily_props)
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &qfamily_count, qfamily_props);

        int32_t found_queues = 0;
        int32_t queue_family_graphics_id = -1;
        int32_t queue_family_present_id = -1;
        for (uint32_t j = 0; j < qfamily_count; j++) {
            const VkQueueFamilyProperties props = qfamily_props[j];
            const VkQueueFlags flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if (NGLI_HAS_ALL_FLAGS(props.queueFlags, flags))
                queue_family_graphics_id = j;
            if (s->surface) {
                VkBool32 support;
                vkGetPhysicalDeviceSurfaceSupportKHR(phy_device, j, s->surface, &support);
                if (support)
                    queue_family_present_id = j;
            }
            found_queues = queue_family_graphics_id >= 0 && (!s->surface || queue_family_present_id >= 0);
            if (found_queues)
                break;
        }
        ngli_free(qfamily_props);

        if (!found_queues)
            continue;

        if (types[dev_props.deviceType].priority > priority) {
            priority = types[dev_props.deviceType].priority;
            s->phy_device = phy_device;
            s->phy_device_props = dev_props;
            s->graphics_queue_index = queue_family_graphics_id;
            s->present_queue_index = queue_family_present_id;
            s->dev_features = dev_features;
            s->phydev_mem_props = mem_props;
        }
    }

    if (!s->phy_device) {
        LOG(ERROR, "no valid physical device found");
        return VK_ERROR_DEVICE_LOST;
    }

    LOG(DEBUG, "select physical device: %s, graphics queue: %d, present queue: %d",
        s->phy_device_props.deviceName, s->graphics_queue_index, s->present_queue_index);

    struct bstr *type = ngli_bstr_create();
    struct bstr *props = ngli_bstr_create();
    if (!type || !props) {
        ngli_bstr_freep(&type);
        ngli_bstr_freep(&props);
        return NGL_ERROR_MEMORY;
    }
    LOG(DEBUG, "available memory types:");
    for (uint32_t i = 0; i < s->phydev_mem_props.memoryTypeCount; i++) {
        get_memory_property_flags_str(type, 1 << i);
        get_memory_property_flags_str(props, s->phydev_mem_props.memoryTypes[i].propertyFlags);
        LOG(DEBUG, "\t%s:\t%s", ngli_bstr_strptr(type), ngli_bstr_strptr(props));
        ngli_bstr_clear(type);
        ngli_bstr_clear(props);
    }
    ngli_bstr_freep(&type);
    ngli_bstr_freep(&props);

    return VK_SUCCESS;
}

static VkResult enumerate_extensions(struct vkcontext *s)
{
    VkResult res = vkEnumerateDeviceExtensionProperties(s->phy_device, NULL, &s->nb_device_extensions, NULL);
    if (res != VK_SUCCESS)
        return res;

    s->device_extensions = ngli_calloc(s->nb_device_extensions, sizeof(*s->device_extensions));
    if (!s->device_extensions)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumerateDeviceExtensionProperties(s->phy_device, NULL, &s->nb_device_extensions, s->device_extensions);
    if (res != VK_SUCCESS)
        return res;

    LOG(DEBUG, "available device extensions:");
    for (uint32_t i = 0; i < s->nb_device_extensions; i++) {
        LOG(DEBUG, "  %d/%d: %s v%d", i+1, s->nb_device_extensions,
            s->device_extensions[i].extensionName, s->device_extensions[i].specVersion);
    }

    return VK_SUCCESS;
}

static VkResult create_device(struct vkcontext *s)
{
    int nb_queues = 0;
    float queue_priority = 1.0;
    VkDeviceQueueCreateInfo queues_create_info[2];

    const VkDeviceQueueCreateInfo graphics_queue_create_info = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = s->graphics_queue_index,
        .queueCount       = 1,
        .pQueuePriorities = &queue_priority,
    };
    queues_create_info[nb_queues++] = graphics_queue_create_info;

    if (s->present_queue_index != -1 &&
        s->graphics_queue_index != s->present_queue_index) {
        const VkDeviceQueueCreateInfo present_queue_create_info = {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = s->present_queue_index,
            .queueCount       = 1,
            .pQueuePriorities = &queue_priority,
        };
        queues_create_info[nb_queues++] = present_queue_create_info;
    }

    VkPhysicalDeviceFeatures dev_features = {0};

#define ENABLE_FEATURE(feature, mandatory) do {                                  \
    if (s->dev_features.feature) {                                               \
        LOG(DEBUG, "optional feature " #feature " is not supported by device");  \
        dev_features.feature = VK_TRUE;                                          \
    } else if (mandatory) {                                                      \
        LOG(ERROR, "mandatory feature " #feature " is not supported by device"); \
        return VK_ERROR_FEATURE_NOT_PRESENT;                                     \
    }                                                                            \
} while (0)                                                                      \

    ENABLE_FEATURE(samplerAnisotropy, 0);
    ENABLE_FEATURE(vertexPipelineStoresAndAtomics, 0);
    ENABLE_FEATURE(fragmentStoresAndAtomics, 0);
    ENABLE_FEATURE(shaderStorageImageExtendedFormats, 0);

#undef ENABLE_FEATURE

    struct darray enabled_extensions;
    ngli_darray_init(&enabled_extensions, sizeof(const char *), 0);

    static const char *mandatory_device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_IOS_MVK)
        VK_EXT_METAL_OBJECTS_EXTENSION_NAME,
#endif
    };

    for (size_t i = 0; i < NGLI_ARRAY_NB(mandatory_device_extensions); i++) {
        if (!ngli_darray_push(&enabled_extensions, &mandatory_device_extensions[i])) {
            ngli_darray_reset(&enabled_extensions);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    static const char *optional_device_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
#if defined(TARGET_ANDROID)
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
#endif
    };

    for (size_t i = 0; i < NGLI_ARRAY_NB(optional_device_extensions); i++) {
        if (ngli_vkcontext_has_extension(s, optional_device_extensions[i], 1)) {
            if (!ngli_darray_push(&enabled_extensions, &optional_device_extensions[i])) {
                ngli_darray_reset(&enabled_extensions);
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }

    VkPhysicalDeviceFeatures2 dev_features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = dev_features,
    };

    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
        .samplerYcbcrConversion = 1,
    };

    if (ngli_vkcontext_has_extension(s, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, 1))
        dev_features2.pNext = &ycbcr_features;

    const VkDeviceCreateInfo device_create_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &dev_features2,
        .pQueueCreateInfos       = queues_create_info,
        .queueCreateInfoCount    = nb_queues,
        .enabledExtensionCount   = (uint32_t)ngli_darray_count(&enabled_extensions),
        .ppEnabledExtensionNames = ngli_darray_data(&enabled_extensions),
    };
    VkResult res = vkCreateDevice(s->phy_device, &device_create_info, NULL, &s->device);

    ngli_darray_reset(&enabled_extensions);

    if (res != VK_SUCCESS)
        return res;

    vkGetDeviceQueue(s->device, s->graphics_queue_index, 0, &s->graphic_queue);
    if (s->present_queue_index != -1)
        vkGetDeviceQueue(s->device, s->present_queue_index, 0, &s->present_queue);

    return VK_SUCCESS;
}

VkFormat ngli_vkcontext_find_supported_format(struct vkcontext *s, const VkFormat *formats,
                                              VkImageTiling tiling, VkFormatFeatureFlags features)
{
    uint32_t i = 0;
    while (formats[i]) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(s->phy_device, formats[i], &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR &&
            NGLI_HAS_ALL_FLAGS(properties.linearTilingFeatures, features))
            return formats[i];
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            NGLI_HAS_ALL_FLAGS(properties.optimalTilingFeatures, features))
            return formats[i];
        i++;
    }
    return VK_FORMAT_UNDEFINED;
}

int ngli_vkcontext_find_memory_type(struct vkcontext *s, uint32_t type, VkMemoryPropertyFlags props)
{
    for (uint32_t i = 0; i < s->phydev_mem_props.memoryTypeCount; i++)
        if ((type & (1 << i)) && NGLI_HAS_ALL_FLAGS(s->phydev_mem_props.memoryTypes[i].propertyFlags, props))
            return i;
    return NGL_ERROR_GRAPHICS_UNSUPPORTED;
}

static VkResult query_swapchain_support(struct vkcontext *s)
{
    if (!s->surface)
        return VK_SUCCESS;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->phy_device, s->surface, &s->surface_caps);

    vkGetPhysicalDeviceSurfaceFormatsKHR(s->phy_device, s->surface, &s->nb_surface_formats, NULL);
    if (!s->nb_surface_formats)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;

    s->surface_formats = ngli_calloc(s->nb_surface_formats, sizeof(*s->surface_formats));
    if (!s->surface_formats)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s->phy_device, s->surface, &s->nb_surface_formats, s->surface_formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(s->phy_device, s->surface, &s->nb_present_modes, NULL);
    if (!s->nb_present_modes)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;

    s->present_modes = ngli_calloc(s->nb_present_modes, sizeof(*s->present_modes));
    if (!s->present_modes)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkGetPhysicalDeviceSurfacePresentModesKHR(s->phy_device, s->surface, &s->nb_present_modes, s->present_modes);

    return VK_SUCCESS;
}

VkBool32 ngli_vkcontext_support_present_mode(const struct vkcontext *s, VkPresentModeKHR mode)
{
    for (uint32_t i = 0; i < s->nb_present_modes; i++) {
        if (s->present_modes[i] == mode) {
            return VK_TRUE;
        }
    }
    return VK_FALSE;
}

static enum ngpu_format ngli_format_from_vk_format(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D32_SFLOAT:         return NGPU_FORMAT_D32_SFLOAT;
    case VK_FORMAT_D16_UNORM:          return NGPU_FORMAT_D16_UNORM;
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return NGPU_FORMAT_D32_SFLOAT_S8_UINT;
    case VK_FORMAT_D24_UNORM_S8_UINT:  return NGPU_FORMAT_D24_UNORM_S8_UINT;
    default:
        ngli_assert(0);
    }
}

static VkResult select_preferred_formats(struct vkcontext *s)
{
    const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkFormat depth_stencil_formats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        0
    };
    VkFormat format = ngli_vkcontext_find_supported_format(s, depth_stencil_formats, tiling, features);
    if (!format)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    s->preferred_depth_stencil_format = ngli_format_from_vk_format(format);

    const VkFormat depth_formats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        0
    };
    format = ngli_vkcontext_find_supported_format(s, depth_formats, tiling, features);
    if (!format)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    s->preferred_depth_format = ngli_format_from_vk_format(format);

    return VK_SUCCESS;
}

struct vk_function {
    const char *name;
    size_t offset;
    int device;
};

#define DECLARE_FUNC(n, d) {                 \
    .name = "vk" #n,                         \
    .offset = offsetof(struct vkcontext, n), \
    .device = d,                             \
}                                            \

struct vk_extension {
    const char *name;
    int device;
    const struct vk_function *functions;
} vk_extensions[] = {
    {
        .name = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        .device = 1,
        .functions = (const struct vk_function[]) {
            DECLARE_FUNC(GetMemoryFdKHR, 1),
            DECLARE_FUNC(GetMemoryFdPropertiesKHR, 1),
            {0},
        },
    }, {
        .name = VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
        .device = 1,
        .functions = (const struct vk_function[]) {
            DECLARE_FUNC(GetRefreshCycleDurationGOOGLE, 1),
            DECLARE_FUNC(GetPastPresentationTimingGOOGLE, 1),
            {0},
        },
    }, {
        .name = VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        .device = 1,
        .functions = (const struct vk_function[]) {
            DECLARE_FUNC(CreateSamplerYcbcrConversionKHR, 1),
            DECLARE_FUNC(DestroySamplerYcbcrConversionKHR, 1),
            {0},
        },
    },
#if defined(TARGET_ANDROID)
    {
        .name = VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        .device = 1,
        .functions = (const struct vk_function[]) {
            DECLARE_FUNC(GetAndroidHardwareBufferPropertiesANDROID, 1),
            DECLARE_FUNC(GetMemoryAndroidHardwareBufferANDROID, 1),
            {0},
        },
    },
#endif
};

static int load_function(struct vkcontext *s, const struct vk_function *func)
{
    PFN_vkVoidFunction *func_ptr = (void *)((uintptr_t)s + func->offset);
    if (func->device)
        *func_ptr = vkGetDeviceProcAddr(s->device, func->name);
    else
        *func_ptr = vkGetInstanceProcAddr(s->instance, func->name);

    if (!*func_ptr)
        return 0;

    return 1;
}

static VkResult load_functions(struct vkcontext *s)
{
    for (size_t i = 0; i < NGLI_ARRAY_NB(vk_extensions); i++) {
        struct vk_extension *ext = &vk_extensions[i];
        if (!ngli_vkcontext_has_extension(s, ext->name, ext->device))
            continue;
        for (const struct vk_function *func = ext->functions; func && func->name; func++) {
            if (!load_function(s, func)) {
                LOG(ERROR, "could not load %s() required by extension %s",
                    func->name, ext->name);
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }
    }
    return VK_SUCCESS;
}

struct vkcontext *ngli_vkcontext_create(void)
{
    struct vkcontext *s = ngli_calloc(1, sizeof(*s));
    return s;
}

VkResult ngli_vkcontext_init(struct vkcontext *s, const struct ngl_config *config)
{
    VkResult res = create_instance(s, config->platform, config->debug);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "failed to create instance: %s", ngli_vk_res2str(res));
        return res;
    }

    res = create_window_surface(s, config);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "failed to create window surface: %s", ngli_vk_res2str(res));
        return res;
    }

    res = enumerate_physical_devices(s, config);
    if (res != VK_SUCCESS)
        return res;

    res = select_physical_device(s, config);
    if (res != VK_SUCCESS)
        return res;

    res = enumerate_extensions(s);
    if (res != VK_SUCCESS)
        return res;

    res = create_device(s);
    if (res != VK_SUCCESS)
        return res;

    res = load_functions(s);
    if (res != VK_SUCCESS)
        return res;

    res = query_swapchain_support(s);
    if (res != VK_SUCCESS)
        return res;

    res = select_preferred_formats(s);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

void *ngli_vkcontext_get_proc_addr(struct vkcontext *s, const char *name)
{
    return vkGetInstanceProcAddr(s->instance, name);
}

int ngli_vkcontext_has_extension(const struct vkcontext *s, const char *name, int device)
{
    uint32_t nb_extensions = device ? s->nb_device_extensions : s->nb_extensions;
    VkExtensionProperties *extensions = device ? s->device_extensions : s->extensions;
    for (uint32_t i = 0; i < nb_extensions; i++) {
        if (!strcmp(extensions[i].extensionName, name))
            return 1;
    }
    return 0;
}

void ngli_vkcontext_freep(struct vkcontext **sp)
{
    struct vkcontext *s = *sp;
    if (!s)
        return;

    if (s->device) {
        vkDeviceWaitIdle(s->device);
        vkDestroyDevice(s->device, NULL);
    }

    if (s->surface)
        vkDestroySurfaceKHR(s->instance, s->surface, NULL);

    if (s->debug_callback) {
        VK_LOAD_FUNC(s->instance, DestroyDebugUtilsMessengerEXT);
        if (DestroyDebugUtilsMessengerEXT)
            DestroyDebugUtilsMessengerEXT(s->instance, s->debug_callback, NULL);
    }

    ngli_freep(&s->present_modes);
    ngli_freep(&s->surface_formats);
    ngli_freep(&s->device_extensions);
    ngli_freep(&s->phy_devices);
    ngli_freep(&s->layers);
    ngli_freep(&s->extensions);

    vkDestroyInstance(s->instance, NULL);

#if defined(TARGET_LINUX)
    if (s->own_x11_display)
        XCloseDisplay(s->x11_display);
#endif

    ngli_freep(sp);
}
