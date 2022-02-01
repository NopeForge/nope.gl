/*
 * Copyright 2022 GoPro Inc.
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

#include "pipeline_utils.h"

void ngli_pipeline_utils_update_texture(struct pipeline *pipeline, const struct pgcraft_texture_info *info)
{
    const struct pgcraft_texture_info_field *fields = info->fields;
    const struct image *image = info->image;

    ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
    ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);
    ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_TIMESTAMP].index, &image->ts);

    if (image->params.layout) {
        const float dimensions[] = {image->params.width, image->params.height, image->params.depth};
        ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_DIMENSIONS].index, dimensions);
    }

    const struct texture *textures[NGLI_INFO_FIELD_NB] = {0};
    switch (image->params.layout) {
    case NGLI_IMAGE_LAYOUT_DEFAULT:
        textures[NGLI_INFO_FIELD_SAMPLER_0] = image->planes[0];
        break;
    case NGLI_IMAGE_LAYOUT_NV12:
        textures[NGLI_INFO_FIELD_SAMPLER_0] = image->planes[0];
        textures[NGLI_INFO_FIELD_SAMPLER_1] = image->planes[1];
        break;
    case NGLI_IMAGE_LAYOUT_NV12_RECTANGLE:
        textures[NGLI_INFO_FIELD_SAMPLER_RECT_0] = image->planes[0];
        textures[NGLI_INFO_FIELD_SAMPLER_RECT_1] = image->planes[1];
        break;
    case NGLI_IMAGE_LAYOUT_MEDIACODEC:
        textures[NGLI_INFO_FIELD_SAMPLER_OES] = image->planes[0];
        break;
    case NGLI_IMAGE_LAYOUT_YUV:
        textures[NGLI_INFO_FIELD_SAMPLER_0] = image->planes[0];
        textures[NGLI_INFO_FIELD_SAMPLER_1] = image->planes[1];
        textures[NGLI_INFO_FIELD_SAMPLER_2] = image->planes[2];
        break;
    case NGLI_IMAGE_LAYOUT_RECTANGLE:
        textures[NGLI_INFO_FIELD_SAMPLER_RECT_0] = image->planes[0];
        break;
    default:
        break;
    }

    static const int samplers[] = {
        NGLI_INFO_FIELD_SAMPLER_0,
        NGLI_INFO_FIELD_SAMPLER_1,
        NGLI_INFO_FIELD_SAMPLER_2,
        NGLI_INFO_FIELD_SAMPLER_OES,
        NGLI_INFO_FIELD_SAMPLER_RECT_0,
        NGLI_INFO_FIELD_SAMPLER_RECT_1,
    };

    int ret = 1;
    for (int i = 0; i < NGLI_ARRAY_NB(samplers); i++) {
        const int sampler = samplers[i];
        const int index = fields[sampler].index;
        const struct texture *texture = textures[sampler];
        ret &= ngli_pipeline_update_texture(pipeline, index, texture);
    };

    const int layout = ret < 0 ? NGLI_IMAGE_LAYOUT_NONE : image->params.layout;
    ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &layout);
}
