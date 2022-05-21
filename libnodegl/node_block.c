/*
 * Copyright 2019-2022 GoPro Inc.
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
#include <stddef.h>

#include "block.h"
#include "buffer.h"
#include "gpu_ctx.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "internal.h"

static const struct param_choices layout_choices = {
    .name = "memory_layout",
    .consts = {
        {"std140", NGLI_BLOCK_LAYOUT_STD140, .desc=NGLI_DOCSTRING("standard uniform block memory layout 140")},
        {"std430", NGLI_BLOCK_LAYOUT_STD430, .desc=NGLI_DOCSTRING("standard uniform block memory layout 430")},
        {NULL}
    }
};

#define FIELD_TYPES_LIST (const int[]){NGL_NODE_ANIMATEDBUFFERFLOAT,    \
                                       NGL_NODE_ANIMATEDBUFFERVEC2,     \
                                       NGL_NODE_ANIMATEDBUFFERVEC3,     \
                                       NGL_NODE_ANIMATEDBUFFERVEC4,     \
                                       NGL_NODE_STREAMEDBUFFERINT,      \
                                       NGL_NODE_STREAMEDBUFFERIVEC2,    \
                                       NGL_NODE_STREAMEDBUFFERIVEC3,    \
                                       NGL_NODE_STREAMEDBUFFERIVEC4,    \
                                       NGL_NODE_STREAMEDBUFFERUINT,     \
                                       NGL_NODE_STREAMEDBUFFERUIVEC2,   \
                                       NGL_NODE_STREAMEDBUFFERUIVEC3,   \
                                       NGL_NODE_STREAMEDBUFFERUIVEC4,   \
                                       NGL_NODE_STREAMEDBUFFERFLOAT,    \
                                       NGL_NODE_STREAMEDBUFFERVEC2,     \
                                       NGL_NODE_STREAMEDBUFFERVEC3,     \
                                       NGL_NODE_STREAMEDBUFFERVEC4,     \
                                       NGL_NODE_BUFFERFLOAT,            \
                                       NGL_NODE_BUFFERVEC2,             \
                                       NGL_NODE_BUFFERVEC3,             \
                                       NGL_NODE_BUFFERVEC4,             \
                                       NGL_NODE_BUFFERINT,              \
                                       NGL_NODE_BUFFERIVEC2,            \
                                       NGL_NODE_BUFFERIVEC3,            \
                                       NGL_NODE_BUFFERIVEC4,            \
                                       NGL_NODE_BUFFERUINT,             \
                                       NGL_NODE_BUFFERUIVEC2,           \
                                       NGL_NODE_BUFFERUIVEC3,           \
                                       NGL_NODE_BUFFERUIVEC4,           \
                                       NGL_NODE_BUFFERMAT4,             \
                                       NGL_NODE_UNIFORMBOOL,            \
                                       NGL_NODE_UNIFORMFLOAT,           \
                                       NGL_NODE_UNIFORMVEC2,            \
                                       NGL_NODE_UNIFORMVEC3,            \
                                       NGL_NODE_UNIFORMVEC4,            \
                                       NGL_NODE_UNIFORMINT,             \
                                       NGL_NODE_UNIFORMIVEC2,           \
                                       NGL_NODE_UNIFORMIVEC3,           \
                                       NGL_NODE_UNIFORMIVEC4,           \
                                       NGL_NODE_UNIFORMUINT,            \
                                       NGL_NODE_UNIFORMUIVEC2,          \
                                       NGL_NODE_UNIFORMUIVEC3,          \
                                       NGL_NODE_UNIFORMUIVEC4,          \
                                       NGL_NODE_UNIFORMMAT4,            \
                                       NGL_NODE_UNIFORMQUAT,            \
                                       NGL_NODE_UNIFORMCOLOR,           \
                                       NGL_NODE_ANIMATEDFLOAT,          \
                                       NGL_NODE_ANIMATEDVEC2,           \
                                       NGL_NODE_ANIMATEDVEC3,           \
                                       NGL_NODE_ANIMATEDVEC4,           \
                                       NGL_NODE_ANIMATEDQUAT,           \
                                       NGL_NODE_ANIMATEDCOLOR,          \
                                       NGL_NODE_STREAMEDINT,            \
                                       NGL_NODE_STREAMEDIVEC2,          \
                                       NGL_NODE_STREAMEDIVEC3,          \
                                       NGL_NODE_STREAMEDIVEC4,          \
                                       NGL_NODE_STREAMEDUINT,           \
                                       NGL_NODE_STREAMEDUIVEC2,         \
                                       NGL_NODE_STREAMEDUIVEC3,         \
                                       NGL_NODE_STREAMEDUIVEC4,         \
                                       NGL_NODE_STREAMEDFLOAT,          \
                                       NGL_NODE_STREAMEDVEC2,           \
                                       NGL_NODE_STREAMEDVEC3,           \
                                       NGL_NODE_STREAMEDVEC4,           \
                                       NGL_NODE_STREAMEDMAT4,           \
                                       NGL_NODE_TIME,                   \
                                       -1}

struct block_priv {
    struct block_info blk;
    int force_update;
};

struct block_opts {
    struct ngl_node **fields;
    int nb_fields;
    int layout;
};

#define OFFSET(x) offsetof(struct block_opts, x)
static const struct node_param block_params[] = {
    {"fields", NGLI_PARAM_TYPE_NODELIST, OFFSET(fields),
               .node_types=FIELD_TYPES_LIST,
               .desc=NGLI_DOCSTRING("block fields defined in the graphic program")},
    {"layout", NGLI_PARAM_TYPE_SELECT, OFFSET(layout), {.i32=NGLI_BLOCK_LAYOUT_STD140},
               .choices=&layout_choices,
               .desc=NGLI_DOCSTRING("memory layout set in the graphic program")},
    {NULL}
};

NGLI_STATIC_ASSERT(block_info_is_first, offsetof(struct block_priv, blk) == 0);

void ngli_node_block_extend_usage(struct ngl_node *node, int usage)
{
    struct block_info *s = node->priv_data;
    s->usage |= usage;
}

int ngli_node_block_get_cpu_size(struct ngl_node *node)
{
    struct block_info *s = node->priv_data;
    return s->data_size;
}

int ngli_node_block_get_gpu_size(struct ngl_node *node)
{
    struct block_info *s = node->priv_data;
    return s->data_size;
}

static int get_node_data_type(const struct ngl_node *node)
{
    if (node->cls->category == NGLI_NODE_CATEGORY_VARIABLE) {
        const struct variable_info *variable = node->priv_data;
        return variable->data_type;
    } else if (node->cls->category == NGLI_NODE_CATEGORY_BUFFER) {
        const struct buffer_info *buffer = node->priv_data;
        return buffer->layout.type;
    } else {
        ngli_assert(0);
    }
}

static int get_node_data_count(const struct ngl_node *node)
{
    if (node->cls->category == NGLI_NODE_CATEGORY_VARIABLE) {
        return 0;
    } else if (node->cls->category == NGLI_NODE_CATEGORY_BUFFER) {
        const struct buffer_info *buffer = node->priv_data;
        return buffer->layout.count;
    } else {
        ngli_assert(0);
    }
}

static int is_dynamic_uniform(const struct ngl_node *unode)
{
    const struct variable_info *uniform = unode->priv_data;
    return uniform->dynamic;
}

static int is_dynamic_buffer(const struct ngl_node *bnode)
{
    const struct buffer_info *buffer = bnode->priv_data;
    return buffer->flags & NGLI_BUFFER_INFO_FLAG_DYNAMIC;
}

static void update_uniform_field(uint8_t *dst,
                                 const struct ngl_node *node,
                                 const struct block_field *fi)
{
    const struct variable_info *uniform = node->priv_data;
    ngli_block_field_copy(fi, dst, uniform->data);
}

static void update_buffer_field(uint8_t *dst,
                                const struct ngl_node *node,
                                const struct block_field *fi)
{
    const struct buffer_info *buffer = node->priv_data;
    ngli_block_field_copy(fi, dst, buffer->data);
}

enum field_type { IS_SINGLE, IS_ARRAY };

static const struct {
    int (*is_dynamic)(const struct ngl_node *node);
    void (*update_data)(uint8_t *dst, const struct ngl_node *node, const struct block_field *fi);
} field_funcs[] = {
    [IS_SINGLE] = {is_dynamic_uniform, update_uniform_field},
    [IS_ARRAY]  = {is_dynamic_buffer,  update_buffer_field},
};

static int field_is_dynamic(const struct ngl_node *node, const struct block_field *fi)
{
    return field_funcs[fi->count ? IS_ARRAY : IS_SINGLE].is_dynamic(node);
}

static int update_block_data(struct ngl_node *node, int forced)
{
    int has_changed = 0;
    struct block_priv *s = node->priv_data;
    const struct block_opts *o = node->opts;
    const struct block_field *field_info = ngli_darray_data(&s->blk.block.fields);
    for (int i = 0; i < o->nb_fields; i++) {
        const struct ngl_node *field_node = o->fields[i];
        const struct block_field *fi = &field_info[i];
        if (!forced && !field_is_dynamic(field_node, fi))
            continue;
        field_funcs[fi->count ? IS_ARRAY : IS_SINGLE].update_data(s->blk.data + fi->offset, field_node, fi);
        has_changed = 1; // TODO: only re-upload the changing data segments
    }
    return has_changed;
}

static int cmp_str(const void *a, const void *b)
{
    const char *s0 = *(const char * const *)a;
    const char *s1 = *(const char * const *)b;
    return strcmp(s0, s1);
}

static int check_dup_labels(const char *block_name, struct ngl_node * const *nodes, int nb_nodes)
{
    char **labels = ngli_calloc(nb_nodes, sizeof(*labels));
    if (!labels)
        return NGL_ERROR_MEMORY;
    for (int i = 0; i < nb_nodes; i++) {
        if (!nodes[i]->label) {
            LOG(ERROR, "block field labels cannot be NULL");
            ngli_free(labels);
            return NGL_ERROR_INVALID_ARG;
        }
        labels[i] = nodes[i]->label;
    }
    qsort(labels, nb_nodes, sizeof(*labels), cmp_str);
    for (int i = 1; i < nb_nodes; i++) {
        if (!strcmp(labels[i - 1], labels[i])) {
            LOG(ERROR, "duplicated label %s in block %s", labels[i], block_name);
            ngli_free(labels);
            return NGL_ERROR_INVALID_ARG;
        }
    }
    ngli_free(labels);
    return 0;
}

#define FEATURES_STD140 (NGLI_FEATURE_UNIFORM_BUFFER | NGLI_FEATURE_STORAGE_BUFFER)
#define FEATURES_STD430 (NGLI_FEATURE_STORAGE_BUFFER)

static int block_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct block_priv *s = node->priv_data;
    const struct block_opts *o = node->opts;

    if (o->layout == NGLI_BLOCK_LAYOUT_STD140 && !(gpu_ctx->features & FEATURES_STD140)) {
        LOG(ERROR, "std140 blocks are not supported by this context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (o->layout == NGLI_BLOCK_LAYOUT_STD430 && !(gpu_ctx->features & FEATURES_STD430)) {
        LOG(ERROR, "std430 blocks are not supported by this context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (!o->nb_fields) {
        LOG(ERROR, "block fields must not be empty");
        return NGL_ERROR_INVALID_ARG;
    }

    int ret = check_dup_labels(node->label, o->fields, o->nb_fields);
    if (ret < 0)
        return ret;

    ngli_block_init(&s->blk.block, o->layout);

    s->blk.usage = NGLI_BUFFER_USAGE_TRANSFER_DST_BIT;

    for (int i = 0; i < o->nb_fields; i++) {
        const struct ngl_node *field_node = o->fields[i];

        if (field_node->cls->category == NGLI_NODE_CATEGORY_BUFFER) {
            const struct buffer_info *info = field_node->priv_data;
            if (info->block) {
                LOG(ERROR, "buffers used as a block field referencing a block are not supported");
                return NGL_ERROR_UNSUPPORTED;
            }
        }

        const int type  = get_node_data_type(field_node);
        const int count = get_node_data_count(field_node);

        int ret = ngli_block_add_field(&s->blk.block, field_node->label, type, count);
        if (ret < 0)
            return ret;

        const struct block_field *fields = ngli_darray_data(&s->blk.block.fields);
        const struct block_field *fi = &fields[i];
        LOG(DEBUG, "%s.field[%d]: %s offset=%d size=%d stride=%d",
            node->label, i, field_node->label, fi->offset, fi->size, fi->stride);

        if (field_is_dynamic(field_node, fi))
            s->blk.usage |= NGLI_BUFFER_USAGE_DYNAMIC_BIT;
    }

    s->blk.data_size = s->blk.block.size;
    LOG(DEBUG, "total %s size: %d", node->label, s->blk.data_size);
    s->blk.data = ngli_calloc(1, s->blk.data_size);
    if (!s->blk.data)
        return NGL_ERROR_MEMORY;

    update_block_data(node, 1);
    s->force_update = 1; /* First update will need an upload */

    s->blk.buffer = ngli_buffer_create(gpu_ctx);
    if (!s->blk.buffer)
        return NGL_ERROR_MEMORY;

    return 0;
}

static int block_prepare(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    ngli_assert(s->blk.buffer);

    if (s->blk.buffer->size)
        return 0;

    int ret = ngli_buffer_init(s->blk.buffer, s->blk.data_size, s->blk.usage);
    if (ret < 0)
        return ret;

    return ngli_node_prepare_children(node);
}

static int block_invalidate(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    s->force_update = 1;

    return 0;
}

static int block_update(struct ngl_node *node, double t)
{
    struct block_priv *s = node->priv_data;

    int ret = ngli_node_update_children(node, t);
    if (ret < 0)
        return ret;

    const int has_changed = update_block_data(node, s->force_update);
    s->force_update = 0;

    if (has_changed) {
        ret = ngli_buffer_upload(s->blk.buffer, s->blk.data, s->blk.data_size, 0);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void block_uninit(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    ngli_buffer_freep(&s->blk.buffer);
    ngli_block_reset(&s->blk.block);
    ngli_free(s->blk.data);
}

const struct node_class ngli_block_class = {
    .id        = NGL_NODE_BLOCK,
    .category  = NGLI_NODE_CATEGORY_BLOCK,
    .name      = "Block",
    .init      = block_init,
    .prepare   = block_prepare,
    .invalidate = block_invalidate,
    .update    = block_update,
    .uninit    = block_uninit,
    .opts_size = sizeof(struct block_opts),
    .priv_size = sizeof(struct block_priv),
    .params    = block_params,
    .file      = __FILE__,
};
