/*
 * Copyright 2023 Nope Forge
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

#define OFFSET(x) offsetof(struct fontface_opts, x)
static const struct node_param fontface_params[] = {
    {"path",  NGLI_PARAM_TYPE_STR, OFFSET(path),
              .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_FILEPATH,
              .desc=NGLI_DOCSTRING("path to the font file")},
    {NULL}
};

#if HAVE_TEXT_LIBRARIES
static int fontface_init(struct ngl_node *node)
{
    return 0;
}
#else
static int fontface_init(struct ngl_node *node)
{
    LOG(ERROR, "nope.gl is not compiled with text libraries support");
    return NGL_ERROR_UNSUPPORTED;
}
#endif

const struct node_class ngli_fontface_class = {
    .id        = NGL_NODE_FONTFACE,
    .name      = "FontFace",
    .init      = fontface_init,
    .opts_size = sizeof(struct fontface_opts),
    .params    = fontface_params,
    .file      = __FILE__,
};
