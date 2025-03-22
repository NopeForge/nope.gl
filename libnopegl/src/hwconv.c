/*
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

#include <string.h>

#include "hwconv.h"
#include "image.h"
#include "internal.h"
#include "log.h"
#include "ngpu/buffer.h"
#include "ngpu/ctx.h"
#include "ngpu/texture.h"
#include "ngpu/type.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "utils/utils.h"

/* GLSL fragments as string */
#include "hdr_hlg2sdr_frag.h"
#include "hdr_pq2sdr_frag.h"
#include "hwconv_frag.h"
#include "hwconv_vert.h"

static const struct pgcraft_iovar vert_out_vars[] = {
    {.name = "tex_coord", .type = NGPU_TYPE_VEC2},
};

int ngli_hwconv_init(struct hwconv *hwconv, struct ngl_ctx *ctx,
                     const struct image *dst_image,
                     const struct image_params *src_params)
{
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    hwconv->ctx = ctx;
    hwconv->src_params = *src_params;

    if (dst_image->params.layout != NGLI_IMAGE_LAYOUT_DEFAULT) {
        LOG(ERROR, "unsupported output image layout: 0x%x", dst_image->params.layout);
        return NGL_ERROR_UNSUPPORTED;
    }

    struct ngpu_texture *texture = dst_image->planes[0];
    const struct ngpu_texture_params *texture_params = &texture->params;

    const struct ngpu_rendertarget_layout rt_layout = {
        .nb_colors = 1,
        .colors[0].format = texture_params->format,
    };
    const struct ngpu_rendertarget_params rt_params = {
        .width = dst_image->params.width,
        .height = dst_image->params.height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = texture,
            .load_op    = NGPU_LOAD_OP_CLEAR,
            .store_op   = NGPU_STORE_OP_STORE,
        }
    };
    hwconv->rt = ngpu_rendertarget_create(gpu_ctx);
    if (!hwconv->rt)
        return NGL_ERROR_MEMORY;
    int ret = ngpu_rendertarget_init(hwconv->rt, &rt_params);
    if (ret < 0)
        return ret;

    const enum image_layout src_layout = src_params->layout;
    if (src_layout != NGLI_IMAGE_LAYOUT_DEFAULT &&
        src_layout != NGLI_IMAGE_LAYOUT_NV12 &&
        src_layout != NGLI_IMAGE_LAYOUT_YUV &&
        src_layout != NGLI_IMAGE_LAYOUT_NV12_RECTANGLE &&
        src_layout != NGLI_IMAGE_LAYOUT_MEDIACODEC) {
        LOG(ERROR, "unsupported texture layout: 0x%x", src_layout);
        return NGL_ERROR_UNSUPPORTED;
    }

    struct pgcraft_texture textures[] = {
        {.name = "tex", .type = NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO, .stage = NGPU_PROGRAM_SHADER_FRAG},
    };

    const char *vert_base = hwconv_vert;
    const char *frag_base = hwconv_frag;

    const struct color_info *src_color_info = &src_params->color_info;
    if (src_color_info->space == NMD_COL_SPC_BT2020_NCL) {
        if (src_color_info->transfer == NMD_COL_TRC_ARIB_STD_B67) { // HLG
            frag_base = hdr_hlg2sdr_frag;
        } else if (src_color_info->transfer == NMD_COL_TRC_SMPTE2084)  { // PQ
            frag_base = hdr_pq2sdr_frag;
        }
    }

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/hwconv",
        .vert_base        = vert_base,
        .frag_base        = frag_base,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    hwconv->crafter = ngli_pgcraft_create(gpu_ctx);
    if (!hwconv->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcraft_craft(hwconv->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    hwconv->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!hwconv->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_compat_params params = {
        .type         = NGPU_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGPU_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = rt_layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(hwconv->crafter),
        },
        .program          = ngli_pgcraft_get_program(hwconv->crafter),
        .layout_desc      = ngli_pgcraft_get_bindgroup_layout_desc(hwconv->crafter),
        .resources        = ngli_pgcraft_get_bindgroup_resources(hwconv->crafter),
        .vertex_resources = ngli_pgcraft_get_vertex_resources(hwconv->crafter),
        .compat_info      = ngli_pgcraft_get_compat_info(hwconv->crafter),
    };

    ret = ngli_pipeline_compat_init(hwconv->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_hwconv_convert_image(struct hwconv *hwconv, const struct image *image)
{
    struct ngl_ctx *ctx = hwconv->ctx;
    struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    ngli_assert(hwconv->src_params.layout == image->params.layout);

    struct ngpu_rendertarget *rt = hwconv->rt;
    struct pipeline_compat *pipeline = hwconv->pipeline_compat;

    ngpu_ctx_begin_render_pass(gpu_ctx, rt);

    ngli_pipeline_compat_update_image(pipeline, 0, image);
    ngli_pipeline_compat_draw(pipeline, 3, 1, 0);

    ngpu_ctx_end_render_pass(gpu_ctx);

    return 0;
}

void ngli_hwconv_reset(struct hwconv *hwconv)
{
    struct ngl_ctx *ctx = hwconv->ctx;
    if (!ctx)
        return;

    ngli_pipeline_compat_freep(&hwconv->pipeline_compat);
    ngli_pgcraft_freep(&hwconv->crafter);
    ngpu_rendertarget_freep(&hwconv->rt);

    memset(hwconv, 0, sizeof(*hwconv));
}
