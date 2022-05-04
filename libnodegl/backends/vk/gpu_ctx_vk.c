/*
 * Copyright 2018 GoPro Inc.
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

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <limits.h>

#include <vulkan/vulkan.h>

#include "glslang_utils.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"

#include "buffer_vk.h"
#include "format_vk.h"
#include "gpu_ctx_vk.h"
#include "pipeline_vk.h"
#include "program_vk.h"
#include "rendertarget_vk.h"
#include "texture_vk.h"
#include "vkcontext.h"
#include "vkutils.h"

#if DEBUG_GPU_CAPTURE
#include "gpu_capture.h"
#endif

static VkResult create_dummy_texture(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    s_priv->dummy_texture = ngli_texture_vk_create(s);
    if (!s_priv->dummy_texture)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    const struct texture_params params = {
        .type    = NGLI_TEXTURE_TYPE_2D,
        .format  = NGLI_FORMAT_R8G8B8A8_UNORM,
        .width   = 1,
        .height  = 1,
        .samples = 1,
        .usage   = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
                   NGLI_TEXTURE_USAGE_STORAGE_BIT |
                   NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT,
    };

    VkResult res = ngli_texture_vk_init(s_priv->dummy_texture, &params);
    if (res != VK_SUCCESS)
        return res;

    const uint8_t buf[4] = {0};
    return ngli_texture_vk_upload(s_priv->dummy_texture, buf, 0);
}

static void destroy_dummy_texture(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    ngli_texture_vk_freep(&s_priv->dummy_texture);
}

static VkResult create_texture(struct gpu_ctx *s, int format, int samples, int usage, struct texture **texturep)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    struct texture *texture = ngli_texture_vk_create(s);
    if (!texture)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    const struct texture_params params = {
        .type    = NGLI_TEXTURE_TYPE_2D,
        .format  = format,
        .width   = s_priv->width,
        .height  = s_priv->height,
        .samples = samples,
        .usage   = usage,
    };

    VkResult res = ngli_texture_vk_init(texture, &params);
    if (res != VK_SUCCESS) {
        ngli_texture_vk_freep(&texture);
        return res;
    }

    *texturep = texture;

    return VK_SUCCESS;
}

static VkResult create_rendertarget(struct gpu_ctx *s,
                                    struct texture *color,
                                    struct texture *resolve_color,
                                    struct texture *depth_stencil,
                                    int load_op,
                                    struct rendertarget **rendertargetp)
{
    const struct ngl_config *config = &s->config;

    struct rendertarget *rendertarget = ngli_rendertarget_vk_create(s);
    if (!rendertarget)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    const struct rendertarget_params params = {
        .width = config->width,
        .height = config->height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment     = color,
            .resolve_target = resolve_color,
            .load_op        = load_op,
            .clear_value[0] = config->clear_color[0],
            .clear_value[1] = config->clear_color[1],
            .clear_value[2] = config->clear_color[2],
            .clear_value[3] = config->clear_color[3],
            .store_op       = NGLI_STORE_OP_STORE,
        },
        .depth_stencil = {
            .attachment = depth_stencil,
            .load_op    = load_op,
            .store_op   = NGLI_STORE_OP_STORE,
        },
    };

    VkResult res = ngli_rendertarget_vk_init(rendertarget, &params);
    if (res != VK_SUCCESS) {
        ngli_rendertarget_vk_freep(&rendertarget);
        return res;
    }

    *rendertargetp = rendertarget;

    return VK_SUCCESS;
}

#define COLOR_USAGE (NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT)
#define DEPTH_USAGE NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT

static VkResult create_render_resources(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    const struct ngl_config *config = &s->config;

    const int color_format = config->offscreen
                           ? NGLI_FORMAT_R8G8B8A8_UNORM
                           : ngli_format_vk_to_ngl(s_priv->surface_format.format);
    const int ds_format = vk->preferred_depth_stencil_format;

    const int nb_images = config->offscreen ? s_priv->nb_in_flight_frames : s_priv->nb_images;
    for (uint32_t i = 0; i < nb_images; i++) {
        struct texture *color = NULL;
        if (config->offscreen) {
            VkResult res = create_texture(s, color_format, 0, COLOR_USAGE, &color);
            if (res != VK_SUCCESS)
                return res;
        } else {
            color = ngli_texture_vk_create(s);
            if (!color)
                return VK_ERROR_OUT_OF_HOST_MEMORY;

            const struct texture_params params = {
                .type             = NGLI_TEXTURE_TYPE_2D,
                .format           = color_format,
                .width            = s_priv->width,
                .height           = s_priv->height,
                .usage            = COLOR_USAGE,
                .external_storage = 1,
            };

            VkResult res = ngli_texture_vk_wrap(color, &params, s_priv->images[i], VK_IMAGE_LAYOUT_UNDEFINED);
            if (res != VK_SUCCESS) {
                ngli_texture_vk_freep(&color);
                return res;
            }
        }

        if (!ngli_darray_push(&s_priv->colors, &color)) {
            ngli_texture_vk_freep(&color);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        struct texture *depth_stencil = NULL;
        VkResult res = create_texture(s, ds_format, config->samples, DEPTH_USAGE, &depth_stencil);
        if (res != VK_SUCCESS)
            return res;

        if (!ngli_darray_push(&s_priv->depth_stencils, &depth_stencil)) {
            ngli_texture_vk_freep(&depth_stencil);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        struct texture *ms_color = NULL;
        if (config->samples) {
            res = create_texture(s, color_format, config->samples, COLOR_USAGE, &ms_color);
            if (res != VK_SUCCESS)
                return res;

            if (!ngli_darray_push(&s_priv->ms_colors, &ms_color)) {
                ngli_texture_vk_freep(&ms_color);
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        struct texture *target_color = ms_color ? ms_color : color;
        struct texture *resolve_color = ms_color ? color : NULL;

        struct rendertarget *rt = NULL;
        res = create_rendertarget(s, target_color, resolve_color, depth_stencil, NGLI_LOAD_OP_CLEAR, &rt);
        if (res != VK_SUCCESS)
            return res;

        if (!ngli_darray_push(&s_priv->rts, &rt)) {
            ngli_rendertarget_vk_freep(&rt);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        struct rendertarget *rt_load = NULL;
        res = create_rendertarget(s, target_color, resolve_color, depth_stencil, NGLI_LOAD_OP_LOAD, &rt_load);
        if (res != VK_SUCCESS)
            return res;

        if (!ngli_darray_push(&s_priv->rts_load, &rt_load)) {
            ngli_rendertarget_vk_freep(&rt_load);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (config->offscreen) {
        s_priv->capture_buffer = ngli_buffer_vk_create(s);
        if (!s_priv->capture_buffer)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        s_priv->capture_buffer_size = s_priv->width * s_priv->height * ngli_format_get_bytes_per_pixel(color_format);
        VkResult res = ngli_buffer_vk_init(s_priv->capture_buffer,
                                           s_priv->capture_buffer_size,
                                           NGLI_BUFFER_USAGE_MAP_READ |
                                           NGLI_BUFFER_USAGE_TRANSFER_DST_BIT);
        if (res != VK_SUCCESS)
            return res;

        res = ngli_buffer_vk_map(s_priv->capture_buffer, s_priv->capture_buffer_size, 0, &s_priv->mapped_data);
        if (res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
}

static void destroy_render_resources(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    struct texture **colors = ngli_darray_data(&s_priv->colors);
    for (int i = 0; i < ngli_darray_count(&s_priv->colors); i++)
        ngli_texture_vk_freep(&colors[i]);
    ngli_darray_reset(&s_priv->colors);

    struct texture **ms_colors = ngli_darray_data(&s_priv->ms_colors);
    for (int i = 0; i < ngli_darray_count(&s_priv->ms_colors); i++)
        ngli_texture_vk_freep(&ms_colors[i]);
    ngli_darray_reset(&s_priv->ms_colors);

    struct texture **depth_stencils = ngli_darray_data(&s_priv->depth_stencils);
    for (int i = 0; i < ngli_darray_count(&s_priv->depth_stencils); i++)
        ngli_texture_vk_freep(&depth_stencils[i]);
    ngli_darray_reset(&s_priv->depth_stencils);

    struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
    for (int i = 0; i < ngli_darray_count(&s_priv->rts); i++)
        ngli_rendertarget_vk_freep(&rts[i]);
    ngli_darray_reset(&s_priv->rts);

    struct rendertarget **rts_load = ngli_darray_data(&s_priv->rts_load);
    for (int i = 0; i < ngli_darray_count(&s_priv->rts_load); i++)
        ngli_rendertarget_vk_freep(&rts_load[i]);
    ngli_darray_reset(&s_priv->rts_load);

    if (s_priv->mapped_data) {
        ngli_buffer_unmap(s_priv->capture_buffer);
        s_priv->mapped_data = NULL;
    }
    ngli_buffer_vk_freep(&s_priv->capture_buffer);
}

static VkResult create_query_pool(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    const VkQueryPoolCreateInfo create_info = {
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };

    return vkCreateQueryPool(vk->device, &create_info, NULL, &s_priv->query_pool);
}

static void destroy_query_pool(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    vkDestroyQueryPool(vk->device, s_priv->query_pool, NULL);
}

static VkResult create_command_pool_and_buffers(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    const VkCommandPoolCreateInfo cmd_pool_create_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk->graphics_queue_index,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    VkResult res = vkCreateCommandPool(vk->device, &cmd_pool_create_info, NULL, &s_priv->cmd_pool);
    if (res != VK_SUCCESS)
        return res;

    s_priv->cmds = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(struct cmd_vk *));
    s_priv->update_cmds = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(struct cmd_vk *));
    if (!s_priv->cmds || !s_priv->update_cmds)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (int i = 0; i < s_priv->nb_in_flight_frames; i++) {
        s_priv->cmds[i] = ngli_cmd_vk_create(s);
        if (!s_priv->cmds[i])
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        res = ngli_cmd_vk_init(s_priv->cmds[i], 0);
        if (res != VK_SUCCESS)
            return res;

        s_priv->update_cmds[i] = ngli_cmd_vk_create(s);
        if (!s_priv->update_cmds[i])
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        res = ngli_cmd_vk_init(s_priv->update_cmds[i], 0);
        if (res != VK_SUCCESS)
            return res;
    }

    ngli_darray_init(&s_priv->pending_cmds, sizeof(struct vmd_vk *), 0);

    return VK_SUCCESS;
}

static void destroy_command_pool_and_buffers(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    if (s_priv->cmds) {
        for (int i = 0; i < s_priv->nb_in_flight_frames; i++)
            ngli_cmd_vk_freep(&s_priv->cmds[i]);
        ngli_freep(&s_priv->cmds);
    }

    if (s_priv->update_cmds) {
        for (int i = 0; i < s_priv->nb_in_flight_frames; i++)
            ngli_cmd_vk_freep(&s_priv->update_cmds[i]);
        ngli_freep(&s_priv->update_cmds);
    }

    vkDestroyCommandPool(vk->device, s_priv->cmd_pool, NULL);

    ngli_darray_reset(&s_priv->pending_cmds);
}

static VkResult create_semaphores(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    s_priv->image_avail_sems = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!s_priv->image_avail_sems)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    s_priv->update_finished_sems = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!s_priv->update_finished_sems)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    s_priv->render_finished_sems = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!s_priv->render_finished_sems)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    const VkSemaphoreCreateInfo sem_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkResult res;
    for (int i = 0; i < s_priv->nb_in_flight_frames; i++) {
        if ((res = vkCreateSemaphore(vk->device, &sem_create_info, NULL,
                                     &s_priv->image_avail_sems[i])) != VK_SUCCESS ||
            (res = vkCreateSemaphore(vk->device, &sem_create_info, NULL,
                                     &s_priv->update_finished_sems[i])) != VK_SUCCESS ||
            (res = vkCreateSemaphore(vk->device, &sem_create_info, NULL,
                                     &s_priv->render_finished_sems[i])) != VK_SUCCESS) {
            return res;
        }
    }

    ngli_darray_init(&s_priv->pending_wait_sems, sizeof(VkSemaphore), 0);

    return VK_SUCCESS;
}

static void destroy_semaphores(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    if (s_priv->update_finished_sems) {
        for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++)
            vkDestroySemaphore(vk->device, s_priv->update_finished_sems[i], NULL);
        ngli_freep(&s_priv->update_finished_sems);
    }

    if (s_priv->render_finished_sems) {
        for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++)
            vkDestroySemaphore(vk->device, s_priv->render_finished_sems[i], NULL);
        ngli_freep(&s_priv->render_finished_sems);
    }

    if (s_priv->image_avail_sems) {
        for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++)
            vkDestroySemaphore(vk->device, s_priv->image_avail_sems[i], NULL);
        ngli_freep(&s_priv->image_avail_sems);
    }

    ngli_darray_reset(&s_priv->pending_wait_sems);
}

static VkResult select_swapchain_surface_format(const struct vkcontext *vk, VkSurfaceFormatKHR *format)
{
    LOG(DEBUG, "available surface formats:");
    for (uint32_t i = 0; i < vk->nb_surface_formats; i++) {
        LOG(DEBUG, "\tformat: %d, colorspace: %d",
            vk->surface_formats[i].format,
            vk->surface_formats[i].colorSpace);
    }

    for (uint32_t i = 0; i < vk->nb_surface_formats; i++) {
        switch (vk->surface_formats[i].format) {
        case VK_FORMAT_UNDEFINED:
            *format = (VkSurfaceFormatKHR) {
                .format     = VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            };
            return VK_SUCCESS;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
            if (vk->surface_formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
                *format = vk->surface_formats[i];
            return VK_SUCCESS;
        default:
            break;
        }
    }
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

static const char *get_vk_present_mode_str(VkPresentModeKHR mode)
{
    switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:                 return "immediate";
    case VK_PRESENT_MODE_MAILBOX_KHR:                   return "mailbox";
    case VK_PRESENT_MODE_FIFO_KHR:                      return "fifo";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:              return "fifo_relaxed";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:     return "shared_demand_refresh";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR: return "shared_continuous_refresh";
    default:                                            return "unknown";
    }
}

static VkPresentModeKHR select_swapchain_present_mode(struct vkcontext *vk, int swap_interval)
{
    LOG(DEBUG, "available surface present modes:");
    for (uint32_t i = 0; i < vk->nb_present_modes; i++) {
        LOG(DEBUG, "\tmode: %s", get_vk_present_mode_str(vk->present_modes[i]));
    }

    if (!swap_interval) {
        /*
         * When vsync is disabled use VK_PRESENT_MODE_IMMEDIATE_KHR if
         * available, otherwise fall back to VK_PRESENT_MODE_FIFO_KHR which is
         * guaranteed to be supported.
         */
        if (vk->support_present_mode_immediate)
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkCompositeAlphaFlagBitsKHR select_swapchain_composite_alpha(struct vkcontext *vk)
{
    if (vk->surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    if (vk->surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ngli_assert(0);
}

static VkResult create_swapchain(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    struct ngl_config *config = &s->config;

    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->phy_device, vk->surface, &s_priv->surface_caps);
    if (res != VK_SUCCESS)
        return res;

    res = select_swapchain_surface_format(vk, &s_priv->surface_format);
    if (res != VK_SUCCESS)
        return res;

    const VkSurfaceCapabilitiesKHR caps = s_priv->surface_caps;
    s_priv->present_mode = select_swapchain_present_mode(vk, config->swap_interval);
    s_priv->width  = NGLI_CLAMP(s_priv->width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
    s_priv->height = NGLI_CLAMP(s_priv->height, caps.minImageExtent.height, caps.maxImageExtent.height),
    config->width = s_priv->width;
    config->height = s_priv->height;
    LOG(DEBUG, "current extent: %dx%d", s_priv->width, s_priv->height);

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount && img_count > caps.maxImageCount)
        img_count = caps.maxImageCount;
    LOG(DEBUG, "swapchain image count: %d [%d-%d]", img_count, caps.minImageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = vk->surface,
        .minImageCount    = img_count,
        .imageFormat      = s_priv->surface_format.format,
        .imageColorSpace  = s_priv->surface_format.colorSpace,
        .imageExtent      = {
            .width  = s_priv->width,
            .height = s_priv->height
        },
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = select_swapchain_composite_alpha(vk),
        .presentMode      = s_priv->present_mode,
        .clipped          = VK_TRUE,
    };

    const uint32_t queue_family_indices[2] = {
        vk->graphics_queue_index,
        vk->present_queue_index,
    };
    if (queue_family_indices[0] != queue_family_indices[1]) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = NGLI_ARRAY_NB(queue_family_indices);
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }

    res = vkCreateSwapchainKHR(vk->device, &swapchain_create_info, NULL, &s_priv->swapchain);
    if (res != VK_SUCCESS)
        return res;

    res = vkGetSwapchainImagesKHR(vk->device, s_priv->swapchain, &s_priv->nb_images, NULL);
    if (res != VK_SUCCESS)
        return res;

    VkImage *images = ngli_realloc(s_priv->images, s_priv->nb_images * sizeof(VkImage));
    if (!images)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    s_priv->images = images;

    res = vkGetSwapchainImagesKHR(vk->device, s_priv->swapchain, &s_priv->nb_images, s_priv->images);
    if (res != VK_SUCCESS)
        return res;

    return res;
}

static void destroy_swapchain(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    vkDestroySwapchainKHR(vk->device, s_priv->swapchain, NULL);
    ngli_freep(&s_priv->images);
    s_priv->nb_images = 0;
}

static VkResult recreate_swapchain(struct gpu_ctx *gpu_ctx, struct vkcontext *vk)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)gpu_ctx;

    VkResult res = vkDeviceWaitIdle(vk->device);
    if (res != VK_SUCCESS)
        return res;

    VkSurfaceCapabilitiesKHR surface_caps;
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->phy_device, vk->surface, &surface_caps);
    if (res != VK_SUCCESS)
        return res;

    /*
     * According to the Vulkan specification, on Windows, the window size may
     * become (0, 0) if the window is minimized and so a swapchain cannot be
     * created until the size changes.
     * See: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap33.html#platformCreateSurface_win32
     */
    if (!surface_caps.currentExtent.width || !surface_caps.currentExtent.height)
        return VK_SUCCESS;

    struct texture **colors = ngli_darray_data(&s_priv->colors);
    for (int i = 0; i < ngli_darray_count(&s_priv->colors); i++)
        ngli_texture_vk_freep(&colors[i]);
    ngli_darray_clear(&s_priv->colors);

    struct texture **ms_colors = ngli_darray_data(&s_priv->ms_colors);
    for (int i = 0; i < ngli_darray_count(&s_priv->ms_colors); i++)
        ngli_texture_vk_freep(&ms_colors[i]);
    ngli_darray_clear(&s_priv->ms_colors);

    struct texture **depth_stencils = ngli_darray_data(&s_priv->depth_stencils);
    for (int i = 0; i < ngli_darray_count(&s_priv->depth_stencils); i++)
        ngli_texture_vk_freep(&depth_stencils[i]);
    ngli_darray_clear(&s_priv->depth_stencils);

    struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
    for (int i = 0; i < ngli_darray_count(&s_priv->rts); i++)
        ngli_rendertarget_vk_freep(&rts[i]);
    ngli_darray_clear(&s_priv->rts);

    struct rendertarget **rts_load = ngli_darray_data(&s_priv->rts_load);
    for (int i = 0; i < ngli_darray_count(&s_priv->rts_load); i++)
        ngli_rendertarget_vk_freep(&rts_load[i]);
    ngli_darray_clear(&s_priv->rts_load);

    vkDestroySwapchainKHR(vk->device, s_priv->swapchain, NULL);
    s_priv->nb_images = 0;

    if ((res = create_swapchain(gpu_ctx)) != VK_SUCCESS ||
        (res = create_render_resources(gpu_ctx) != VK_SUCCESS))
        return res;

    return VK_SUCCESS;
}

static VkResult swapchain_acquire_image(struct gpu_ctx *s, uint32_t *image_index)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    if (s_priv->recreate_swapchain) {
        VkResult res = recreate_swapchain(s, vk);
        if (res != VK_SUCCESS)
            return res;
        s_priv->recreate_swapchain = 0;
    }

    VkSemaphore sem = s_priv->image_avail_sems[s_priv->cur_frame_index];
    VkResult res = vkAcquireNextImageKHR(vk->device, s_priv->swapchain,
                                         UINT64_MAX, sem, VK_NULL_HANDLE, image_index);
    switch (res) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        res = recreate_swapchain(s, vk);
        if (res != VK_SUCCESS)
            return res;

        res = vkAcquireNextImageKHR(vk->device, s_priv->swapchain,
                                    UINT64_MAX, sem, VK_NULL_HANDLE, image_index);
        if (res != VK_SUCCESS)
            return res;
        break;
    default:
        LOG(ERROR, "failed to acquire swapchain image: %s", ngli_vk_res2str(res));
        return res;
    }

    res = ngli_cmd_vk_add_wait_sem(s_priv->cur_cmd, &sem, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    if (res != VK_SUCCESS)
        return res;

    res = ngli_cmd_vk_add_signal_sem(s_priv->cur_cmd, &s_priv->render_finished_sems[s_priv->cur_frame_index]);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

static VkResult swapchain_present_buffer(struct gpu_ctx *s, double t)
{
    const struct ngl_config *config = &s->config;
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    VkSemaphore sem = s_priv->render_finished_sems[s_priv->cur_frame_index];

    VkPresentInfoKHR present_info = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &sem,
        .swapchainCount     = 1,
        .pSwapchains        = &s_priv->swapchain,
        .pImageIndices      = &s_priv->cur_image_index,
    };

    VkPresentTimeGOOGLE present_time = {
        .presentID = 0,
    };

    VkPresentTimesInfoGOOGLE present_time_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE,
        .swapchainCount = 1,
        .pTimes = &present_time,
    };

    if (config->set_surface_pts) {
        /*
         * On first frame, compute the presentation time offset based on
         * ngli_gettime_relative() result converted to ns. This is mandatory as
         * setting desiredPresentTime to 0 specifies that the presentation
         * engine may display the image at any time. In practice, when
         * desiredPresentTime is set to 0 for the first frame, the Mediacodec
         * encoder providing the surface only encodes the first frame and
         * discards the others.
         */
        if (s_priv->present_time_offset == 0)
            s_priv->present_time_offset = ngli_gettime_relative() * 1000;
        present_time.desiredPresentTime = s_priv->present_time_offset + t * 1000000000LL;
        present_info.pNext = &present_time_info;
    }

    VkResult res = vkQueuePresentKHR(vk->present_queue, &present_info);
    switch (res) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        /*
         * Silently ignore this error since the swapchain will be re-created on
         * the next frame.
         */
        break;
    default:
        LOG(ERROR, "failed to present image %s", ngli_vk_res2str(res));
        return res;
    }

    return VK_SUCCESS;
}

static struct gpu_ctx *vk_create(const struct ngl_config *config)
{
    struct gpu_ctx_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    return (struct gpu_ctx *)s;
}

static int get_max_supported_samples(const VkPhysicalDeviceLimits *limits)
{
    const int max_color_samples = ngli_vk_samples_to_ngl(limits->framebufferColorSampleCounts);
    const int max_depth_samples = ngli_vk_samples_to_ngl(limits->framebufferDepthSampleCounts);
    const int max_stencil_samples = ngli_vk_samples_to_ngl(limits->framebufferStencilSampleCounts);
    return NGLI_MIN(max_color_samples, NGLI_MIN(max_depth_samples, max_stencil_samples));
}

static void set_viewport_and_scissor(struct gpu_ctx *s, int width, int height, const int *viewport)
{
    if (viewport && viewport[2] > 0 && viewport[3] > 0) {
        ngli_gpu_ctx_set_viewport(s, viewport);
    } else {
        const int default_viewport[] = {0, 0, width, height};
        ngli_gpu_ctx_set_viewport(s, default_viewport);
    }

    const int scissor[] = {0, 0, width, height};
    ngli_gpu_ctx_set_scissor(s, scissor);
}

static int vk_init(struct gpu_ctx *s)
{
    const struct ngl_config *config = &s->config;
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    if (config->offscreen) {
        if (config->width <= 0 || config->height <= 0) {
            LOG(ERROR, "could not create offscreen context with invalid dimensions (%dx%d)",
                config->width, config->height);
            return NGL_ERROR_INVALID_ARG;
        }
    } else {
        if (config->capture_buffer) {
            LOG(ERROR, "capture_buffer is not supported by onscreen context");
            return NGL_ERROR_INVALID_ARG;
        }
    }

#if DEBUG_GPU_CAPTURE
    const char *var = getenv("NGL_GPU_CAPTURE");
    s->gpu_capture = var && !strcmp(var, "yes");
    if (s->gpu_capture) {
        s->gpu_capture_ctx = ngli_gpu_capture_ctx_create(s);
        if (!s->gpu_capture_ctx) {
            LOG(ERROR, "could not create GPU capture context");
            return NGL_ERROR_MEMORY;
        }
        int ret = ngli_gpu_capture_init(s->gpu_capture_ctx);
        if (ret < 0) {
            LOG(ERROR, "could not initialize GPU capture");
            s->gpu_capture = 0;
            return ret;
        }
    }
#endif

    ngli_darray_init(&s_priv->colors, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->ms_colors, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->depth_stencils, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->rts, sizeof(struct rendertarget *), 0);
    ngli_darray_init(&s_priv->rts_load, sizeof(struct rendertarget *), 0);

    s_priv->vkcontext = ngli_vkcontext_create();
    if (!s_priv->vkcontext)
        return NGL_ERROR_MEMORY;

    VkResult res = ngli_vkcontext_init(s_priv->vkcontext, config);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "unable to initialize Vulkan context: %s", ngli_vk_res2str(res));
        /*
         * Reset the failed vkcontext so if we do not end up calling vulkan
         * functions on a partially initialized vkcontext in
         * ngli_gpu_ctx_freep() / vk_destroy().
         */
        ngli_vkcontext_freep(&s_priv->vkcontext);
        return ngli_vk_res2ret(res);
    }

#if DEBUG_GPU_CAPTURE
    if (s->gpu_capture)
        ngli_gpu_capture_begin(s->gpu_capture_ctx);
#endif

    s->features = NGLI_FEATURE_COMPUTE |
                  NGLI_FEATURE_INSTANCED_DRAW |
                  NGLI_FEATURE_COLOR_RESOLVE |
                  NGLI_FEATURE_SHADER_TEXTURE_LOD |
                  NGLI_FEATURE_TEXTURE_3D |
                  NGLI_FEATURE_TEXTURE_CUBE_MAP |
                  NGLI_FEATURE_TEXTURE_NPOT |
                  NGLI_FEATURE_UINT_UNIFORMS |
                  NGLI_FEATURE_UNIFORM_BUFFER |
                  NGLI_FEATURE_STORAGE_BUFFER |
                  NGLI_FEATURE_BUFFER_MAP;

    struct vkcontext *vk = s_priv->vkcontext;
    const VkPhysicalDeviceLimits *limits = &vk->phy_device_props.limits;
    s->limits.max_color_attachments              = limits->maxColorAttachments;
    s->limits.max_texture_dimension_1d           = limits->maxImageDimension1D;
    s->limits.max_texture_dimension_2d           = limits->maxImageDimension2D;
    s->limits.max_texture_dimension_3d           = limits->maxImageDimension3D;
    s->limits.max_texture_dimension_cube         = limits->maxImageDimensionCube;
    s->limits.max_compute_work_group_count[0]    = limits->maxComputeWorkGroupCount[0];
    s->limits.max_compute_work_group_count[1]    = limits->maxComputeWorkGroupCount[1];
    s->limits.max_compute_work_group_count[2]    = limits->maxComputeWorkGroupCount[2];
    s->limits.max_compute_work_group_invocations = limits->maxComputeWorkGroupInvocations;
    s->limits.max_compute_work_group_size[0]     = limits->maxComputeWorkGroupSize[0];
    s->limits.max_compute_work_group_size[1]     = limits->maxComputeWorkGroupSize[1];
    s->limits.max_compute_work_group_size[2]     = limits->maxComputeWorkGroupSize[2];
    s->limits.max_compute_shared_memory_size     = limits->maxComputeSharedMemorySize;
    s->limits.max_draw_buffers                   = limits->maxColorAttachments;
    s->limits.max_samples                        = get_max_supported_samples(limits);
    /* max_texture_image_units is specific to the OpenGL backend and has no
     * direct Vulkan equivalent so use a sane default value */
    s->limits.max_texture_image_units            = 32;
    s->limits.max_uniform_block_size             = limits->maxUniformBufferRange;

    if (config->set_surface_pts &&
        !ngli_vkcontext_has_extension(vk, VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME, 1)) {
        LOG(ERROR, "context does not support setting surface pts: %s is not supported",
            VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    s_priv->width = config->width;
    s_priv->height = config->height;
    s_priv->nb_in_flight_frames = 1;

    int ret = ngli_glslang_init();
    if (ret < 0)
        return ret;

    res = create_query_pool(s);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    res = create_semaphores(s);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    res = create_command_pool_and_buffers(s);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    res = create_dummy_texture(s);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    if (config->offscreen) {
        if (config->capture_buffer_type != NGL_CAPTURE_BUFFER_TYPE_CPU) {
            LOG(ERROR, "unsupported capture buffer type");
            return NGL_ERROR_UNSUPPORTED;
        }
    } else {
        res = create_swapchain(s);
        if (res != VK_SUCCESS)
            return ngli_vk_res2ret(res);
    }

    res = create_render_resources(s);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    if (config->offscreen) {
        s_priv->default_rt_desc.samples               = config->samples;
        s_priv->default_rt_desc.nb_colors             = 1;
        s_priv->default_rt_desc.colors[0].format      = NGLI_FORMAT_R8G8B8A8_UNORM;
        s_priv->default_rt_desc.colors[0].resolve     = config->samples > 0 ? 1 : 0;
        s_priv->default_rt_desc.depth_stencil.format  = vk->preferred_depth_stencil_format;
        s_priv->default_rt_desc.depth_stencil.resolve = 0;
    } else {
        s_priv->default_rt_desc.samples               = config->samples;
        s_priv->default_rt_desc.nb_colors             = 1;
        s_priv->default_rt_desc.colors[0].format      = ngli_format_vk_to_ngl(s_priv->surface_format.format);
        s_priv->default_rt_desc.colors[0].resolve     = config->samples > 0 ? 1 : 0;
        s_priv->default_rt_desc.depth_stencil.format  = vk->preferred_depth_stencil_format;
        s_priv->default_rt_desc.depth_stencil.resolve = 0;
    }

    set_viewport_and_scissor(s, config->width, config->height, config->viewport);

    return 0;
}

static int vk_resize(struct gpu_ctx *s, int width, int height, const int *viewport)
{
    const struct ngl_config *config = &s->config;
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    if (config->offscreen) {
        LOG(ERROR, "resize operation is not supported by offscreen context");
        return NGL_ERROR_UNSUPPORTED;
    }

    s_priv->recreate_swapchain = 1;
    s_priv->width = width;
    s_priv->height = height;
    set_viewport_and_scissor(s, width, height, viewport);

    return 0;
}

static int vk_set_capture_buffer(struct gpu_ctx *s, void *capture_buffer)
{
    struct ngl_config *config = &s->config;

    if (!config->offscreen) {
        LOG(ERROR, "capture_buffer is not supported by onscreen context");
        return NGL_ERROR_UNSUPPORTED;
    }

    config->capture_buffer = capture_buffer;
    return 0;
}

static int vk_begin_update(struct gpu_ctx *s, double t)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    struct cmd_vk **cmds = ngli_darray_data(&s_priv->pending_cmds);
    for (int i = 0; i < ngli_darray_count(&s_priv->pending_cmds); i++) {
        VkResult res = ngli_cmd_vk_wait(cmds[i]);
        if (res != VK_SUCCESS)
            return res;
    }
    ngli_darray_clear(&s_priv->pending_cmds);

    struct cmd_vk *cmd_vk = s_priv->cmds[s_priv->cur_frame_index];
    VkResult res = ngli_cmd_vk_wait(cmd_vk);
    if (res != VK_SUCCESS)
        return res;

    s_priv->cur_frame_index = (s_priv->cur_frame_index + 1) % s_priv->nb_in_flight_frames;

    s_priv->cur_cmd = s_priv->update_cmds[s_priv->cur_frame_index];
    return ngli_cmd_vk_begin(s_priv->cur_cmd);
}

static int vk_end_update(struct gpu_ctx *s, double t)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    VkSemaphore update_finished_sem = s_priv->update_finished_sems[s_priv->cur_frame_index];
    VkResult res = ngli_cmd_vk_add_signal_sem(s_priv->cur_cmd, &update_finished_sem);
    if (res != VK_SUCCESS)
        return res;

    res = ngli_cmd_vk_submit(s_priv->cur_cmd);
    if (res != VK_SUCCESS)
        return res;

    if (!ngli_darray_push(&s_priv->pending_wait_sems, &update_finished_sem))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    s_priv->cur_cmd = NULL;

    return 0;
}

static int vk_begin_draw(struct gpu_ctx *s, double t)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    const struct ngl_config *config = &s->config;

    s_priv->cur_cmd = s_priv->cmds[s_priv->cur_frame_index];
    VkResult res = ngli_cmd_vk_begin(s_priv->cur_cmd);
    if (res != VK_SUCCESS)
        return res;

    VkSemaphore *wait_sems = ngli_darray_data(&s_priv->pending_wait_sems);
    for (int i = 0; i < ngli_darray_count(&s_priv->pending_wait_sems); i++) {
        res = ngli_cmd_vk_add_wait_sem(s_priv->cur_cmd,
                                       &wait_sems[i],
                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        if (res != VK_SUCCESS)
            return res;

    }
    ngli_darray_clear(&s_priv->pending_wait_sems);

    if (config->offscreen) {
        struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
        s_priv->default_rt = rts[s_priv->cur_frame_index];

        struct rendertarget **rts_load = ngli_darray_data(&s_priv->rts_load);
        s_priv->default_rt_load = rts_load[s_priv->cur_frame_index];
    } else {
        VkResult res = swapchain_acquire_image(s, &s_priv->cur_image_index);
        if (res != VK_SUCCESS)
            return ngli_vk_res2ret(res);

        struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
        s_priv->default_rt = rts[s_priv->cur_image_index];
        s_priv->default_rt->width = s_priv->width;
        s_priv->default_rt->height = s_priv->height;

        struct rendertarget **rts_load = ngli_darray_data(&s_priv->rts_load);
        s_priv->default_rt_load = rts_load[s_priv->cur_image_index];
        s_priv->default_rt_load->width = s_priv->width;
        s_priv->default_rt_load->height = s_priv->height;
    }

    if (config->hud) {
        vkCmdResetQueryPool(s_priv->cur_cmd->cmd_buf, s_priv->query_pool, 0, 2);
        vkCmdWriteTimestamp(s_priv->cur_cmd->cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_priv->query_pool, 0);
    }

    return 0;
}

static int vk_query_draw_time(struct gpu_ctx *s, int64_t *time)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    const struct ngl_config *config = &s->config;

    if (!config->hud)
        return NGL_ERROR_INVALID_USAGE;

    ngli_assert(s_priv->cur_cmd->cmd_buf);
    VkCommandBuffer cmd_buf = s_priv->cur_cmd->cmd_buf;
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_priv->query_pool, 1);

    VkResult res = ngli_cmd_vk_submit(s_priv->cur_cmd);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    res = ngli_cmd_vk_wait(s_priv->cur_cmd);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    uint64_t results[2];
    vkGetQueryPoolResults(vk->device,
                          s_priv->query_pool, 0, 2,
                          sizeof(results), results, sizeof(results[0]),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    *time = results[1] - results[0];

    res = ngli_cmd_vk_begin(s_priv->cur_cmd);
    if (res != VK_SUCCESS)
        return ngli_vk_res2ret(res);

    return 0;
}

static int vk_end_draw(struct gpu_ctx *s, double t)
{
    const struct ngl_config *config = &s->config;
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    if (config->offscreen) {
        if (config->capture_buffer) {
            struct texture **colors = ngli_darray_data(&s_priv->colors);
            struct texture *color = colors[s_priv->cur_frame_index];
            ngli_texture_vk_copy_to_buffer(color, s_priv->capture_buffer);

            VkResult res = ngli_cmd_vk_submit(s_priv->cur_cmd);
            if (res != VK_SUCCESS)
                return ngli_vk_res2ret(res);

            res = ngli_cmd_vk_wait(s_priv->cur_cmd);
            if (res != VK_SUCCESS)
                return ngli_vk_res2ret(res);

            memcpy(config->capture_buffer, s_priv->mapped_data, s_priv->capture_buffer_size);
        } else {
            VkResult res = ngli_cmd_vk_submit(s_priv->cur_cmd);
            if (res != VK_SUCCESS)
                return ngli_vk_res2ret(res);
        }
    } else {
        struct texture **colors = ngli_darray_data(&s_priv->colors);
        ngli_texture_vk_transition_layout(colors[s_priv->cur_image_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        VkResult res = ngli_cmd_vk_submit(s_priv->cur_cmd);
        if (res != VK_SUCCESS)
            return ngli_vk_res2ret(res);

        res = swapchain_present_buffer(s, t);
        if (res != VK_SUCCESS)
            return ngli_vk_res2ret(res);
    }

    s_priv->cur_cmd = NULL;

    return 0;
}

static void vk_destroy(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    if (!vk)
        return;

    vkDeviceWaitIdle(vk->device);

#if DEBUG_GPU_CAPTURE
    if (s->gpu_capture)
        ngli_gpu_capture_end(s->gpu_capture_ctx);
    ngli_gpu_capture_freep(&s->gpu_capture_ctx);
#endif

    destroy_command_pool_and_buffers(s);
    destroy_semaphores(s);
    destroy_dummy_texture(s);
    destroy_render_resources(s);
    destroy_swapchain(s);
    destroy_query_pool(s);

    ngli_glslang_uninit();

    ngli_vkcontext_freep(&s_priv->vkcontext);
}

static void vk_wait_idle(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    vkDeviceWaitIdle(vk->device);
}

static int vk_transform_cull_mode(struct gpu_ctx *s, int cull_mode)
{
    static const int cull_mode_map[NGLI_CULL_MODE_NB] = {
        [NGLI_CULL_MODE_NONE]      = NGLI_CULL_MODE_NONE,
        [NGLI_CULL_MODE_FRONT_BIT] = NGLI_CULL_MODE_BACK_BIT,
        [NGLI_CULL_MODE_BACK_BIT]  = NGLI_CULL_MODE_FRONT_BIT,
    };
    return cull_mode_map[cull_mode];
}

static void vk_transform_projection_matrix(struct gpu_ctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f,
    };
    ngli_mat4_mul(dst, matrix, dst);
}

static void vk_get_rendertarget_uvcoord_matrix(struct gpu_ctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    memcpy(dst, matrix, 4 * 4 * sizeof(float));
}

static struct rendertarget *vk_get_default_rendertarget(struct gpu_ctx *s, int load_op)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    switch (load_op) {
    case NGLI_LOAD_OP_DONT_CARE:
    case NGLI_LOAD_OP_CLEAR:
        return s_priv->default_rt;
    case NGLI_LOAD_OP_LOAD:
        return s_priv->default_rt_load;
    default:
        ngli_assert(0);
    }
}

static const struct rendertarget_desc *vk_get_default_rendertarget_desc(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    return &s_priv->default_rt_desc;
}

static void vk_begin_render_pass(struct gpu_ctx *s, struct rendertarget *rt)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    const struct rendertarget_params *params = &rt->params;
    const struct rendertarget_vk *rt_vk = (struct rendertarget_vk *)rt;

    ngli_assert(rt);

    if (!s_priv->cur_cmd) {
        VkResult res = ngli_cmd_vk_begin_transient(s, 0, &s_priv->cur_cmd);
        ngli_assert(res == VK_SUCCESS);
        s_priv->cur_cmd_is_transient = 1;
    }

    for (int i = 0; i < params->nb_colors; i++) {
        struct texture *attachment = params->colors[i].attachment;
        ngli_texture_vk_transition_layout(attachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        struct texture *resolve_target = params->colors[i].resolve_target;
        if (resolve_target)
            ngli_texture_vk_transition_layout(resolve_target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    struct texture *attachment = params->depth_stencil.attachment;
    if (attachment) {
        ngli_texture_vk_transition_layout(attachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        struct texture *resolve_target = params->depth_stencil.resolve_target;
        if (resolve_target) {
            ngli_texture_vk_transition_layout(resolve_target, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }

    VkCommandBuffer cmd_buf = s_priv->cur_cmd->cmd_buf;
    const VkRenderPassBeginInfo render_pass_begin_info = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = rt_vk->render_pass,
        .framebuffer = rt_vk->framebuffer,
        .renderArea  = {
            .extent.width  = rt->width,
            .extent.height = rt->height,
        },
        .clearValueCount = rt_vk->nb_clear_values,
        .pClearValues    = rt_vk->clear_values,
    };
    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    s_priv->current_rt = rt;
}

static void vk_end_render_pass(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;

    ngli_assert(s_priv->current_rt);

    VkCommandBuffer cmd_buf = s_priv->cur_cmd->cmd_buf;
    vkCmdEndRenderPass(cmd_buf);

    const struct rendertarget *rt = s_priv->current_rt;
    const struct rendertarget_params *params = &rt->params;

    for (int i = 0; i < params->nb_colors; i++) {
        struct texture *texture = params->colors[i].attachment;
        ngli_texture_vk_transition_to_default_layout(texture);
        struct texture *resolve_target = params->colors[i].resolve_target;
        if (resolve_target) {
            ngli_texture_vk_transition_to_default_layout(resolve_target);
        }
    }

    struct texture *attachment = params->depth_stencil.attachment;
    if (attachment) {
        ngli_texture_vk_transition_to_default_layout(attachment);
        struct texture *resolve_target = params->depth_stencil.resolve_target;
        if (resolve_target) {
            ngli_texture_vk_transition_to_default_layout(resolve_target);
        }
    }

    if (s_priv->cur_cmd_is_transient) {
        ngli_cmd_vk_execute_transient(&s_priv->cur_cmd);
        s_priv->cur_cmd_is_transient = 0;
    }

    s_priv->current_rt = NULL;
}

static void vk_set_viewport(struct gpu_ctx *s, const int *viewport)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    memcpy(s_priv->viewport, viewport, sizeof(s_priv->viewport));
}

static void vk_get_viewport(struct gpu_ctx *s, int *viewport)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    memcpy(viewport, &s_priv->viewport, sizeof(s_priv->viewport));
}

static void vk_set_scissor(struct gpu_ctx *s, const int *scissor)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    memcpy(&s_priv->scissor, scissor, sizeof(s_priv->scissor));
}

static void vk_get_scissor(struct gpu_ctx *s, int *scissor)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    memcpy(scissor, &s_priv->scissor, sizeof(s_priv->scissor));
}

static int vk_get_preferred_depth_format(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    return vk->preferred_depth_format;
}

static int vk_get_preferred_depth_stencil_format(struct gpu_ctx *s)
{
    struct gpu_ctx_vk *s_priv = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    return vk->preferred_depth_stencil_format;
}

static int vk_buffer_init(struct buffer *s, int size, int usage)
{
    VkResult res = ngli_buffer_vk_init(s, size, usage);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize buffer: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int vk_buffer_upload(struct buffer *s, const void *data, int size, int offset)
{
    VkResult res = ngli_buffer_vk_upload(s, data, size, offset);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to upload buffer: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int vk_buffer_map(struct buffer *s, int size, int offset, void **data)
{
    VkResult res = ngli_buffer_vk_map(s, size, offset, data);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to map buffer: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static void vk_buffer_unmap(struct buffer *s)
{
    ngli_buffer_vk_unmap(s);
}

static int vk_texture_init(struct texture *s, const struct texture_params *params)
{
    VkResult res = ngli_texture_vk_init(s, params);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize texture: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int vk_texture_upload(struct texture *s, const uint8_t *data, int linesize)
{
    VkResult res = ngli_texture_vk_upload(s, data, linesize);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to upload texture: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int vk_texture_generate_mipmap(struct texture *s)
{
    VkResult res = ngli_texture_vk_generate_mipmap(s);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to generate texture mipmap: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int vk_rendertarget_init(struct rendertarget *s, const struct rendertarget_params *params)
{
    VkResult res = ngli_rendertarget_vk_init(s, params);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize render target: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int vk_pipeline_init(struct pipeline *s, const struct pipeline_params *params)
{
    VkResult res = ngli_pipeline_vk_init(s, params);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize pipeline: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

const struct gpu_ctx_class ngli_gpu_ctx_vk = {
    .name                               = "Vulkan",
    .create                             = vk_create,
    .init                               = vk_init,
    .resize                             = vk_resize,
    .set_capture_buffer                 = vk_set_capture_buffer,
    .begin_update                       = vk_begin_update,
    .end_update                         = vk_end_update,
    .begin_draw                         = vk_begin_draw,
    .query_draw_time                    = vk_query_draw_time,
    .end_draw                           = vk_end_draw,
    .wait_idle                          = vk_wait_idle,
    .destroy                            = vk_destroy,

    .transform_cull_mode                = vk_transform_cull_mode,
    .transform_projection_matrix        = vk_transform_projection_matrix,
    .get_rendertarget_uvcoord_matrix    = vk_get_rendertarget_uvcoord_matrix,

    .get_default_rendertarget           = vk_get_default_rendertarget,
    .get_default_rendertarget_desc      = vk_get_default_rendertarget_desc,

    .begin_render_pass                  = vk_begin_render_pass,
    .end_render_pass                    = vk_end_render_pass,

    .set_viewport                       = vk_set_viewport,
    .get_viewport                       = vk_get_viewport,
    .set_scissor                        = vk_set_scissor,
    .get_scissor                        = vk_get_scissor,

    .get_preferred_depth_format         = vk_get_preferred_depth_format,
    .get_preferred_depth_stencil_format = vk_get_preferred_depth_stencil_format,

    .buffer_create                      = ngli_buffer_vk_create,
    .buffer_init                        = vk_buffer_init,
    .buffer_upload                      = vk_buffer_upload,
    .buffer_map                         = vk_buffer_map,
    .buffer_unmap                       = vk_buffer_unmap,
    .buffer_freep                       = ngli_buffer_vk_freep,

    .pipeline_create                    = ngli_pipeline_vk_create,
    .pipeline_init                      = vk_pipeline_init,
    .pipeline_set_resources             = ngli_pipeline_vk_set_resources,
    .pipeline_update_attribute          = ngli_pipeline_vk_update_attribute,
    .pipeline_update_uniform            = ngli_pipeline_vk_update_uniform,
    .pipeline_update_texture            = ngli_pipeline_vk_update_texture,
    .pipeline_update_buffer             = ngli_pipeline_vk_update_buffer,
    .pipeline_draw                      = ngli_pipeline_vk_draw,
    .pipeline_draw_indexed              = ngli_pipeline_vk_draw_indexed,
    .pipeline_dispatch                  = ngli_pipeline_vk_dispatch,
    .pipeline_freep                     = ngli_pipeline_vk_freep,

    .program_create                     = ngli_program_vk_create,
    .program_init                       = ngli_program_vk_init,
    .program_freep                      = ngli_program_vk_freep,

    .rendertarget_create                = ngli_rendertarget_vk_create,
    .rendertarget_init                  = vk_rendertarget_init,
    .rendertarget_freep                 = ngli_rendertarget_vk_freep,

    .texture_create                     = ngli_texture_vk_create,
    .texture_init                       = vk_texture_init,
    .texture_upload                     = vk_texture_upload,
    .texture_generate_mipmap            = vk_texture_generate_mipmap,
    .texture_freep                      = ngli_texture_vk_freep,
};
