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

#include <stddef.h>

#include "log.h"
#include "internal.h"
#include "nodegl.h"

#define OFFSET(x) offsetof(struct textureview_priv, opts.x)
static const struct node_param textureview_params[] = {
    {"texture", NGLI_PARAM_TYPE_NODE, OFFSET(texture),
                .flags = NGLI_PARAM_FLAG_NON_NULL,
                .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURECUBE, -1},
                .desc=NGLI_DOCSTRING("texture used for the view")},
    {"layer",   NGLI_PARAM_TYPE_I32, OFFSET(layer),
                .desc=NGLI_DOCSTRING("texture layer used for the view")},
    {NULL}
};

static int textureview_init(struct ngl_node *node)
{
    struct textureview_priv *s = node->priv_data;
    const struct textureview_opts *o = &s->opts;

    if (o->layer < 0) {
        LOG(ERROR, "layer cannot be negative");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->texture->cls->id == NGL_NODE_TEXTURE2D && o->layer) {
        LOG(ERROR, "2d textures only have one layer");
        return NGL_ERROR_INVALID_ARG;
    }

    if (o->texture->cls->id == NGL_NODE_TEXTURECUBE && o->layer >= 6) {
        LOG(ERROR, "cubemap textures only have 6 layers");
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

const struct node_class ngli_textureview_class = {
    .id        = NGL_NODE_TEXTUREVIEW,
    .name      = "TextureView",
    .priv_size = sizeof(struct textureview_priv),
    .params    = textureview_params,
    .init      = textureview_init,
    .update    = ngli_node_update_children,
    .file      = __FILE__,
};
