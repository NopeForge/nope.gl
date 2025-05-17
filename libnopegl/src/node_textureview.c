/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <stddef.h>

#include "internal.h"
#include "log.h"
#include "node_texture.h"
#include "node_textureview.h"
#include "nopegl.h"

#define OFFSET(x) offsetof(struct textureview_opts, x)
static const struct node_param textureview_params[] = {
    {"texture", NGLI_PARAM_TYPE_NODE, OFFSET(texture),
                .flags = NGLI_PARAM_FLAG_NON_NULL,
                .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURE2DARRAY, NGL_NODE_TEXTURE3D, NGL_NODE_TEXTURECUBE, NGLI_NODE_NONE},
                .desc=NGLI_DOCSTRING("texture used for the view")},
    {"layer",   NGLI_PARAM_TYPE_U32, OFFSET(layer),
                .desc=NGLI_DOCSTRING("texture layer used for the view")},
    {NULL}
};

static int textureview_init(struct ngl_node *node)
{
    const struct textureview_opts *o = node->opts;

    if (o->texture->cls->id == NGL_NODE_TEXTURE2D && o->layer >= 1) {
        LOG(ERROR, "2d textures only have one layer");
        return NGL_ERROR_INVALID_ARG;
    }

    const struct texture_info *texture_info = o->texture->priv_data;
    const struct ngpu_texture_params *texture_params = &texture_info->params;
    if (o->texture->cls->id == NGL_NODE_TEXTURE2DARRAY && o->layer >= texture_params->depth) {
        LOG(ERROR, "requested layer (%d) exceeds texture 2D array layer count (%d)", o->layer, texture_params->depth);
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->texture->cls->id == NGL_NODE_TEXTURECUBE && o->layer >= 6) {
        LOG(ERROR, "requested layer (%d) exceeds cube map layer count (6)", o->layer);
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->texture->cls->id == NGL_NODE_TEXTURE3D && o->layer >= texture_params->depth) {
        LOG(ERROR, "requested layer (%d) exceeds texture 3D layer count (%d)", o->layer, texture_params->depth);
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

const struct node_class ngli_textureview_class = {
    .id        = NGL_NODE_TEXTUREVIEW,
    .name      = "TextureView",
    .opts_size = sizeof(struct textureview_opts),
    .params    = textureview_params,
    .init      = textureview_init,
    .update    = ngli_node_update_children,
    .file      = __FILE__,
};
