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

#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "format.h"
#include "format_vk.h"
#include "gpu_ctx_vk.h"
#include "internal.h"
#include "log.h"
#include "memory.h"
#include "rendertarget_vk.h"
#include "texture_vk.h"
#include "vkutils.h"

static const VkAttachmentLoadOp load_op_map[] = {
    [NGLI_LOAD_OP_LOAD]      = VK_ATTACHMENT_LOAD_OP_LOAD,
    [NGLI_LOAD_OP_CLEAR]     = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [NGLI_LOAD_OP_DONT_CARE] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
};

static VkAttachmentLoadOp get_vk_load_op(int load_op)
{
    return load_op_map[load_op];
}

static const VkAttachmentStoreOp store_op_map[] = {
    [NGLI_STORE_OP_STORE]     = VK_ATTACHMENT_STORE_OP_STORE,
    [NGLI_STORE_OP_DONT_CARE] = VK_ATTACHMENT_STORE_OP_DONT_CARE,
};

static VkAttachmentStoreOp get_vk_store_op(int store_op)
{
    return store_op_map[store_op];
}

static VkResult vk_create_compatible_renderpass(struct gpu_ctx *s, const struct rendertarget_desc *desc, const struct rendertarget_params *params, VkRenderPass *render_pass)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    VkAttachmentDescription descs[2 * (NGLI_MAX_COLOR_ATTACHMENTS + 1)] = {0};
    int nb_descs = 0;

    VkAttachmentReference color_refs[NGLI_MAX_COLOR_ATTACHMENTS] = {0};
    int nb_color_refs = 0;

    VkAttachmentReference resolve_refs[NGLI_MAX_COLOR_ATTACHMENTS + 1] = {0};
    int nb_resolve_refs = 0;

    VkAttachmentReference depth_stencil_ref = {0};
    int nb_depth_stencil_refs = 0;
    const VkSampleCountFlags samples = ngli_ngl_samples_to_vk(desc->samples);

    for (int i = 0; i < desc->nb_colors; i++) {
        VkFormat format = ngli_format_ngl_to_vk(desc->colors[i].format);

        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(vk->phy_device, format, &properties);

        const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if ((properties.optimalTilingFeatures & features) != features) {
            LOG(ERROR, "format %d does not support features 0x%d", format, properties.optimalTilingFeatures);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        const VkAttachmentLoadOp load_op   = params ? get_vk_load_op(params->colors[i].load_op)   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        const VkAttachmentStoreOp store_op = params ? get_vk_store_op(params->colors[i].store_op) : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        descs[nb_descs] = (VkAttachmentDescription) {
            .format         = format,
            .samples        = samples,
            .loadOp         = load_op,
            .storeOp        = store_op,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        color_refs[nb_color_refs] = (VkAttachmentReference) {
            .attachment = nb_descs,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        nb_descs++;
        nb_color_refs++;

        if (desc->colors[i].resolve) {
            descs[nb_descs] = (VkAttachmentDescription) {
                .format         = format,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            resolve_refs[nb_resolve_refs] = (VkAttachmentReference) {
                .attachment = nb_descs,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            nb_descs++;
            nb_resolve_refs++;
        }
    }

    if (desc->depth_stencil.format != NGLI_FORMAT_UNDEFINED) {
        VkFormat format = ngli_format_ngl_to_vk(desc->depth_stencil.format);

        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(vk->phy_device, format, &properties);

        const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((properties.optimalTilingFeatures & features) != features) {
            LOG(ERROR, "format %d does not support features 0x%d", format, properties.optimalTilingFeatures);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        const VkAttachmentLoadOp load_op = params ? get_vk_load_op(params->depth_stencil.load_op) : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        const VkAttachmentStoreOp store_op = params ? get_vk_store_op(params->depth_stencil.store_op) : VK_ATTACHMENT_STORE_OP_DONT_CARE;

        descs[nb_descs] = (VkAttachmentDescription) {
            .format         = format,
            .samples        = samples,
            .loadOp         = load_op,
            .storeOp        = store_op,
            .stencilLoadOp  = load_op,
            .stencilStoreOp = store_op,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        depth_stencil_ref = (VkAttachmentReference) {
            .attachment = nb_descs,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        nb_descs++;
        nb_depth_stencil_refs++;

        if (desc->depth_stencil.resolve) {
            LOG(ERROR, "resolving depth/stencil attachment is not supported");
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
    }

    const VkSubpassDescription subpass_description = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = nb_color_refs,
        .pColorAttachments       = color_refs,
        .pResolveAttachments     = nb_resolve_refs ? resolve_refs : NULL,
        .pDepthStencilAttachment = nb_depth_stencil_refs ? &depth_stencil_ref : NULL,
    };

    const VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        }, {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        }
    };

    const VkRenderPassCreateInfo render_pass_create_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = nb_descs,
        .pAttachments    = descs,
        .subpassCount    = 1,
        .pSubpasses      = &subpass_description,
        .dependencyCount = (uint32_t)NGLI_ARRAY_NB(dependencies),
        .pDependencies   = dependencies,
    };

    return vkCreateRenderPass(vk->device, &render_pass_create_info, NULL, render_pass);
}

VkResult ngli_vk_create_compatible_renderpass(struct gpu_ctx *s, const struct rendertarget_desc *desc, VkRenderPass *render_pass)
{
    return vk_create_compatible_renderpass(s, desc, NULL, render_pass);
}

static VkImageAspectFlags get_vk_image_aspect_flags(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

static VkResult create_image_view(const struct rendertarget *s, const struct texture *texture, uint32_t layer, VkImageView *view)
{
    const struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    const struct texture_vk *texture_vk = (struct texture_vk *)texture;

    const VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = texture_vk->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = texture_vk->format,
        .subresourceRange = {
            .aspectMask     = get_vk_image_aspect_flags(texture_vk->format),
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = layer,
            .layerCount     = 1,
        },
    };
    return vkCreateImageView(vk->device, &view_info, NULL, view);
}

struct rendertarget *ngli_rendertarget_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct rendertarget_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct rendertarget *)s;
}

VkResult ngli_rendertarget_vk_init(struct rendertarget *s, const struct rendertarget_params *params)
{
    struct rendertarget_vk *s_priv = (struct rendertarget_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    s->width = params->width;
    s->height = params->height;
    s->params = *params;

    ngli_assert(params->nb_colors <= NGLI_MAX_COLOR_ATTACHMENTS);

    if (params->depth_stencil.resolve_target) {
        LOG(ERROR, "resolving depth/stencil attachment is not supported");
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    /* Set the rendertarget samples value from the attachments samples value
     * and ensure all the attachments have the same samples value */
    int samples = -1;
    struct rendertarget_desc desc = {0};
    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        const struct texture *texture = attachment->attachment;
        const struct texture_params *texture_params = &texture->params;
        desc.colors[desc.nb_colors].format = texture_params->format;
        desc.colors[desc.nb_colors].resolve = attachment->resolve_target != NULL;
        desc.nb_colors++;
        ngli_assert(samples == -1 || samples == texture_params->samples);
        samples = texture_params->samples;
    }
    const struct attachment *attachment = &params->depth_stencil;
    const struct texture *texture = attachment->attachment;
    if (texture) {
        const struct texture_params *texture_params = &texture->params;
        desc.depth_stencil.format = texture_params->format;
        desc.depth_stencil.resolve = attachment->resolve_target != NULL;
        ngli_assert(samples == -1 || samples == texture_params->samples);
        samples = texture_params->samples;
    }
    desc.samples = samples;

    VkResult res = vk_create_compatible_renderpass(s->gpu_ctx, &desc, params, &s_priv->render_pass);
    if (res != VK_SUCCESS)
        return res;

    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];

        VkImageView view;
        res = create_image_view(s, attachment->attachment, attachment->attachment_layer, &view);
        if (res != VK_SUCCESS)
            return res;

        s_priv->attachments[s_priv->nb_attachments] = view;
        s_priv->nb_attachments++;

        s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {
            .color.float32 = {NGLI_ARG_VEC4(attachment->clear_value)},
        };
        s_priv->nb_clear_values++;

        if (attachment->resolve_target) {
            VkImageView view;
            res = create_image_view(s, attachment->resolve_target, attachment->resolve_target_layer, &view);
            if (res != VK_SUCCESS)
                return res;

            s_priv->attachments[s_priv->nb_attachments] = view;
            s_priv->nb_attachments++;

            s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {
                .color.float32 = {NGLI_ARG_VEC4(attachment->clear_value)},
            };
            s_priv->nb_clear_values++;
        }
    }

    attachment = &params->depth_stencil;
    texture = attachment->attachment;
    if (texture) {
        VkImageView view;
        res = create_image_view(s, texture, attachment->attachment_layer, &view);
        if (res != VK_SUCCESS)
            return res;

        s_priv->attachments[s_priv->nb_attachments] = view;
        s_priv->nb_attachments++;

        s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {.depthStencil = {1.f, 0}};
        s_priv->nb_clear_values++;

        if (attachment->resolve_target) {
            VkImageView view;
            res = create_image_view(s, attachment->resolve_target, attachment->resolve_target_layer, &view);
            if (res != VK_SUCCESS)
                return res;

            s_priv->attachments[s_priv->nb_attachments] = view;
            s_priv->nb_attachments++;

            s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {.depthStencil = {1.f, 0}};
            s_priv->nb_clear_values++;
        }
    }

    const VkFramebufferCreateInfo framebuffer_create_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = s_priv->render_pass,
        .attachmentCount = s_priv->nb_attachments,
        .pAttachments    = s_priv->attachments,
        .width           = s->width,
        .height          = s->height,
        .layers          = 1,
    };

    return vkCreateFramebuffer(vk->device, &framebuffer_create_info, NULL, &s_priv->framebuffer);
}

void ngli_rendertarget_vk_freep(struct rendertarget **sp)
{
    struct rendertarget *s = *sp;
    if (!s)
        return;

    struct rendertarget_vk *s_priv = (struct rendertarget_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;

    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    vkDestroyRenderPass(vk->device, s_priv->render_pass, NULL);
    vkDestroyFramebuffer(vk->device, s_priv->framebuffer, NULL);

    for (int i = 0; i < s_priv->nb_attachments; i++)
        vkDestroyImageView(vk->device, s_priv->attachments[i], NULL);

    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);

    ngli_freep(sp);
}
