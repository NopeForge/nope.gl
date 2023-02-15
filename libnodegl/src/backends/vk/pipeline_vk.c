/*
 * Copyright 2016-2022 GoPro Inc.
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
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "darray.h"
#include "format.h"
#include "gpu_ctx_vk.h"
#include "hmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"
#include "pipeline_vk.h"
#include "rendertarget.h"
#include "texture_vk.h"
#include "topology.h"
#include "type.h"
#include "utils.h"
#include "vkcontext.h"
#include "vkutils.h"
#include "format_vk.h"
#include "buffer_vk.h"
#include "program_vk.h"
#include "rendertarget_vk.h"
#include "ycbcr_sampler_vk.h"

struct attribute_binding {
    struct pipeline_attribute_desc desc;
    const struct buffer *buffer;
};

struct buffer_binding {
    struct pipeline_buffer_desc desc;
    const struct buffer *buffer;
    uint32_t update_desc_flags;
};

struct texture_binding {
    struct pipeline_texture_desc desc;
    uint32_t desc_binding_index;
    const struct texture *texture;
    uint32_t update_desc_flags;
    int use_ycbcr_sampler;
    struct ycbcr_sampler_vk *ycbcr_sampler;
};

static const VkPrimitiveTopology vk_primitive_topology_map[NGLI_PRIMITIVE_TOPOLOGY_NB] = {
    [NGLI_PRIMITIVE_TOPOLOGY_POINT_LIST]     = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_LIST]      = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_STRIP]     = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST]  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

static VkPrimitiveTopology get_vk_topology(int topology)
{
    return vk_primitive_topology_map[topology];
}

static const VkBlendFactor vk_blend_factor_map[NGLI_BLEND_FACTOR_NB] = {

    [NGLI_BLEND_FACTOR_ZERO]                = VK_BLEND_FACTOR_ZERO,
    [NGLI_BLEND_FACTOR_ONE]                 = VK_BLEND_FACTOR_ONE,
    [NGLI_BLEND_FACTOR_SRC_COLOR]           = VK_BLEND_FACTOR_SRC_COLOR,
    [NGLI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    [NGLI_BLEND_FACTOR_DST_COLOR]           = VK_BLEND_FACTOR_DST_COLOR,
    [NGLI_BLEND_FACTOR_ONE_MINUS_DST_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    [NGLI_BLEND_FACTOR_SRC_ALPHA]           = VK_BLEND_FACTOR_SRC_ALPHA,
    [NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    [NGLI_BLEND_FACTOR_DST_ALPHA]           = VK_BLEND_FACTOR_DST_ALPHA,
    [NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
};

static VkBlendFactor get_vk_blend_factor(int blend_factor)
{
    return vk_blend_factor_map[blend_factor];
}

static const VkBlendOp vk_blend_op_map[NGLI_BLEND_OP_NB] = {
    [NGLI_BLEND_OP_ADD]              = VK_BLEND_OP_ADD,
    [NGLI_BLEND_OP_SUBTRACT]         = VK_BLEND_OP_SUBTRACT,
    [NGLI_BLEND_OP_REVERSE_SUBTRACT] = VK_BLEND_OP_REVERSE_SUBTRACT,
    [NGLI_BLEND_OP_MIN]              = VK_BLEND_OP_MIN,
    [NGLI_BLEND_OP_MAX]              = VK_BLEND_OP_MAX,
};

static VkBlendOp get_vk_blend_op(int blend_op)
{
    return vk_blend_op_map[blend_op];
}

static const VkCompareOp vk_compare_op_map[NGLI_COMPARE_OP_NB] = {
    [NGLI_COMPARE_OP_NEVER]            = VK_COMPARE_OP_NEVER,
    [NGLI_COMPARE_OP_LESS]             = VK_COMPARE_OP_LESS,
    [NGLI_COMPARE_OP_EQUAL]            = VK_COMPARE_OP_EQUAL,
    [NGLI_COMPARE_OP_LESS_OR_EQUAL]    = VK_COMPARE_OP_LESS_OR_EQUAL,
    [NGLI_COMPARE_OP_GREATER]          = VK_COMPARE_OP_GREATER,
    [NGLI_COMPARE_OP_NOT_EQUAL]        = VK_COMPARE_OP_NOT_EQUAL,
    [NGLI_COMPARE_OP_GREATER_OR_EQUAL] = VK_COMPARE_OP_GREATER_OR_EQUAL,
    [NGLI_COMPARE_OP_ALWAYS]           = VK_COMPARE_OP_ALWAYS,
};

static VkCompareOp get_vk_compare_op(int compare_op)
{
    return vk_compare_op_map[compare_op];
}

static const VkStencilOp vk_stencil_op_map[NGLI_STENCIL_OP_NB] = {
    [NGLI_STENCIL_OP_KEEP]                = VK_STENCIL_OP_KEEP,
    [NGLI_STENCIL_OP_ZERO]                = VK_STENCIL_OP_ZERO,
    [NGLI_STENCIL_OP_REPLACE]             = VK_STENCIL_OP_REPLACE,
    [NGLI_STENCIL_OP_INCREMENT_AND_CLAMP] = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    [NGLI_STENCIL_OP_DECREMENT_AND_CLAMP] = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    [NGLI_STENCIL_OP_INVERT]              = VK_STENCIL_OP_INVERT,
    [NGLI_STENCIL_OP_INCREMENT_AND_WRAP]  = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    [NGLI_STENCIL_OP_DECREMENT_AND_WRAP]  = VK_STENCIL_OP_DECREMENT_AND_WRAP,
};

static VkStencilOp get_vk_stencil_op(int stencil_op)
{
    return vk_stencil_op_map[stencil_op];
}

static const VkCullModeFlags vk_cull_mode_map[NGLI_CULL_MODE_NB] = {
    [NGLI_CULL_MODE_NONE]           = VK_CULL_MODE_NONE,
    [NGLI_CULL_MODE_FRONT_BIT]      = VK_CULL_MODE_FRONT_BIT,
    [NGLI_CULL_MODE_BACK_BIT]       = VK_CULL_MODE_BACK_BIT,
};

static VkCullModeFlags get_vk_cull_mode(int cull_mode)
{
    return vk_cull_mode_map[cull_mode];
}

static VkColorComponentFlags get_vk_color_write_mask(int color_write_mask)
{
    return (color_write_mask & NGLI_COLOR_COMPONENT_R_BIT ? VK_COLOR_COMPONENT_R_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_G_BIT ? VK_COLOR_COMPONENT_G_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_B_BIT ? VK_COLOR_COMPONENT_B_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_A_BIT ? VK_COLOR_COMPONENT_A_BIT : 0);
}

static VkResult create_attribute_descs(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_darray_init(&s_priv->vertex_attribute_descs, sizeof(VkVertexInputAttributeDescription), 0);
    ngli_darray_init(&s_priv->vertex_binding_descs,   sizeof(VkVertexInputBindingDescription), 0);
    ngli_darray_init(&s_priv->vertex_buffers, sizeof(VkBuffer), 0);
    ngli_darray_init(&s_priv->vertex_offsets, sizeof(VkDeviceSize), 0);

    const struct pipeline_layout *layout = &params->layout;
    for (int i = 0; i < layout->nb_attributes; i++) {
        const struct pipeline_attribute_desc *desc = &layout->attributes_desc[i];

        const struct attribute_binding attribute_binding = {
            .desc = *desc,
        };
        if (!ngli_darray_push(&s_priv->attribute_bindings, &attribute_binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const VkVertexInputBindingDescription binding_desc = {
            .binding   = i,
            .stride    = desc->stride,
            .inputRate = desc->rate ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        if (!ngli_darray_push(&s_priv->vertex_binding_descs, &binding_desc))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const VkVertexInputAttributeDescription attr_desc = {
            .binding  = i,
            .location = desc->location,
            .format   = ngli_format_ngl_to_vk(desc->format),
            .offset   = desc->offset,
        };
        if (!ngli_darray_push(&s_priv->vertex_attribute_descs, &attr_desc))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const VkBuffer buffer = VK_NULL_HANDLE;
        if (!ngli_darray_push(&s_priv->vertex_buffers, &buffer))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const VkDeviceSize offset = 0;
        if (!ngli_darray_push(&s_priv->vertex_offsets, &offset))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return VK_SUCCESS;
}

static VkResult pipeline_graphics_init(struct pipeline *s)
{
    const struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    const struct pipeline_graphics *graphics = &s->graphics;
    const struct graphicstate *state = &graphics->state;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = ngli_darray_count(&s_priv->vertex_binding_descs),
        .pVertexBindingDescriptions      = ngli_darray_data(&s_priv->vertex_binding_descs),
        .vertexAttributeDescriptionCount = ngli_darray_count(&s_priv->vertex_attribute_descs),
        .pVertexAttributeDescriptions    = ngli_darray_data(&s_priv->vertex_attribute_descs),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = get_vk_topology(graphics->topology),
    };

    const VkViewport viewport = {0};
    const VkRect2D scissor = {0};
    const VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
    };

    const VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth   = 1.f,
        .cullMode    = get_vk_cull_mode(state->cull_mode),
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
    };

    const struct rendertarget_desc *desc = &graphics->rt_desc;
    const VkSampleCountFlagBits samples = ngli_ngl_samples_to_vk(desc->samples);
    const VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = samples,
    };

    const VkPipelineDepthStencilStateCreateInfo depthstencil_state_create_info = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = state->depth_test,
        .depthWriteEnable      = state->depth_write_mask,
        .depthCompareOp        = get_vk_compare_op(state->depth_func),
        .depthBoundsTestEnable = 0,
        .stencilTestEnable     = state->stencil_test,
        .front = {
            .failOp      = get_vk_stencil_op(state->stencil_fail),
            .passOp      = get_vk_stencil_op(state->stencil_depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_depth_fail),
            .compareOp   = get_vk_compare_op(state->stencil_func),
            .compareMask = state->stencil_read_mask,
            .writeMask   = state->stencil_write_mask,
            .reference   = state->stencil_ref,
        },
        .back = {
            .failOp      = get_vk_stencil_op(state->stencil_fail),
            .passOp      = get_vk_stencil_op(state->stencil_depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_depth_fail),
            .compareOp   = get_vk_compare_op(state->stencil_func),
            .compareMask = state->stencil_read_mask,
            .writeMask   = state->stencil_write_mask,
            .reference   = state->stencil_ref,
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };

    VkPipelineColorBlendAttachmentState colorblend_attachment_states[NGLI_MAX_COLOR_ATTACHMENTS] = {0};
    for (int i = 0; i < graphics->rt_desc.nb_colors; i++) {
        colorblend_attachment_states[i] = (VkPipelineColorBlendAttachmentState) {
            .blendEnable         = state->blend,
            .srcColorBlendFactor = get_vk_blend_factor(state->blend_src_factor),
            .dstColorBlendFactor = get_vk_blend_factor(state->blend_dst_factor),
            .colorBlendOp        = get_vk_blend_op(state->blend_op),
            .srcAlphaBlendFactor = get_vk_blend_factor(state->blend_src_factor_a),
            .dstAlphaBlendFactor = get_vk_blend_factor(state->blend_dst_factor_a),
            .alphaBlendOp        = get_vk_blend_op(state->blend_op_a),
            .colorWriteMask      = get_vk_color_write_mask(state->color_write_mask),
        };
    }

    const VkPipelineColorBlendStateCreateInfo colorblend_state_create_info = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = graphics->rt_desc.nb_colors,
        .pAttachments    = colorblend_attachment_states,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NGLI_ARRAY_NB(dynamic_states),
        .pDynamicStates    = dynamic_states,
    };

    const struct program_vk *program_vk = (struct program_vk *)s->program;
    const VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = program_vk->shaders[NGLI_PROGRAM_SHADER_VERT],
            .pName  = "main",
        }, {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = program_vk->shaders[NGLI_PROGRAM_SHADER_FRAG],
            .pName  = "main",
        },
    };

    VkRenderPass render_pass;
    VkResult res = ngli_vk_create_compatible_renderpass(s->gpu_ctx, &graphics->rt_desc, &render_pass);
    if (res != VK_SUCCESS)
        return res;

    const VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = NGLI_ARRAY_NB(shader_stage_create_info),
        .pStages             = shader_stage_create_info,
        .pVertexInputState   = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState      = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState   = &multisampling_state_create_info,
        .pDepthStencilState  = &depthstencil_state_create_info,
        .pColorBlendState    = &colorblend_state_create_info,
        .pDynamicState       = &dynamic_state_create_info,
        .layout              = s_priv->pipeline_layout,
        .renderPass          = render_pass,
        .subpass             = 0,
    };
    res = vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &s_priv->pipeline);

    vkDestroyRenderPass(vk->device, render_pass, NULL);

    return res;
}

static VkResult pipeline_compute_init(struct pipeline *s)
{
    const struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const struct program_vk *program_vk = (struct program_vk *)s->program;
    const VkPipelineShaderStageCreateInfo shader_stage_create_info = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = program_vk->shaders[NGLI_PROGRAM_SHADER_COMP],
        .pName  = "main",
    };

    const VkComputePipelineCreateInfo pipeline_create_info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = shader_stage_create_info,
        .layout = s_priv->pipeline_layout,
    };

    return vkCreateComputePipelines(vk->device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &s_priv->pipeline);
}

static const VkShaderStageFlags stage_flag_map[NGLI_PROGRAM_SHADER_NB] = {
    [NGLI_PROGRAM_SHADER_VERT] = VK_SHADER_STAGE_VERTEX_BIT,
    [NGLI_PROGRAM_SHADER_FRAG] = VK_SHADER_STAGE_FRAGMENT_BIT,
    [NGLI_PROGRAM_SHADER_COMP] = VK_SHADER_STAGE_COMPUTE_BIT,
};

static VkShaderStageFlags get_vk_stage_flags(int stage)
{
    return stage_flag_map[stage];
}

static const VkDescriptorType descriptor_type_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [NGLI_TYPE_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [NGLI_TYPE_SAMPLER_2D]     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_SAMPLER_3D]     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_SAMPLER_CUBE]   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_IMAGE_2D]       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
};

static VkDescriptorType get_vk_descriptor_type(int type)
{
    const VkDescriptorType descriptor_type = descriptor_type_map[type];
    ngli_assert(descriptor_type);
    return descriptor_type;
}

static VkResult create_desc_set_layout_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    const struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_darray_init(&s_priv->desc_set_layout_bindings, sizeof(VkDescriptorSetLayoutBinding), 0);

    VkDescriptorPoolSize desc_pool_size_map[NGLI_TYPE_NB] = {
        [NGLI_TYPE_UNIFORM_BUFFER] = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        [NGLI_TYPE_STORAGE_BUFFER] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        [NGLI_TYPE_SAMPLER_2D]     = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_SAMPLER_3D]     = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_SAMPLER_CUBE]   = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_IMAGE_2D]       = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
    };

    const struct pipeline_layout *layout = &params->layout;
    for (int i = 0; i < layout->nb_buffers; i++) {
        const struct pipeline_buffer_desc *desc = &layout->buffers_desc[i];

        const VkDescriptorType type = get_vk_descriptor_type(desc->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = desc->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = get_vk_stage_flags(desc->stage),
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const struct buffer_binding buffer_binding = {
            .desc = *desc,
        };
        if (!ngli_darray_push(&s_priv->buffer_bindings, &buffer_binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        ngli_assert(desc_pool_size_map[desc->type].type);
        desc_pool_size_map[desc->type].descriptorCount += gpu_ctx_vk->nb_in_flight_frames;
    }

    for (int i = 0; i < layout->nb_textures; i++) {
        const struct pipeline_texture_desc *desc = &layout->textures_desc[i];

        const VkDescriptorType type = get_vk_descriptor_type(desc->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = desc->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = get_vk_stage_flags(desc->stage),
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        const struct texture_binding texture_binding = {
            .desc = *desc,
            .desc_binding_index = ngli_darray_count(&s_priv->desc_set_layout_bindings) - 1,
        };
        if (!ngli_darray_push(&s_priv->texture_bindings, &texture_binding))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        ngli_assert(desc_pool_size_map[desc->type].type);
        desc_pool_size_map[desc->type].descriptorCount += gpu_ctx_vk->nb_in_flight_frames;
    }

    uint32_t nb_desc_pool_sizes = 0;
    VkDescriptorPoolSize desc_pool_sizes[NGLI_TYPE_NB];
    for (int i = 0; i < NGLI_ARRAY_NB(desc_pool_size_map); i++) {
        if (desc_pool_size_map[i].descriptorCount)
            desc_pool_sizes[nb_desc_pool_sizes++] = desc_pool_size_map[i];
    }
    if (!nb_desc_pool_sizes)
        return VK_SUCCESS;

    const VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = nb_desc_pool_sizes,
        .pPoolSizes    = desc_pool_sizes,
        .maxSets       = gpu_ctx_vk->nb_in_flight_frames,
    };

    VkResult res = vkCreateDescriptorPool(vk->device, &descriptor_pool_create_info, NULL, &s_priv->desc_pool);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

static VkResult create_desc_layout(struct pipeline *s)
{
    const struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ngli_darray_count(&s_priv->desc_set_layout_bindings),
        .pBindings    = ngli_darray_data(&s_priv->desc_set_layout_bindings),
    };

    VkResult res = vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, NULL, &s_priv->desc_set_layout);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

static VkResult create_desc_sets(struct pipeline *s)
{
    const struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    VkDescriptorSetLayout *desc_set_layouts = ngli_calloc(gpu_ctx_vk->nb_in_flight_frames, sizeof(*desc_set_layouts));
    if (!desc_set_layouts)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (int i = 0; i < gpu_ctx_vk->nb_in_flight_frames; i++)
        desc_set_layouts[i] = s_priv->desc_set_layout;

    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = s_priv->desc_pool,
        .descriptorSetCount = gpu_ctx_vk->nb_in_flight_frames,
        .pSetLayouts        = desc_set_layouts
    };

    s_priv->desc_sets = ngli_calloc(gpu_ctx_vk->nb_in_flight_frames, sizeof(*s_priv->desc_sets));
    if (!s_priv->desc_sets) {
        ngli_free(desc_set_layouts);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkResult res = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, s_priv->desc_sets);
    if (res != VK_SUCCESS) {
        ngli_free(desc_set_layouts);
        return res;
    }

    ngli_free(desc_set_layouts);
    return VK_SUCCESS;
}

static VkResult create_pipeline_layout(struct pipeline *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = s_priv->desc_set_layout ? 1 : 0,
        .pSetLayouts    = &s_priv->desc_set_layout,
    };

    return vkCreatePipelineLayout(vk->device, &pipeline_layout_create_info, NULL, &s_priv->pipeline_layout);
}

static VkResult create_pipeline(struct pipeline *s)
{
    VkResult res = create_desc_layout(s);
    if (res != VK_SUCCESS)
        return res;

    res = create_desc_sets(s);
    if (res != VK_SUCCESS)
        return res;

    res = create_pipeline_layout(s);
    if (res != VK_SUCCESS)
        return res;

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        res = pipeline_graphics_init(s);
    } else if (s->type == NGLI_PIPELINE_TYPE_COMPUTE) {
        res = pipeline_compute_init(s);
    } else {
        ngli_assert(0);
    }
    return VK_SUCCESS;
}

static void destroy_pipeline_keep_pool(struct pipeline *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    vkDestroyPipeline(vk->device, s_priv->pipeline, NULL);
    s_priv->pipeline = VK_NULL_HANDLE;

    vkDestroyPipelineLayout(vk->device, s_priv->pipeline_layout, NULL);
    s_priv->pipeline_layout = VK_NULL_HANDLE;

    vkDestroyDescriptorSetLayout(vk->device, s_priv->desc_set_layout, NULL);
    s_priv->desc_set_layout = VK_NULL_HANDLE;
}

static void destroy_pipeline(struct pipeline *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    destroy_pipeline_keep_pool(s);

    vkDestroyDescriptorPool(vk->device, s_priv->desc_pool, NULL);
    s_priv->desc_pool = VK_NULL_HANDLE;
    ngli_freep(&s_priv->desc_sets);
}

static void request_desc_sets_update(struct pipeline *s)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    struct texture_binding *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        struct texture_binding *binding = &texture_bindings[i];
        binding->update_desc_flags = ~0;
    }

    struct buffer_binding *buffer_bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        struct buffer_binding *binding = &buffer_bindings[i];
        binding->update_desc_flags = ~0;
    }
}

static VkResult recreate_pipeline(struct pipeline *s)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    /*
     * Destroy the current pipeline but keep the descriptor pool to avoid
     * re-allocating the descriptor sets.
     */
    destroy_pipeline_keep_pool(s);

    VkResult res = vkResetDescriptorPool(vk->device, s_priv->desc_pool, 0);
    if (res != VK_SUCCESS)
        return res;
    ngli_freep(&s_priv->desc_sets);

    res = create_pipeline(s);
    if (res != VK_SUCCESS)
        return res;

    /*
     * The descriptor sets have been reset during pipeline re-creation, thus,
     * we need to ensure they are properly updated before the next pipeline
     * execution.
     */
    request_desc_sets_update(s);

    return VK_SUCCESS;
}

struct pipeline *ngli_pipeline_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct pipeline_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct pipeline *)s;
}

VkResult ngli_pipeline_vk_init(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    s->type     = params->type;
    s->graphics = params->graphics;
    s->program  = params->program;

    ngli_darray_init(&s_priv->texture_bindings, sizeof(struct texture_binding), 0);
    ngli_darray_init(&s_priv->buffer_bindings,  sizeof(struct buffer_binding), 0);
    ngli_darray_init(&s_priv->attribute_bindings, sizeof(struct attribute_binding), 0);

    if (params->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        VkResult res = create_attribute_descs(s, params);
        if (res != VK_SUCCESS)
            return res;
    }

    VkResult res = create_desc_set_layout_bindings(s, params);
    if (res != VK_SUCCESS)
        return res;

    return create_pipeline(s);
}

int ngli_pipeline_vk_set_resources(struct pipeline *s, const struct pipeline_resources *resources)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_assert(ngli_darray_count(&s_priv->attribute_bindings) == resources->nb_attributes);
    for (int i = 0; i < resources->nb_attributes; i++) {
        int ret = ngli_pipeline_vk_update_attribute(s, i, resources->attributes[i]);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->buffer_bindings) == resources->nb_buffers);
    for (int i = 0; i < resources->nb_buffers; i++) {
        const struct buffer_binding *buffer_binding = ngli_darray_get(&s_priv->buffer_bindings, i);
        const struct pipeline_buffer_desc *buffer_desc = &buffer_binding->desc;
        int ret = ngli_pipeline_vk_update_buffer(s, i, resources->buffers[i], buffer_desc->offset, buffer_desc->size);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->texture_bindings) == resources->nb_textures);
    for (int i = 0; i < resources->nb_textures; i++) {
        int ret = ngli_pipeline_vk_update_texture(s, i, resources->textures[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int ngli_pipeline_vk_update_attribute(struct pipeline *s, int index, const struct buffer *buffer)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(s->type == NGLI_PIPELINE_TYPE_GRAPHICS);

    struct attribute_binding *attribute_binding = ngli_darray_get(&s_priv->attribute_bindings, index);
    ngli_assert(attribute_binding);
    attribute_binding->buffer = buffer;

    VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    if (buffer) {
        struct buffer_vk *buffer_vk = (struct buffer_vk *)buffer;
        vertex_buffers[index] = buffer_vk->buffer;
    } else {
        vertex_buffers[index] = VK_NULL_HANDLE;
    }

    return 0;
}

int ngli_pipeline_vk_update_uniform(struct pipeline *s, int index, const void *value)
{
    return NGL_ERROR_GRAPHICS_UNSUPPORTED;
}

static int need_desc_set_layout_update(const struct texture_binding *binding,
                                       const struct texture *texture)
{
    const struct texture_vk *texture_vk = (const struct texture_vk *)texture;

    if (binding->use_ycbcr_sampler != texture_vk->use_ycbcr_sampler)
        return 1;

    if (binding->use_ycbcr_sampler) {
        /*
         * The specification mandates that, for a given combined image sampler
         * descriptor set, the sampler and image view with YCbCr conversion
         * enabled must have been created with an identical
         * VkSamplerYcbcrConversionInfo object (ie: with the same
         * VkSamplerYcbcrConversion object).
         */
        return binding->ycbcr_sampler != texture_vk->ycbcr_sampler;
    }

    return 0;
}

int ngli_pipeline_vk_update_texture(struct pipeline *s, int index, const struct texture *texture)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    struct texture_binding *texture_binding = ngli_darray_get(&s_priv->texture_bindings, index);
    ngli_assert(texture_binding);

    texture_binding->texture = texture ? texture : gpu_ctx_vk->dummy_texture;
    texture_binding->update_desc_flags = ~0;

    if (texture) {
        struct texture_vk *texture_vk = (struct texture_vk *)texture;
        if (need_desc_set_layout_update(texture_binding, texture)) {
            VkDescriptorSetLayoutBinding *desc_bindings = ngli_darray_data(&s_priv->desc_set_layout_bindings);
            VkDescriptorSetLayoutBinding *desc_binding = &desc_bindings[texture_binding->desc_binding_index];
            texture_binding->use_ycbcr_sampler = texture_vk->use_ycbcr_sampler;
            ngli_ycbcr_sampler_vk_unrefp(&texture_binding->ycbcr_sampler);
            if (texture_vk->use_ycbcr_sampler) {
                /*
                 * YCbCr conversion enabled samplers must be set at pipeline
                 * creation (through the pImmutableSamplers field). Moreover,
                 * the YCbCr conversion and sampler objects must be kept alive
                 * as long as they are used by the pipeline. For that matter,
                 * we hold a reference on the ycbcr_sampler_vk structure
                 * (containing the conversion and sampler objects) as long as
                 * necessary.
                 */
                texture_binding->ycbcr_sampler = ngli_ycbcr_sampler_vk_ref(texture_vk->ycbcr_sampler);
                desc_binding->pImmutableSamplers = &texture_vk->ycbcr_sampler->sampler;
            } else {
                desc_binding->pImmutableSamplers = VK_NULL_HANDLE;
            }

            VkResult res = recreate_pipeline(s);
            if (res != VK_SUCCESS) {
                LOG(ERROR, "could not recreate pipeline");
                return ngli_vk_res2ret(res);
            }
        }
    }

    return 0;
}

int ngli_pipeline_vk_update_buffer(struct pipeline *s, int index, const struct buffer *buffer, int offset, int size)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    struct buffer_binding *buffer_binding = ngli_darray_get(&s_priv->buffer_bindings, index);
    ngli_assert(buffer_binding);

    buffer_binding->buffer = buffer;
    buffer_binding->desc.offset = offset;
    buffer_binding->desc.size = size;
    buffer_binding->update_desc_flags = ~0;

    return 0;
}

static const VkIndexType vk_indices_type_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R16_UNORM] = VK_INDEX_TYPE_UINT16,
    [NGLI_FORMAT_R32_UINT]  = VK_INDEX_TYPE_UINT32,
};

static VkIndexType get_vk_indices_type(int indices_format)
{
    return vk_indices_type_map[indices_format];
}

static int update_descriptor_set(struct pipeline *s)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    const uint32_t update_desc_flags = (1 << gpu_ctx_vk->cur_frame_index);
    const uint32_t update_desc_mask = ~update_desc_flags;

    struct texture_binding *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        struct texture_binding *binding = &texture_bindings[i];
        if (binding->update_desc_flags & update_desc_flags) {
            const struct texture_vk *texture_vk = (struct texture_vk *)binding->texture;
            const VkDescriptorImageInfo image_info = {
                .imageLayout = texture_vk->default_image_layout,
                .imageView   = texture_vk->image_view,
                .sampler     = texture_vk->sampler,
            };
            const struct pipeline_texture_desc *desc = &binding->desc;
            const VkWriteDescriptorSet write_descriptor_set = {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet           = s_priv->desc_sets[gpu_ctx_vk->cur_frame_index],
                .dstBinding       = desc->binding,
                .dstArrayElement  = 0,
                .descriptorType   = get_vk_descriptor_type(desc->type),
                .descriptorCount  = 1,
                .pImageInfo       = &image_info,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
            binding->update_desc_flags &= update_desc_mask;
        }
    }

    struct buffer_binding *buffer_bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        struct buffer_binding *binding = &buffer_bindings[i];
        if (binding->update_desc_flags & update_desc_flags) {
            const struct pipeline_buffer_desc *desc = &binding->desc;
            const struct buffer_vk *buffer_vk = (struct buffer_vk *)(binding->buffer);
            const VkDescriptorBufferInfo descriptor_buffer_info = {
                .buffer = buffer_vk->buffer,
                .offset = desc->offset,
                .range  = desc->size ? desc->size : binding->buffer->size,
            };
            const VkWriteDescriptorSet write_descriptor_set = {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet           = s_priv->desc_sets[gpu_ctx_vk->cur_frame_index],
                .dstBinding       = desc->binding,
                .dstArrayElement  = 0,
                .descriptorType   = get_vk_descriptor_type(desc->type),
                .descriptorCount  = 1,
                .pBufferInfo      = &descriptor_buffer_info,
                .pImageInfo       = NULL,
                .pTexelBufferView = NULL,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
            binding->update_desc_flags &= update_desc_mask;
        }
    }

    return 0;
}

static int prepare_pipeline(struct pipeline *s, VkCommandBuffer cmd_buf)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    int ret = update_descriptor_set(s);
    if (ret < 0)
        return ret;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_priv->pipeline);

    const VkViewport viewport = {
        .x        = gpu_ctx_vk->viewport[0],
        .y        = gpu_ctx_vk->viewport[1],
        .width    = gpu_ctx_vk->viewport[2],
        .height   = gpu_ctx_vk->viewport[3],
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    vkCmdSetLineWidth(cmd_buf, 1.0f);

    VkRect2D scissor = {0};
    const struct rendertarget *rt = gpu_ctx_vk->current_rt;
    const struct pipeline_graphics *graphics = &s->graphics;
    if (graphics->state.scissor_test) {
        scissor.offset.x      = gpu_ctx_vk->scissor[0];
        scissor.offset.y      = NGLI_MAX(rt->height - gpu_ctx_vk->scissor[1] - gpu_ctx_vk->scissor[3], 0);
        scissor.extent.width  = gpu_ctx_vk->scissor[2];
        scissor.extent.height = gpu_ctx_vk->scissor[3];
    } else {
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = rt->width;
        scissor.extent.height = rt->height;
    }
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[gpu_ctx_vk->cur_frame_index], 0, NULL);

    const int nb_vertex_buffers = ngli_darray_count(&s_priv->vertex_buffers);
    const VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    const VkDeviceSize *vertex_offsets = ngli_darray_data(&s_priv->vertex_offsets);
    vkCmdBindVertexBuffers(cmd_buf, 0, nb_vertex_buffers, vertex_buffers, vertex_offsets);

    return 0;
}

void ngli_pipeline_vk_draw(struct pipeline *s, int nb_vertices, int nb_instances)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    VkCommandBuffer cmd_buf = gpu_ctx_vk->cur_cmd->cmd_buf;

    int ret = prepare_pipeline(s, cmd_buf);
    if (ret < 0)
        return;

    vkCmdDraw(cmd_buf, nb_vertices, nb_instances, 0, 0);
}

void ngli_pipeline_vk_draw_indexed(struct pipeline *s, const struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    VkCommandBuffer cmd_buf = gpu_ctx_vk->cur_cmd->cmd_buf;

    int ret = prepare_pipeline(s, cmd_buf);
    if (ret < 0)
        return;

    struct buffer_vk *indices_vk = (struct buffer_vk *)indices;
    const VkIndexType indices_type = get_vk_indices_type(indices_format);
    vkCmdBindIndexBuffer(cmd_buf, indices_vk->buffer, 0, indices_type);

    vkCmdDrawIndexed(cmd_buf, nb_indices, nb_instances, 0, 0, 0);
}

void ngli_pipeline_vk_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    int ret = update_descriptor_set(s);
    if (ret < 0)
        return;

    struct cmd_vk *cmd_vk = gpu_ctx_vk->cur_cmd;
    if (!cmd_vk) {
        VkResult res = ngli_cmd_vk_begin_transient(s->gpu_ctx, 0, &cmd_vk);
        if (res != VK_SUCCESS)
            return;
    }
    VkCommandBuffer cmd_buf = cmd_vk->cmd_buf;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, s_priv->pipeline);

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[gpu_ctx_vk->cur_frame_index], 0, NULL);

    vkCmdDispatch(cmd_buf, nb_group_x, nb_group_y, nb_group_z);

    const VkMemoryBarrier barrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                         VK_ACCESS_SHADER_WRITE_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT |
                         VK_ACCESS_TRANSFER_WRITE_BIT |
                         VK_ACCESS_MEMORY_READ_BIT |
                         VK_ACCESS_MEMORY_WRITE_BIT,
    };
    const VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    const VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 1, &barrier, 0, NULL, 0, NULL);

    if (!gpu_ctx_vk->cur_cmd) {
        ngli_cmd_vk_execute_transient(&cmd_vk);
    }
}

void ngli_pipeline_vk_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    destroy_pipeline(s);

    struct texture_binding *texture_bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        struct texture_binding *binding = &texture_bindings[i];
        ngli_ycbcr_sampler_vk_unrefp(&binding->ycbcr_sampler);
    }
    ngli_darray_reset(&s_priv->texture_bindings);
    ngli_darray_reset(&s_priv->buffer_bindings);
    ngli_darray_reset(&s_priv->attribute_bindings);

    ngli_darray_reset(&s_priv->vertex_attribute_descs);
    ngli_darray_reset(&s_priv->vertex_binding_descs);
    ngli_darray_reset(&s_priv->vertex_buffers);
    ngli_darray_reset(&s_priv->vertex_offsets);
    ngli_darray_reset(&s_priv->desc_set_layout_bindings);

    ngli_freep(sp);
}
