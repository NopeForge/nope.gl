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

#include <stddef.h>

#include "gpu_ctx.h"
#include "log.h"
#include "node_resourceprops.h"
#include "nopegl.h"
#include "internal.h"
#include "precision.h"

#define OFFSET(x) offsetof(struct resourceprops_opts, x)
static const struct node_param resourceprops_params[] = {
    {"precision", NGLI_PARAM_TYPE_SELECT, OFFSET(precision), {.i32=NGLI_PRECISION_AUTO},
                  .choices=&ngli_precision_choices,
                  .desc=NGLI_DOCSTRING("precision qualifier for the shader")},
    {"as_image", NGLI_PARAM_TYPE_BOOL, OFFSET(as_image),
                 .desc=NGLI_DOCSTRING("flag this resource for image accessing (only applies to texture nodes)")},
    {"writable", NGLI_PARAM_TYPE_BOOL, OFFSET(writable),
                 .desc=NGLI_DOCSTRING("flag this resource as writable in the shader")},
    {NULL}
};

static int resourceprops_init(struct ngl_node *node)
{
    const struct ngl_ctx *ctx = node->ctx;
    const struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct resourceprops_opts *o = node->opts;

    if (o->as_image && !(gpu_ctx->features & NGLI_FEATURE_IMAGE_LOAD_STORE)) {
        LOG(ERROR, "context does not support image load store operations");
        return NGL_ERROR_GRAPHICS_UNSUPPORTED;
    }

    return 0;
}

static const char * const precisions_map[] = {
    [NGLI_PRECISION_HIGH]   = "high",
    [NGLI_PRECISION_MEDIUM] = "medium",
    [NGLI_PRECISION_LOW]    = "low",
};

static char *resourceprops_info_str(const struct ngl_node *node)
{
    const struct resourceprops_opts *o = node->opts;
    struct bstr *b = ngli_bstr_create();

    if (!b)
        return NULL;

    if (o->precision != NGLI_PRECISION_AUTO)
        ngli_bstr_printf(b, "precision:%s", precisions_map[o->precision]);
    if (o->as_image)
        ngli_bstr_printf(b, "%sas_image", ngli_bstr_len(b) ? " " : "");
    if (o->writable)
        ngli_bstr_printf(b, "%swritable", ngli_bstr_len(b) ? " " : "");

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

const struct node_class ngli_resourceprops_class = {
    .id        = NGL_NODE_RESOURCEPROPS,
    .name      = "ResourceProps",
    .init      = resourceprops_init,
    .info_str  = resourceprops_info_str,
    .opts_size = sizeof(struct resourceprops_opts),
    .params    = resourceprops_params,
    .file      = __FILE__,
};
