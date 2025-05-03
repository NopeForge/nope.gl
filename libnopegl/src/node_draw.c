/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "blending.h"
#include "geometry.h"
#include "internal.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "ngpu/limits.h"
#include "node_buffer.h"
#include "node_program.h"
#include "nopegl.h"
#include "pass.h"
#include "utils/hmap.h"
#include "utils/utils.h"

struct draw_opts {
    struct ngl_node *geometry;
    struct ngl_node *program;
    struct hmap *vert_resources;
    struct hmap *frag_resources;
    struct hmap *attributes;
    struct hmap *instance_attributes;
    int32_t nb_instances;
    enum ngli_blending blending;
};

struct draw_priv {
    struct pass pass;
};

#define PROGRAMS_TYPES_LIST (const uint32_t[]){NGL_NODE_PROGRAM,         \
                                               NGLI_NODE_NONE}

#define INPUT_TYPES_LIST    (const uint32_t[]){NGL_NODE_TEXTURE2D,           \
                                               NGL_NODE_TEXTURE2DARRAY,      \
                                               NGL_NODE_TEXTURE3D,           \
                                               NGL_NODE_TEXTURECUBE,         \
                                               NGL_NODE_BLOCK,               \
                                               NGL_NODE_COLORSTATS,          \
                                               NGL_NODE_BUFFERFLOAT,         \
                                               NGL_NODE_BUFFERVEC2,          \
                                               NGL_NODE_BUFFERVEC3,          \
                                               NGL_NODE_BUFFERVEC4,          \
                                               NGL_NODE_NOISEFLOAT,          \
                                               NGL_NODE_NOISEVEC2,           \
                                               NGL_NODE_NOISEVEC3,           \
                                               NGL_NODE_NOISEVEC4,           \
                                               NGL_NODE_EVALFLOAT,           \
                                               NGL_NODE_EVALVEC2,            \
                                               NGL_NODE_EVALVEC3,            \
                                               NGL_NODE_EVALVEC4,            \
                                               NGL_NODE_STREAMEDBUFFERINT,   \
                                               NGL_NODE_STREAMEDBUFFERIVEC2, \
                                               NGL_NODE_STREAMEDBUFFERIVEC3, \
                                               NGL_NODE_STREAMEDBUFFERIVEC4, \
                                               NGL_NODE_STREAMEDBUFFERUINT,  \
                                               NGL_NODE_STREAMEDBUFFERUIVEC2,\
                                               NGL_NODE_STREAMEDBUFFERUIVEC3,\
                                               NGL_NODE_STREAMEDBUFFERUIVEC4,\
                                               NGL_NODE_STREAMEDBUFFERFLOAT, \
                                               NGL_NODE_STREAMEDBUFFERVEC2,  \
                                               NGL_NODE_STREAMEDBUFFERVEC3,  \
                                               NGL_NODE_STREAMEDBUFFERVEC4,  \
                                               NGL_NODE_UNIFORMBOOL,         \
                                               NGL_NODE_UNIFORMFLOAT,        \
                                               NGL_NODE_UNIFORMVEC2,         \
                                               NGL_NODE_UNIFORMVEC3,         \
                                               NGL_NODE_UNIFORMVEC4,         \
                                               NGL_NODE_UNIFORMCOLOR,        \
                                               NGL_NODE_UNIFORMQUAT,         \
                                               NGL_NODE_UNIFORMINT,          \
                                               NGL_NODE_UNIFORMIVEC2,        \
                                               NGL_NODE_UNIFORMIVEC3,        \
                                               NGL_NODE_UNIFORMIVEC4,        \
                                               NGL_NODE_UNIFORMUINT,         \
                                               NGL_NODE_UNIFORMUIVEC2,       \
                                               NGL_NODE_UNIFORMUIVEC3,       \
                                               NGL_NODE_UNIFORMUIVEC4,       \
                                               NGL_NODE_UNIFORMMAT4,         \
                                               NGL_NODE_ANIMATEDFLOAT,       \
                                               NGL_NODE_ANIMATEDVEC2,        \
                                               NGL_NODE_ANIMATEDVEC3,        \
                                               NGL_NODE_ANIMATEDVEC4,        \
                                               NGL_NODE_ANIMATEDQUAT,        \
                                               NGL_NODE_ANIMATEDCOLOR,       \
                                               NGL_NODE_STREAMEDINT,         \
                                               NGL_NODE_STREAMEDIVEC2,       \
                                               NGL_NODE_STREAMEDIVEC3,       \
                                               NGL_NODE_STREAMEDIVEC4,       \
                                               NGL_NODE_STREAMEDUINT,        \
                                               NGL_NODE_STREAMEDUIVEC2,      \
                                               NGL_NODE_STREAMEDUIVEC3,      \
                                               NGL_NODE_STREAMEDUIVEC4,      \
                                               NGL_NODE_STREAMEDFLOAT,       \
                                               NGL_NODE_STREAMEDVEC2,        \
                                               NGL_NODE_STREAMEDVEC3,        \
                                               NGL_NODE_STREAMEDVEC4,        \
                                               NGL_NODE_STREAMEDMAT4,        \
                                               NGL_NODE_TIME,                \
                                               NGL_NODE_VELOCITYFLOAT,       \
                                               NGL_NODE_VELOCITYVEC2,        \
                                               NGL_NODE_VELOCITYVEC3,        \
                                               NGL_NODE_VELOCITYVEC4,        \
                                               NGLI_NODE_NONE}

#define ATTRIBUTES_TYPES_LIST (const uint32_t[]){NGL_NODE_BUFFERFLOAT,   \
                                                 NGL_NODE_BUFFERVEC2,    \
                                                 NGL_NODE_BUFFERVEC3,    \
                                                 NGL_NODE_BUFFERVEC4,    \
                                                 NGL_NODE_BUFFERMAT4,    \
                                                 NGLI_NODE_NONE}

#define GEOMETRY_TYPES_LIST (const uint32_t[]){NGL_NODE_CIRCLE,          \
                                               NGL_NODE_GEOMETRY,        \
                                               NGL_NODE_QUAD,            \
                                               NGL_NODE_TRIANGLE,        \
                                               NGLI_NODE_NONE}

#define OFFSET(x) offsetof(struct draw_opts, x)
static const struct node_param render_params[] = {
    {"geometry", NGLI_PARAM_TYPE_NODE, OFFSET(geometry), .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .node_types=GEOMETRY_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("geometry to be rasterized")},
    {"program",  NGLI_PARAM_TYPE_NODE, OFFSET(program),
                 .node_types=PROGRAMS_TYPES_LIST,
                 .flags=NGLI_PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("program to be executed")},
    {"vert_resources", NGLI_PARAM_TYPE_NODEDICT, OFFSET(vert_resources),
                         .node_types=INPUT_TYPES_LIST,
                         .desc=NGLI_DOCSTRING("resources made accessible to the vertex stage of the `program`")},
    {"frag_resources", NGLI_PARAM_TYPE_NODEDICT, OFFSET(frag_resources),
                           .node_types=INPUT_TYPES_LIST,
                           .desc=NGLI_DOCSTRING("resources made accessible to the fragment stage of the `program`")},
    {"attributes", NGLI_PARAM_TYPE_NODEDICT, OFFSET(attributes),
                 .node_types=ATTRIBUTES_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("extra vertex attributes made accessible to the `program`")},
    {"instance_attributes", NGLI_PARAM_TYPE_NODEDICT, OFFSET(instance_attributes),
                 .node_types=ATTRIBUTES_TYPES_LIST,
                 .desc=NGLI_DOCSTRING("per instance extra vertex attributes made accessible to the `program`")},
    {"nb_instances", NGLI_PARAM_TYPE_I32, OFFSET(nb_instances), {.i32 = 1},
                 .desc=NGLI_DOCSTRING("number of instances to draw")},
    {"blending", NGLI_PARAM_TYPE_SELECT, OFFSET(blending),
                 .choices=&ngli_blending_choices,
                 .desc=NGLI_DOCSTRING("define how this node and the current frame buffer are blended together")},
    {NULL}
};

static int check_params(const struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    const struct draw_opts *o = node->opts;

    const struct ngpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct ngpu_limits *limits = &gpu_ctx->limits;

    if (o->nb_instances < 1) {
        LOG(ERROR, "nb_instances must be > 0");
        return NGL_ERROR_INVALID_ARG;
    }

    const size_t nb_attributes = o->attributes ? ngli_hmap_count(o->attributes) : 0;
    if (nb_attributes > limits->max_vertex_attributes) {
        LOG(ERROR, "number of attributes (%zu) exceeds device limits (%u)",
            nb_attributes, limits->max_vertex_attributes);
        return NGL_ERROR_GRAPHICS_LIMIT_EXCEEDED;
    }

    const struct geometry *geometry = *(struct geometry **)o->geometry->priv_data;
    const int64_t max_indices = geometry->max_indices;

    const size_t nb_vertices = geometry->vertices_layout.count;

    if (o->attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(o->attributes, entry))) {
            const struct ngl_node *anode = entry->data;
            const struct buffer_info *buffer = anode->priv_data;

            if (geometry->indices_buffer) {
                if (max_indices >= buffer->layout.count) {
                    LOG(ERROR, "indices buffer contains values exceeding attribute buffer %s count (%" PRId64 " >= %zu)",
                        entry->key.str, max_indices, buffer->layout.count);
                    return NGL_ERROR_INVALID_ARG;
                }
            } else {
                if (buffer->layout.count != nb_vertices) {
                    LOG(ERROR, "attribute buffer %s count (%zu) does not match vertices count (%zu)",
                        entry->key.str, buffer->layout.count, nb_vertices);
                    return NGL_ERROR_INVALID_ARG;
                }
            }
        }
    }

    if (o->instance_attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(o->instance_attributes, entry))) {
            const struct ngl_node *anode = entry->data;
            const struct buffer_info *buffer = anode->priv_data;

            if (buffer->layout.count != o->nb_instances) {
                LOG(ERROR, "attribute buffer %s count (%zu) does not match instance count (%d)",
                    entry->key.str, buffer->layout.count, o->nb_instances);
                return NGL_ERROR_INVALID_ARG;
            }
        }
    }

    return 0;
}

static int render_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct draw_priv *s = node->priv_data;
    const struct draw_opts *o = node->opts;

    int ret = check_params(node);
    if (ret < 0)
        return ret;

    const struct program_priv *program_priv = o->program->priv_data;
    const struct program_opts *program_opts = o->program->opts;
    const struct geometry *geometry = *(struct geometry **)o->geometry->priv_data;
    struct pass_params params = {
        .label = node->label,
        .program_label = o->program->label,
        .geometry = geometry,
        .vert_base = program_opts->vertex,
        .frag_base = program_opts->fragment,
        .vert_resources = o->vert_resources,
        .frag_resources = o->frag_resources,
        .properties = program_opts->properties,
        .attributes = o->attributes,
        .instance_attributes = o->instance_attributes,
        .nb_instances = (uint32_t)o->nb_instances,
        .vert_out_vars = ngli_darray_data(&program_priv->vert_out_vars_array),
        .nb_vert_out_vars = ngli_darray_count(&program_priv->vert_out_vars_array),
        .nb_frag_output = program_opts->nb_frag_output,
        .blending = o->blending,
    };
    return ngli_pass_init(&s->pass, ctx, &params);
}

static int render_prepare(struct ngl_node *node)
{
    struct draw_priv *s = node->priv_data;

    int ret = ngli_node_prepare_children(node);
    if (ret < 0)
        return ret;

    return ngli_pass_prepare(&s->pass);
}

static void render_uninit(struct ngl_node *node)
{
    struct draw_priv *s = node->priv_data;
    ngli_pass_uninit(&s->pass);
}

static void render_draw(struct ngl_node *node)
{
    struct draw_priv *s = node->priv_data;
    ngli_node_draw_children(node);
    ngli_pass_exec(&s->pass);
}

const struct node_class ngli_draw_class = {
    .id        = NGL_NODE_DRAW,
    .category  = NGLI_NODE_CATEGORY_DRAW,
    .name      = "Draw",
    .init      = render_init,
    .prepare   = render_prepare,
    .uninit    = render_uninit,
    .update    = ngli_node_update_children,
    .draw      = render_draw,
    .opts_size = sizeof(struct draw_opts),
    .priv_size = sizeof(struct draw_priv),
    .params    = render_params,
    .file      = __FILE__,
};
