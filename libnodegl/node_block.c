/*
 * Copyright 2019 GoPro Inc.
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

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

enum {
    LAYOUT_STD140,
    LAYOUT_STD430,
    NB_LAYOUTS
};

static const struct param_choices layout_choices = {
    .name = "memory_layout",
    .consts = {
        {"std140", LAYOUT_STD140, .desc=NGLI_DOCSTRING("standard uniform block memory layout 140")},
        {"std430", LAYOUT_STD430, .desc=NGLI_DOCSTRING("standard uniform block memory layout 430")},
        {NULL}
    }
};

// TODO: NGL_NODE_UNIFORMQUAT
#define UNIFORMS_TYPES_LIST (const int[]){NGL_NODE_ANIMATEDBUFFERFLOAT, \
                                          NGL_NODE_ANIMATEDBUFFERVEC2,  \
                                          NGL_NODE_ANIMATEDBUFFERVEC3,  \
                                          NGL_NODE_ANIMATEDBUFFERVEC4,  \
                                          NGL_NODE_BUFFERFLOAT,         \
                                          NGL_NODE_BUFFERVEC2,          \
                                          NGL_NODE_BUFFERVEC3,          \
                                          NGL_NODE_BUFFERVEC4,          \
                                          NGL_NODE_BUFFERINT,           \
                                          NGL_NODE_BUFFERIVEC2,         \
                                          NGL_NODE_BUFFERIVEC3,         \
                                          NGL_NODE_BUFFERIVEC4,         \
                                          NGL_NODE_BUFFERUINT,          \
                                          NGL_NODE_BUFFERUIVEC2,        \
                                          NGL_NODE_BUFFERUIVEC3,        \
                                          NGL_NODE_BUFFERUIVEC4,        \
                                          NGL_NODE_BUFFERMAT4,          \
                                          NGL_NODE_UNIFORMFLOAT,        \
                                          NGL_NODE_UNIFORMVEC2,         \
                                          NGL_NODE_UNIFORMVEC3,         \
                                          NGL_NODE_UNIFORMVEC4,         \
                                          NGL_NODE_UNIFORMINT,          \
                                          NGL_NODE_UNIFORMMAT4,         \
                                          -1}

#define OFFSET(x) offsetof(struct block_priv, x)
static const struct node_param block_params[] = {
    {"fields", PARAM_TYPE_NODELIST, OFFSET(fields),
               .node_types=UNIFORMS_TYPES_LIST,
               .desc=NGLI_DOCSTRING("block fields defined in the graphic program")},
    {"layout", PARAM_TYPE_SELECT, OFFSET(layout), {.i64=LAYOUT_STD140},
               .choices=&layout_choices,
               .desc=NGLI_DOCSTRING("memory layout set in the graphic program")},
    {NULL}
};

int ngli_node_block_ref(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct block_priv *s = node->priv_data;

    if (s->buffer_refcount++ == 0) {
        int ret = ngli_buffer_allocate(&s->buffer, gl, s->data_size, s->usage);
        if (ret < 0)
            return ret;

        ret = ngli_buffer_upload(&s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;

        s->buffer_last_upload_time = -1.;
    }

    return 0;
}

void ngli_node_block_unref(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    ngli_assert(s->buffer_refcount);
    if (s->buffer_refcount-- == 1)
        ngli_buffer_free(&s->buffer);
}

int ngli_node_block_upload(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    if (s->has_changed && s->buffer_last_upload_time != node->last_update_time) {
        int ret = ngli_buffer_upload(&s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;
        s->buffer_last_upload_time = node->last_update_time;
        s->has_changed = 0;
    }

    return 0;
}

static int get_buffer_stride(const struct ngl_node *node, int layout)
{
    switch (node->class->id) {
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_BUFFERFLOAT:          return sizeof(GLfloat) * (layout == LAYOUT_STD140 ? 4 : 1);
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_BUFFERVEC2:           return sizeof(GLfloat) * (layout == LAYOUT_STD140 ? 4 : 2);
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_BUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
        case NGL_NODE_BUFFERVEC4:           return sizeof(GLfloat) * 4;
        case NGL_NODE_BUFFERINT:
        case NGL_NODE_BUFFERUINT:           return sizeof(GLint) * (layout == LAYOUT_STD140 ? 4 : 1);
        case NGL_NODE_BUFFERIVEC2:
        case NGL_NODE_BUFFERUIVEC2:         return sizeof(GLint) * (layout == LAYOUT_STD140 ? 4 : 2);
        case NGL_NODE_BUFFERIVEC3:
        case NGL_NODE_BUFFERUIVEC3:
        case NGL_NODE_BUFFERIVEC4:
        case NGL_NODE_BUFFERUIVEC4:         return sizeof(GLint) * 4;
        case NGL_NODE_BUFFERMAT4:           return sizeof(GLfloat) * 4 * 4;
    }
    return 0;
}

static int get_buffer_size(const struct ngl_node *bnode, int layout)
{
    const struct buffer_priv *b = bnode->priv_data;
    return b->count * get_buffer_stride(bnode, layout);
}

static int get_node_size(const struct ngl_node *node, int layout)
{
    switch (node->class->id) {
        case NGL_NODE_UNIFORMFLOAT:         return sizeof(GLfloat) * 1;
        case NGL_NODE_UNIFORMVEC2:          return sizeof(GLfloat) * 2;
        case NGL_NODE_UNIFORMVEC3:          return sizeof(GLfloat) * 3;
        case NGL_NODE_UNIFORMVEC4:          return sizeof(GLfloat) * 4;
        case NGL_NODE_UNIFORMMAT4:          return sizeof(GLfloat) * 4 * 4;
        case NGL_NODE_UNIFORMINT:           return sizeof(GLint);
        default:                            return get_buffer_size(node, layout);
    }
}

static int get_node_align(const struct ngl_node *node, int layout)
{
    switch (node->class->id) {
        case NGL_NODE_UNIFORMFLOAT:         return sizeof(GLfloat) * 1;
        case NGL_NODE_UNIFORMVEC2:          return sizeof(GLfloat) * 2;
        case NGL_NODE_UNIFORMVEC3:
        case NGL_NODE_UNIFORMVEC4:
        case NGL_NODE_UNIFORMMAT4:
        case NGL_NODE_BUFFERMAT4:           return sizeof(GLfloat) * 4;
        case NGL_NODE_UNIFORMINT:           return sizeof(GLint);
        default:                            return get_buffer_stride(node, layout);
    }
    return 0;
}

static int has_changed_uniform(const struct ngl_node *unode)
{
    const struct uniform_priv *uniform = unode->priv_data;
    return uniform->dynamic || uniform->live_changed;
}

static int has_changed_buffer(const struct ngl_node *bnode)
{
    const struct buffer_priv *buffer = bnode->priv_data;
    return buffer->dynamic;
}

static void update_uniform_float_field(uint8_t *dst,
                                       const struct ngl_node *node,
                                       const struct block_field_info *fi)
{
    const struct uniform_priv *uniform = node->priv_data;
    *(float *)dst = (float)uniform->scalar; // double -> float
}

static void update_uniform_vec_field(uint8_t *dst,
                                     const struct ngl_node *node,
                                     const struct block_field_info *fi)
{
    const struct uniform_priv *uniform = node->priv_data;
    memcpy(dst, uniform->vector, fi->size);
}

static void update_uniform_int_field(uint8_t *dst,
                                     const struct ngl_node *node,
                                     const struct block_field_info *fi)
{
    const struct uniform_priv *uniform = node->priv_data;
    memcpy(dst, &uniform->ival, fi->size);
}

static void update_uniform_mat4_field(uint8_t *dst,
                                      const struct ngl_node *node,
                                      const struct block_field_info *fi)
{
    const struct uniform_priv *uniform = node->priv_data;
    memcpy(dst, uniform->matrix, fi->size);
}

static void update_buffer_field(uint8_t *dst,
                                const struct ngl_node *node,
                                const struct block_field_info *fi)
{
    const struct buffer_priv *buffer = node->priv_data;
    if (buffer->data_stride == fi->stride)
        memcpy(dst, buffer->data, fi->size);
    else
        for (int i = 0; i < buffer->count; i++)
            memcpy(dst + i * fi->stride, buffer->data + i * buffer->data_stride, buffer->data_stride);
}

static const struct type_spec {
    int class_id;
    int (*has_changed)(const struct ngl_node *node);
    void (*update_data)(uint8_t *dst, const struct ngl_node *node, const struct block_field_info *fi);
} type_specs[] = {
    {NGL_NODE_BUFFERFLOAT,         has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERVEC2,          has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERVEC3,          has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERVEC4,          has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERINT,           has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERIVEC2,         has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERIVEC3,         has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERIVEC4,         has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERUINT,          has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERUIVEC2,        has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERUIVEC3,        has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERUIVEC4,        has_changed_buffer,  update_buffer_field},
    {NGL_NODE_BUFFERMAT4,          has_changed_buffer,  update_buffer_field},
    {NGL_NODE_ANIMATEDBUFFERFLOAT, has_changed_buffer,  update_buffer_field},
    {NGL_NODE_ANIMATEDBUFFERVEC2,  has_changed_buffer,  update_buffer_field},
    {NGL_NODE_ANIMATEDBUFFERVEC3,  has_changed_buffer,  update_buffer_field},
    {NGL_NODE_ANIMATEDBUFFERVEC4,  has_changed_buffer,  update_buffer_field},
    {NGL_NODE_UNIFORMFLOAT,        has_changed_uniform, update_uniform_float_field},
    {NGL_NODE_UNIFORMVEC2,         has_changed_uniform, update_uniform_vec_field},
    {NGL_NODE_UNIFORMVEC3,         has_changed_uniform, update_uniform_vec_field},
    {NGL_NODE_UNIFORMVEC4,         has_changed_uniform, update_uniform_vec_field},
    {NGL_NODE_UNIFORMINT,          has_changed_uniform, update_uniform_int_field},
    {NGL_NODE_UNIFORMMAT4,         has_changed_uniform, update_uniform_mat4_field},
};

static int get_spec_id(int class_id)
{
    for (int i = 0; i < NGLI_ARRAY_NB(type_specs); i++)
        if (type_specs[i].class_id == class_id)
            return i;
    return -1;
}

static void update_block_data(struct block_priv *s, int forced)
{
    for (int i = 0; i < s->nb_fields; i++) {
        const struct ngl_node *field_node = s->fields[i];
        const struct block_field_info *fi = &s->field_info[i];
        const struct type_spec *spec = &type_specs[fi->spec_id];
        if (!forced && !spec->has_changed(field_node))
            continue;
        spec->update_data(s->data + fi->offset, field_node, fi);
        s->has_changed = 1; // TODO: only re-upload the changing data segments
    }
}

#define FEATURES_STD140 (NGLI_FEATURE_UNIFORM_BUFFER_OBJECT | NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)
#define FEATURES_STD430 (NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)

static int block_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct block_priv *s = node->priv_data;

    if (s->layout == LAYOUT_STD140 && !(gl->features & FEATURES_STD140)) {
        LOG(ERROR, "std140 blocks are not supported by this context");
        return -1;
    }

    if (s->layout == LAYOUT_STD430 && !(gl->features & FEATURES_STD430)) {
        LOG(ERROR, "std430 blocks are not supported by this context");
        return -1;
    }

    s->field_info = ngli_calloc(s->nb_fields, sizeof(*s->field_info));
    if (!s->field_info)
        return -1;

    s->usage = GL_STATIC_DRAW;

    s->data_size = 0;
    for (int i = 0; i < s->nb_fields; i++) {
        const struct ngl_node *field_node = s->fields[i];
        const int spec_id = get_spec_id(field_node->class->id);
        const int size   = get_node_size(field_node, s->layout);
        const int align  = get_node_align(field_node, s->layout);

        ngli_assert(spec_id >= 0 && spec_id < NGLI_ARRAY_NB(type_specs));
        ngli_assert(align);

        const int remain = s->data_size % align;
        const int offset = s->data_size + (remain ? align - remain : 0);

        const struct type_spec *spec = &type_specs[spec_id];
        if (spec->has_changed(field_node))
            s->usage = GL_DYNAMIC_DRAW;

        struct block_field_info *fi = &s->field_info[i];
        fi->spec_id = spec_id;
        fi->size    = size;
        fi->stride  = get_buffer_stride(field_node, s->layout);
        fi->offset  = offset;

        s->data_size = offset + fi->size;
        LOG(DEBUG, "%s.field[%d]: %s offset=%d size=%d stride=%d",
            node->label, i, field_node->label, fi->offset, fi->size, fi->stride);
    }

    LOG(DEBUG, "total %s size: %d", node->label, s->data_size);
    s->data = ngli_calloc(1, s->data_size);
    if (!s->data)
        return -1;

    update_block_data(s, 1);
    return 0;
}

static int block_update(struct ngl_node *node, double t)
{
    struct block_priv *s = node->priv_data;

    for (int i = 0; i < s->nb_fields; i++) {
        struct ngl_node *field_node = s->fields[i];
        int ret = ngli_node_update(field_node, t);
        if (ret < 0)
            return ret;
    }

    update_block_data(s, 0);
    return 0;
}

static void block_uninit(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    ngli_free(s->field_info);
    ngli_free(s->data);
}

const struct node_class ngli_block_class = {
    .id        = NGL_NODE_BLOCK,
    .name      = "Block",
    .init      = block_init,
    .update    = block_update,
    .uninit    = block_uninit,
    .priv_size = sizeof(struct block_priv),
    .params    = block_params,
    .file      = __FILE__,
};
