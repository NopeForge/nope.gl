/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "bindgroup_vk.h"
#include "ctx_vk.h"
#include "format_vk.h"
#include "log.h"
#include "nopegl.h"
#include "pipeline_vk.h"
#include "program_vk.h"
#include "rendertarget_vk.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"
#include "vkcontext.h"
#include "vkutils.h"

static const VkPrimitiveTopology vk_primitive_topology_map[NGPU_PRIMITIVE_TOPOLOGY_NB] = {
    [NGPU_PRIMITIVE_TOPOLOGY_POINT_LIST]     = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [NGPU_PRIMITIVE_TOPOLOGY_LINE_LIST]      = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [NGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP]     = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    [NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST]  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    [NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

static VkPrimitiveTopology get_vk_topology(enum ngpu_primitive_topology topology)
{
    return vk_primitive_topology_map[topology];
}

static const VkBlendFactor vk_blend_factor_map[NGPU_BLEND_FACTOR_NB] = {

    [NGPU_BLEND_FACTOR_ZERO]                     = VK_BLEND_FACTOR_ZERO,
    [NGPU_BLEND_FACTOR_ONE]                      = VK_BLEND_FACTOR_ONE,
    [NGPU_BLEND_FACTOR_SRC_COLOR]                = VK_BLEND_FACTOR_SRC_COLOR,
    [NGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR]      = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    [NGPU_BLEND_FACTOR_DST_COLOR]                = VK_BLEND_FACTOR_DST_COLOR,
    [NGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR]      = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    [NGPU_BLEND_FACTOR_SRC_ALPHA]                = VK_BLEND_FACTOR_SRC_ALPHA,
    [NGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA]      = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    [NGPU_BLEND_FACTOR_DST_ALPHA]                = VK_BLEND_FACTOR_DST_ALPHA,
    [NGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA]      = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    [NGPU_BLEND_FACTOR_CONSTANT_COLOR]           = VK_BLEND_FACTOR_CONSTANT_COLOR,
    [NGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    [NGPU_BLEND_FACTOR_CONSTANT_ALPHA]           = VK_BLEND_FACTOR_CONSTANT_ALPHA,
    [NGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    [NGPU_BLEND_FACTOR_SRC_ALPHA_SATURATE]       = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
};

static VkBlendFactor get_vk_blend_factor(enum ngpu_blend_factor blend_factor)
{
    return vk_blend_factor_map[blend_factor];
}

static const VkBlendOp vk_blend_op_map[NGPU_BLEND_OP_NB] = {
    [NGPU_BLEND_OP_ADD]              = VK_BLEND_OP_ADD,
    [NGPU_BLEND_OP_SUBTRACT]         = VK_BLEND_OP_SUBTRACT,
    [NGPU_BLEND_OP_REVERSE_SUBTRACT] = VK_BLEND_OP_REVERSE_SUBTRACT,
    [NGPU_BLEND_OP_MIN]              = VK_BLEND_OP_MIN,
    [NGPU_BLEND_OP_MAX]              = VK_BLEND_OP_MAX,
};

static VkBlendOp get_vk_blend_op(enum ngpu_blend_op blend_op)
{
    return vk_blend_op_map[blend_op];
}

static const VkCompareOp vk_compare_op_map[NGPU_COMPARE_OP_NB] = {
    [NGPU_COMPARE_OP_NEVER]            = VK_COMPARE_OP_NEVER,
    [NGPU_COMPARE_OP_LESS]             = VK_COMPARE_OP_LESS,
    [NGPU_COMPARE_OP_EQUAL]            = VK_COMPARE_OP_EQUAL,
    [NGPU_COMPARE_OP_LESS_OR_EQUAL]    = VK_COMPARE_OP_LESS_OR_EQUAL,
    [NGPU_COMPARE_OP_GREATER]          = VK_COMPARE_OP_GREATER,
    [NGPU_COMPARE_OP_NOT_EQUAL]        = VK_COMPARE_OP_NOT_EQUAL,
    [NGPU_COMPARE_OP_GREATER_OR_EQUAL] = VK_COMPARE_OP_GREATER_OR_EQUAL,
    [NGPU_COMPARE_OP_ALWAYS]           = VK_COMPARE_OP_ALWAYS,
};

static VkCompareOp get_vk_compare_op(enum ngpu_compare_op compare_op)
{
    return vk_compare_op_map[compare_op];
}

static const VkStencilOp vk_stencil_op_map[NGPU_STENCIL_OP_NB] = {
    [NGPU_STENCIL_OP_KEEP]                = VK_STENCIL_OP_KEEP,
    [NGPU_STENCIL_OP_ZERO]                = VK_STENCIL_OP_ZERO,
    [NGPU_STENCIL_OP_REPLACE]             = VK_STENCIL_OP_REPLACE,
    [NGPU_STENCIL_OP_INCREMENT_AND_CLAMP] = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    [NGPU_STENCIL_OP_DECREMENT_AND_CLAMP] = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    [NGPU_STENCIL_OP_INVERT]              = VK_STENCIL_OP_INVERT,
    [NGPU_STENCIL_OP_INCREMENT_AND_WRAP]  = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    [NGPU_STENCIL_OP_DECREMENT_AND_WRAP]  = VK_STENCIL_OP_DECREMENT_AND_WRAP,
};

static VkStencilOp get_vk_stencil_op(enum ngpu_stencil_op stencil_op)
{
    return vk_stencil_op_map[stencil_op];
}

static const VkCullModeFlags vk_cull_mode_map[NGPU_CULL_MODE_NB] = {
    [NGPU_CULL_MODE_NONE]           = VK_CULL_MODE_NONE,
    [NGPU_CULL_MODE_FRONT_BIT]      = VK_CULL_MODE_FRONT_BIT,
    [NGPU_CULL_MODE_BACK_BIT]       = VK_CULL_MODE_BACK_BIT,
};

static VkCullModeFlags get_vk_cull_mode(enum ngpu_cull_mode cull_mode)
{
    return vk_cull_mode_map[cull_mode];
}

static const VkFrontFace vk_front_face_map[NGPU_FRONT_FACE_NB] = {
    [NGPU_FRONT_FACE_COUNTER_CLOCKWISE] = VK_FRONT_FACE_CLOCKWISE,
    [NGPU_FRONT_FACE_CLOCKWISE]         = VK_FRONT_FACE_COUNTER_CLOCKWISE,
};

static VkFrontFace get_vk_front_face(enum ngpu_front_face front_face)
{
    return vk_front_face_map[front_face];
}

static VkColorComponentFlags get_vk_color_write_mask(int color_write_mask)
{
    return (color_write_mask & NGPU_COLOR_COMPONENT_R_BIT ? VK_COLOR_COMPONENT_R_BIT : 0)
         | (color_write_mask & NGPU_COLOR_COMPONENT_G_BIT ? VK_COLOR_COMPONENT_G_BIT : 0)
         | (color_write_mask & NGPU_COLOR_COMPONENT_B_BIT ? VK_COLOR_COMPONENT_B_BIT : 0)
         | (color_write_mask & NGPU_COLOR_COMPONENT_A_BIT ? VK_COLOR_COMPONENT_A_BIT : 0);
}

static VkResult create_attribute_descs(struct ngpu_pipeline *s)
{
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    ngli_darray_init(&s_priv->vertex_attribute_descs, sizeof(VkVertexInputAttributeDescription), 0);
    ngli_darray_init(&s_priv->vertex_binding_descs,   sizeof(VkVertexInputBindingDescription), 0);

    const struct ngpu_pipeline_graphics *graphics = &s->graphics;
    const struct ngpu_vertex_state *state = &graphics->vertex_state;
    for (size_t i = 0; i < state->nb_buffers; i++) {
        const struct ngpu_vertex_buffer_layout *buffer = &state->buffers[i];

        const VkVertexInputBindingDescription binding_desc = {
            .binding   = (uint32_t)i,
            .stride    = (uint32_t)buffer->stride,
            .inputRate = buffer->rate ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        if (!ngli_darray_push(&s_priv->vertex_binding_descs, &binding_desc))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        for (size_t j = 0; j < buffer->nb_attributes; j++) {
            const struct ngpu_vertex_attribute *attribute = &buffer->attributes[j];
            const VkVertexInputAttributeDescription attr_desc = {
                .binding  = (uint32_t)i,
                .location = attribute->location,
                .format   = ngpu_format_ngl_to_vk(attribute->format),
                .offset   = (uint32_t)attribute->offset,
            };
            if (!ngli_darray_push(&s_priv->vertex_attribute_descs, &attr_desc))
                return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return VK_SUCCESS;
}

static VkResult pipeline_graphics_init(struct ngpu_pipeline *s)
{
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    const struct ngpu_pipeline_graphics *graphics = &s->graphics;
    const struct ngpu_graphics_state *state = &graphics->state;
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    s_priv->pipeline_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = (uint32_t)ngli_darray_count(&s_priv->vertex_binding_descs),
        .pVertexBindingDescriptions      = ngli_darray_data(&s_priv->vertex_binding_descs),
        .vertexAttributeDescriptionCount = (uint32_t)ngli_darray_count(&s_priv->vertex_attribute_descs),
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
        .frontFace   = get_vk_front_face(state->front_face),
    };

    const struct ngpu_rendertarget_layout *layout = &graphics->rt_layout;
    const VkSampleCountFlagBits samples = ngli_ngl_samples_to_vk(layout->samples);
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
            .failOp      = get_vk_stencil_op(state->stencil_front.fail),
            .passOp      = get_vk_stencil_op(state->stencil_front.depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_front.depth_fail),
            .compareOp   = get_vk_compare_op(state->stencil_front.func),
            .compareMask = state->stencil_front.read_mask,
            .writeMask   = state->stencil_front.write_mask,
            .reference   = state->stencil_front.ref,
        },
        .back = {
            .failOp      = get_vk_stencil_op(state->stencil_back.fail),
            .passOp      = get_vk_stencil_op(state->stencil_back.depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_back.depth_fail),
            .compareOp   = get_vk_compare_op(state->stencil_back.func),
            .compareMask = state->stencil_back.read_mask,
            .writeMask   = state->stencil_back.write_mask,
            .reference   = state->stencil_back.ref,
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };

    VkPipelineColorBlendAttachmentState colorblend_attachment_states[NGPU_MAX_COLOR_ATTACHMENTS] = {0};
    for (size_t i = 0; i < graphics->rt_layout.nb_colors; i++) {
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
        .attachmentCount = (uint32_t)graphics->rt_layout.nb_colors,
        .pAttachments    = colorblend_attachment_states,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (uint32_t)NGLI_ARRAY_NB(dynamic_states),
        .pDynamicStates    = dynamic_states,
    };

    const struct ngpu_program_vk *program_vk = (struct ngpu_program_vk *)s->program;
    const VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = program_vk->shaders[NGPU_PROGRAM_SHADER_VERT],
            .pName  = "main",
        }, {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = program_vk->shaders[NGPU_PROGRAM_SHADER_FRAG],
            .pName  = "main",
        },
    };

    VkRenderPass render_pass;
    VkResult res = ngpu_vk_create_compatible_renderpass(s->gpu_ctx, &graphics->rt_layout, &render_pass);
    if (res != VK_SUCCESS)
        return res;

    const VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = (uint32_t)NGLI_ARRAY_NB(shader_stage_create_info),
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

static VkResult pipeline_compute_init(struct ngpu_pipeline *s)
{
    const struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    const struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    s_priv->pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

    const struct ngpu_program_vk *program_vk = (struct ngpu_program_vk *)s->program;
    const VkPipelineShaderStageCreateInfo shader_stage_create_info = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = program_vk->shaders[NGPU_PROGRAM_SHADER_COMP],
        .pName  = "main",
    };

    const VkComputePipelineCreateInfo pipeline_create_info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = shader_stage_create_info,
        .layout = s_priv->pipeline_layout,
    };

    return vkCreateComputePipelines(vk->device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &s_priv->pipeline);
}

static VkResult create_pipeline_layout(struct ngpu_pipeline *s)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    struct ngpu_bindgroup_layout_vk *layout = (struct ngpu_bindgroup_layout_vk *)s->layout.bindgroup_layout;

    const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = layout->desc_set_layout  ? 1 : 0,
        .pSetLayouts    = &layout->desc_set_layout,
    };

    return vkCreatePipelineLayout(vk->device, &pipeline_layout_create_info, NULL, &s_priv->pipeline_layout);
}

static VkResult create_pipeline(struct ngpu_pipeline *s)
{
    VkResult res = create_pipeline_layout(s);
    if (res != VK_SUCCESS)
        return res;

    if (s->type == NGPU_PIPELINE_TYPE_GRAPHICS) {
        res = pipeline_graphics_init(s);
    } else if (s->type == NGPU_PIPELINE_TYPE_COMPUTE) {
        res = pipeline_compute_init(s);
    } else {
        ngli_assert(0);
    }
    return res;
}

struct ngpu_pipeline *ngpu_pipeline_vk_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_pipeline_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct ngpu_pipeline *)s;
}

static VkResult pipeline_vk_init(struct ngpu_pipeline *s)
{
    if (s->type == NGPU_PIPELINE_TYPE_GRAPHICS) {
        VkResult res = create_attribute_descs(s);
        if (res != VK_SUCCESS)
            return res;
    }

    return create_pipeline(s);
}

int ngpu_pipeline_vk_init(struct ngpu_pipeline *s)
{
    VkResult res = pipeline_vk_init(s);
    if (res != VK_SUCCESS)
        LOG(ERROR, "unable to initialize pipeline: %s", ngli_vk_res2str(res));
    return ngli_vk_res2ret(res);
}

static int prepare_and_bind_descriptor_set(struct ngpu_pipeline *s, VkCommandBuffer cmd_buf)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)gpu_ctx;
    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    if (!gpu_ctx->bindgroup)
        return 0;

    ngpu_bindgroup_vk_update_descriptor_set(gpu_ctx->bindgroup);

    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, gpu_ctx->bindgroup);
    struct ngpu_bindgroup_vk *bindgroup_vk = (struct ngpu_bindgroup_vk *)gpu_ctx->bindgroup;
    if (bindgroup_vk->desc_set) {
        struct buffer_binding_vk *bindings = ngli_darray_data(&bindgroup_vk->buffer_bindings);
        for (size_t i = 0; i < ngli_darray_count(&bindgroup_vk->buffer_bindings); i++) {
            struct buffer_binding_vk *binding = &bindings[i];
            ngpu_cmd_buffer_vk_ref_buffer(cmd_buffer_vk, (struct ngpu_buffer *)binding->buffer);
        }
        vkCmdBindDescriptorSets(cmd_buf, s_priv->pipeline_bind_point, s_priv->pipeline_layout, 0,
                                1, &bindgroup_vk->desc_set,
                                (uint32_t)gpu_ctx->nb_dynamic_offsets, gpu_ctx->dynamic_offsets);
    }

    return 0;
}

static int prepare_and_bind_graphics_pipeline(struct ngpu_pipeline *s, VkCommandBuffer cmd_buf)
{
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    vkCmdBindPipeline(cmd_buf, s_priv->pipeline_bind_point, s_priv->pipeline);

    return 0;
}

void ngpu_pipeline_vk_draw(struct ngpu_pipeline *s, uint32_t nb_vertices, uint32_t nb_instances, uint32_t first_vertex)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    VkCommandBuffer cmd_buf = cmd_buffer_vk->cmd_buf;

    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, s);

    int ret = prepare_and_bind_descriptor_set(s, cmd_buf);
    if (ret < 0)
        return;

    ret = prepare_and_bind_graphics_pipeline(s, cmd_buf);
    if (ret < 0)
        return;

    vkCmdDraw(cmd_buf, nb_vertices, nb_instances, first_vertex, 0);
}

void ngpu_pipeline_vk_draw_indexed(struct ngpu_pipeline *s, uint32_t nb_vertices, uint32_t nb_instances)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    VkCommandBuffer cmd_buf = cmd_buffer_vk->cmd_buf;
    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, s);

    int ret = prepare_and_bind_descriptor_set(s, cmd_buf);
    if (ret < 0)
        return;

    ret = prepare_and_bind_graphics_pipeline(s, cmd_buf);
    if (ret < 0)
        return;

    vkCmdDrawIndexed(cmd_buf, nb_vertices, nb_instances, 0, 0, 0);
}

void ngpu_pipeline_vk_dispatch(struct ngpu_pipeline *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    struct ngpu_cmd_buffer_vk *cmd_buffer_vk = gpu_ctx_vk->cur_cmd_buffer;
    const int cmd_is_transient = cmd_buffer_vk ? 0 : 1;
    if (cmd_is_transient) {
        VkResult res = ngpu_cmd_buffer_vk_begin_transient(s->gpu_ctx, 0, &cmd_buffer_vk);
        if (res != VK_SUCCESS)
            return;
    }
    VkCommandBuffer cmd_buf = cmd_buffer_vk->cmd_buf;
    NGPU_CMD_BUFFER_VK_REF(cmd_buffer_vk, s);

    int ret = prepare_and_bind_descriptor_set(s, cmd_buf);
    if (ret < 0)
        return;

    vkCmdBindPipeline(cmd_buf, s_priv->pipeline_bind_point, s_priv->pipeline);
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

    if (cmd_is_transient) {
        ngpu_cmd_buffer_vk_execute_transient(&cmd_buffer_vk);
    }
}

void ngpu_pipeline_vk_freep(struct ngpu_pipeline **sp)
{
    if (!*sp)
        return;

    struct ngpu_pipeline *s = *sp;
    struct ngpu_pipeline_vk *s_priv = (struct ngpu_pipeline_vk *)s;

    ngli_darray_reset(&s_priv->vertex_attribute_descs);
    ngli_darray_reset(&s_priv->vertex_binding_descs);

    struct ngpu_ctx_vk *gpu_ctx_vk = (struct ngpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    vkDestroyPipeline(vk->device, s_priv->pipeline, NULL);
    vkDestroyPipelineLayout(vk->device, s_priv->pipeline_layout, NULL);

    ngli_freep(sp);
}
