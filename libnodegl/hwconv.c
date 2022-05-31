/*
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

#include "buffer.h"
#include "hwconv.h"
#include "gpu_ctx.h"
#include "image.h"
#include "log.h"
#include "internal.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "texture.h"
#include "topology.h"
#include "type.h"
#include "utils.h"

/* GLSL fragments as string */
#include "hdr_hlg2sdr_frag.h"
#include "hdr_pq2sdr_frag.h"

static const char *default_vert_base =
    "void main()"                                                               "\n"
    "{"                                                                         "\n"
    "    ngl_out_pos = vec4(position.xy, 0.0, 1.0);"                            "\n"
    "    var_tex_coord = (tex_coord_matrix * vec4(position.zw, 0.0, 1.0)).xy;"  "\n"
    "}";

static const char *default_frag_base =
    "void main()"                                                               "\n"
    "{"                                                                         "\n"
    "    ngl_out_color = ngl_texvideo(tex, var_tex_coord);"                     "\n"
    "}";

static const struct pgcraft_iovar vert_out_vars[] = {
    {.name = "var_tex_coord", .type = NGLI_TYPE_VEC2},
};

int ngli_hwconv_init(struct hwconv *hwconv, struct ngl_ctx *ctx,
                     const struct image *dst_image,
                     const struct image_params *src_params)
{
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    hwconv->ctx = ctx;
    hwconv->src_params = *src_params;

    if (dst_image->params.layout != NGLI_IMAGE_LAYOUT_DEFAULT) {
        LOG(ERROR, "unsupported output image layout: 0x%x", dst_image->params.layout);
        return NGL_ERROR_UNSUPPORTED;
    }

    struct texture *texture = dst_image->planes[0];
    const struct texture_params *texture_params = &texture->params;

    const struct rendertarget_desc rt_desc = {
        .nb_colors = 1,
        .colors[0].format = texture_params->format,
    };
    const struct rendertarget_params rt_params = {
        .width = dst_image->params.width,
        .height = dst_image->params.height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = texture,
            .load_op    = NGLI_LOAD_OP_CLEAR,
            .store_op   = NGLI_STORE_OP_STORE,
        }
    };
    hwconv->rt = ngli_rendertarget_create(gpu_ctx);
    if (!hwconv->rt)
        return NGL_ERROR_MEMORY;
    int ret = ngli_rendertarget_init(hwconv->rt, &rt_params);
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

    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    hwconv->vertices = ngli_buffer_create(gpu_ctx);
    if (!hwconv->vertices)
        return NGL_ERROR_MEMORY;
    ret = ngli_buffer_init(hwconv->vertices, sizeof(vertices), NGLI_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                               NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(hwconv->vertices, vertices, sizeof(vertices), 0);
    if (ret < 0)
        return ret;

    struct pgcraft_texture textures[] = {
        {.name = "tex", .type = NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO, .stage = NGLI_PROGRAM_SHADER_FRAG},
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * 4,
            .buffer   = hwconv->vertices,
        },
    };

    const char *vert_base = default_vert_base;
    const char *frag_base = default_frag_base;

    const struct color_info *src_color_info = &src_params->color_info;
    if (src_color_info->space == SXPLAYER_COL_SPC_BT2020_NCL) {
        if (src_color_info->transfer == SXPLAYER_COL_TRC_ARIB_STD_B67) { // HLG
            frag_base = hdr_hlg2sdr_frag;
        } else if (src_color_info->transfer == SXPLAYER_COL_TRC_SMPTE2084)  { // PQ
            frag_base = hdr_pq2sdr_frag;
        }
    }

    const struct pgcraft_params crafter_params = {
        .program_label    = "nodegl/hwconv",
        .vert_base        = vert_base,
        .frag_base        = frag_base,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    hwconv->crafter = ngli_pgcraft_create(ctx);
    if (!hwconv->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcraft_craft(hwconv->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    hwconv->pipeline_compat = ngli_pipeline_compat_create(gpu_ctx);
    if (!hwconv->pipeline_compat)
        return NGL_ERROR_MEMORY;

    const struct pipeline_params pipeline_params = {
        .type         = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state    = NGLI_GRAPHICSTATE_DEFAULTS,
            .rt_desc  = rt_desc,
        },
        .program      = ngli_pgcraft_get_program(hwconv->crafter),
        .layout       = ngli_pgcraft_get_pipeline_layout(hwconv->crafter),
    };

    const struct pipeline_resources pipeline_resources = ngli_pgcraft_get_pipeline_resources(hwconv->crafter);
    const struct pgcraft_compat_info *compat_info = ngli_pgcraft_get_compat_info(hwconv->crafter);

    const struct pipeline_compat_params params = {
        .params = &pipeline_params,
        .resources = &pipeline_resources,
        .compat_info = compat_info,
    };

    ret = ngli_pipeline_compat_init(hwconv->pipeline_compat, &params);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_hwconv_convert_image(struct hwconv *hwconv, const struct image *image)
{
    struct ngl_ctx *ctx = hwconv->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    ngli_assert(hwconv->src_params.layout == image->params.layout);

    struct rendertarget *rt = hwconv->rt;
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, rt);

    int prev_vp[4] = {0};
    ngli_gpu_ctx_get_viewport(gpu_ctx, prev_vp);

    const int vp[4] = {0, 0, rt->width, rt->height};
    ngli_gpu_ctx_set_viewport(gpu_ctx, vp);

    struct pipeline_compat *pipeline = hwconv->pipeline_compat;

    const struct darray *texture_infos_array = ngli_pgcraft_get_texture_infos(hwconv->crafter);
    const struct pgcraft_texture_info *info = ngli_darray_data(texture_infos_array);
    ngli_assert(ngli_darray_count(texture_infos_array) == 1);

    const struct pgcraft_texture_info_field *fields = info->fields;

    if (image->params.layout) {
        const float dimensions[] = {image->params.width, image->params.height, image->params.depth};
        ngli_pipeline_compat_update_uniform(pipeline, fields[NGLI_INFO_FIELD_DIMENSIONS].index, dimensions);
    }

    int ret = -1;
    switch (image->params.layout) {
    case NGLI_IMAGE_LAYOUT_DEFAULT:
        ret = ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_0].index, image->planes[0]);
        break;
    case NGLI_IMAGE_LAYOUT_NV12:
        ret = ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_0].index, image->planes[0]);
        ret &= ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_1].index, image->planes[1]);
        break;
    case NGLI_IMAGE_LAYOUT_NV12_RECTANGLE:
        ret = ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_RECT_0].index, image->planes[0]);
        ret &= ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_RECT_1].index, image->planes[1]);
        break;
    case NGLI_IMAGE_LAYOUT_MEDIACODEC:
        ret = ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_OES].index, image->planes[0]);
        break;
    case NGLI_IMAGE_LAYOUT_YUV:
        ret = ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_0].index, image->planes[0]);
        ret &= ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_1].index, image->planes[1]);
        ret &= ngli_pipeline_compat_update_texture(pipeline, fields[NGLI_INFO_FIELD_SAMPLER_2].index, image->planes[2]);
        break;
    default:
        ngli_assert(0);
    }
    ngli_assert(ret == 0);
    ngli_pipeline_compat_update_uniform(pipeline, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &image->params.layout);
    ngli_pipeline_compat_update_uniform(pipeline, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
    ngli_pipeline_compat_update_uniform(pipeline, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);

    ngli_pipeline_compat_draw(pipeline, 4, 1);

    ngli_gpu_ctx_end_render_pass(gpu_ctx);
    ngli_gpu_ctx_set_viewport(gpu_ctx, prev_vp);

    return 0;
}

void ngli_hwconv_reset(struct hwconv *hwconv)
{
    struct ngl_ctx *ctx = hwconv->ctx;
    if (!ctx)
        return;

    ngli_pipeline_compat_freep(&hwconv->pipeline_compat);
    ngli_pgcraft_freep(&hwconv->crafter);
    ngli_buffer_freep(&hwconv->vertices);
    ngli_rendertarget_freep(&hwconv->rt);

    memset(hwconv, 0, sizeof(*hwconv));
}
