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

#include "internal.h"
#include "node_io.h"
#include "node_program.h"
#include "nopegl.h"
#include "ngpu/pgcraft.h"
#include "utils/darray.h"

#define IO_NODES (const uint32_t[]){NGL_NODE_IOINT,        \
                                    NGL_NODE_IOIVEC2,      \
                                    NGL_NODE_IOIVEC3,      \
                                    NGL_NODE_IOIVEC4,      \
                                    NGL_NODE_IOUINT,       \
                                    NGL_NODE_IOUIVEC2,     \
                                    NGL_NODE_IOUIVEC3,     \
                                    NGL_NODE_IOUIVEC4,     \
                                    NGL_NODE_IOFLOAT,      \
                                    NGL_NODE_IOVEC2,       \
                                    NGL_NODE_IOVEC3,       \
                                    NGL_NODE_IOVEC4,       \
                                    NGL_NODE_IOMAT3,       \
                                    NGL_NODE_IOMAT4,       \
                                    NGL_NODE_IOBOOL,       \
                                    NGLI_NODE_NONE}

#define OFFSET(x) offsetof(struct program_opts, x)
static const struct node_param program_params[] = {
    {"vertex",   NGLI_PARAM_TYPE_STR, OFFSET(vertex),   {.str=NULL},
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("vertex shader")},
    {"fragment", NGLI_PARAM_TYPE_STR, OFFSET(fragment), {.str=NULL},
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("fragment shader")},
    {"properties", NGLI_PARAM_TYPE_NODEDICT, OFFSET(properties),
                   .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                   .node_types=(const uint32_t[]){NGL_NODE_RESOURCEPROPS, NGLI_NODE_NONE},
                   .desc=NGLI_DOCSTRING("resource properties")},
    {"vert_out_vars", NGLI_PARAM_TYPE_NODEDICT, OFFSET(vert_out_vars),
                       .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                       .node_types=IO_NODES,
                       .desc=NGLI_DOCSTRING("in/out communication variables shared between vertex and fragment stages")},
    {"nb_frag_output", NGLI_PARAM_TYPE_U32, OFFSET(nb_frag_output),
                       .desc=NGLI_DOCSTRING("number of color outputs in the fragment shader")},
    {NULL}
};

static int program_init(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;
    const struct program_opts *o = node->opts;

    ngli_darray_init(&s->vert_out_vars_array, sizeof(struct ngpu_pgcraft_iovar), 0);
    if (o->vert_out_vars) {
        const struct hmap_entry *e = NULL;
        while ((e = ngli_hmap_next(o->vert_out_vars, e))) {
            const struct ngl_node *iovar_node = e->data;
            const struct io_priv *iovar_priv = iovar_node->priv_data;
            const struct io_opts *iovar_opts = iovar_node->opts;
            struct ngpu_pgcraft_iovar iovar = {
                .type = iovar_priv->type,
                .precision_in = iovar_opts->precision_in,
                .precision_out = iovar_opts->precision_out,
            };
            snprintf(iovar.name, sizeof(iovar.name), "%s", e->key.str);
            if (!ngli_darray_push(&s->vert_out_vars_array, &iovar))
                return NGL_ERROR_MEMORY;
        }
    }

    return 0;
}

static void program_uninit(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;
    ngli_darray_reset(&s->vert_out_vars_array);
}

const struct node_class ngli_program_class = {
    .id        = NGL_NODE_PROGRAM,
    .name      = "Program",
    .init      = program_init,
    .uninit    = program_uninit,
    .opts_size = sizeof(struct program_opts),
    .priv_size = sizeof(struct program_priv),
    .params    = program_params,
    .file      = __FILE__,
};
