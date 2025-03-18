/*
 * Copyright 2024-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "log.h"

#include "bindgroup_gl.h"
#include "buffer_gl.h"
#include "cmd_buffer_gl.h"
#include "ctx_gl.h"
#include "fence_gl.h"
#include "pipeline_gl.h"
#include "rendertarget_gl.h"

#include "ngpu/ctx.h"
#include "utils/memory.h"
#include "utils/refcount.h"

struct ngpu_cmd_buffer_gl {
    struct ngli_rc rc;
    struct ngpu_ctx *gpu_ctx;
    int type;
    struct ngpu_fence_gl *fence;
    struct darray cmds; // array of cmd_gl
    struct darray refs; // array of ngli_rc pointers
};

static void cmd_buffer_gl_freep(void **sp)
{
    struct ngpu_cmd_buffer_gl *s = *sp;
    if (!s)
        return;

    ngpu_cmd_buffer_gl_wait(s);

    ngli_darray_reset(&s->refs);
    ngli_darray_reset(&s->cmds);

    ngli_freep(sp);
}

struct ngpu_cmd_buffer_gl *ngpu_cmd_buffer_gl_create(struct ngpu_ctx *gpu_ctx)
{
    struct ngpu_cmd_buffer_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->rc = NGLI_RC_CREATE(cmd_buffer_gl_freep);
    s->gpu_ctx = gpu_ctx;
    return s;
}

static void unref_rc(void *user_arg, void *data)
{
    struct ngli_rc **rcp = data;
    NGLI_RC_UNREFP(rcp);
}

void ngpu_cmd_buffer_gl_freep(struct ngpu_cmd_buffer_gl **sp)
{
    NGLI_RC_UNREFP(sp);
}

int ngpu_cmd_buffer_gl_init(struct ngpu_cmd_buffer_gl *s, int type)
{
    s->type = type;

    ngli_darray_init(&s->cmds, sizeof(struct ngpu_cmd_gl), 0);
    ngli_darray_init(&s->refs, sizeof(struct ngli_rc *), 0);
    ngli_darray_set_free_func(&s->refs, unref_rc, NULL);

    return 0;
}

int ngpu_cmd_buffer_gl_ref(struct ngpu_cmd_buffer_gl *s, struct ngli_rc *rc)
{
    if (!ngli_darray_push(&s->refs, &rc))
        return NGL_ERROR_MEMORY;

    NGLI_RC_REF(rc);

    return 0;
}

int ngpu_cmd_buffer_gl_begin(struct ngpu_cmd_buffer_gl *s)
{
    ngli_darray_clear(&s->refs);
    ngli_darray_clear(&s->cmds);

    return 0;
}

int ngpu_cmd_buffer_gl_push(struct ngpu_cmd_buffer_gl *s, const struct ngpu_cmd_gl *cmd)
{
    if (!ngli_darray_push(&s->cmds, cmd))
        return NGL_ERROR_MEMORY;

    return 0;
}

int ngpu_cmd_buffer_gl_submit(struct ngpu_cmd_buffer_gl *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    struct ngpu_rendertarget *cur_rendertarget = NULL;
    struct ngpu_pipeline *cur_pipeline = NULL;

    struct ngpu_cmd_gl *cmds = ngli_darray_data(&s->cmds);
    for (size_t i = 0; i < ngli_darray_count(&s->cmds); i++) {
        struct ngpu_cmd_gl *cmd = &cmds[i];

        switch (cmd->type) {
        case NGPU_CMD_TYPE_GL_SET_INDEX_BUFFER:
            gpu_ctx->index_buffer = cmd->set_index_buffer.buffer;
            gpu_ctx->index_format = cmd->set_index_buffer.format;
            break;
        case NGPU_CMD_TYPE_GL_SET_VERTEX_BUFFER: {
            size_t index = cmd->set_vertex_buffer.index;
            gpu_ctx->vertex_buffers[index] = cmd->set_vertex_buffer.buffer;
            break;
        }
        case NGPU_CMD_TYPE_GL_SET_VIEWPORT: {
            ngli_glstate_update_viewport(gl, &gpu_ctx_gl->glstate, &cmd->set_viewport.viewport);
            break;
        }
        case NGPU_CMD_TYPE_GL_SET_SCISSOR: {
            ngli_glstate_update_scissor(gl, &gpu_ctx_gl->glstate, &cmd->set_scissor.scissor);
            break;
        }
        case NGPU_CMD_TYPE_GL_BEGIN_RENDER_PASS: {
            cur_rendertarget = cmd->begin_render_pass.rendertarget;
            ngpu_rendertarget_gl_begin_pass(cmd->begin_render_pass.rendertarget);
            break;
        }
        case NGPU_CMD_TYPE_GL_END_RENDER_PASS: {
            ngli_assert(cur_rendertarget != NULL);
            ngpu_rendertarget_gl_end_pass(cur_rendertarget);
            cur_rendertarget = NULL;
            break;
        }
        case NGPU_CMD_TYPE_GL_GENERATE_TEXTURE_MIPMAP: {
            ngpu_texture_generate_mipmap(cmd->generate_texture_mipmap.texture);
            break;
        }
        case NGPU_CMD_TYPE_GL_SET_PIPELINE: {
            cur_pipeline = cmd->set_pipeline.pipeline;
            break;
        }
        case NGPU_CMD_TYPE_GL_SET_BINDGROUP: {
            gpu_ctx->bindgroup = cmd->set_bindgroup.bindgroup;
            ngpu_bindgroup_gl_bind(cmd->set_bindgroup.bindgroup,
                                   cmd->set_bindgroup.offsets,
                                   cmd->set_bindgroup.nb_offsets);
            break;
        }
        case NGPU_CMD_TYPE_GL_DRAW: {
            ngli_assert(cur_pipeline != NULL);
            ngpu_pipeline_gl_draw(cur_pipeline,
                                  cmd->draw.nb_vertices,
                                  cmd->draw.nb_instances,
                                  cmd->draw.first_vertex);
            break;
        }
        case NGPU_CMD_TYPE_GL_DRAW_INDEXED: {
            ngli_assert(cur_pipeline != NULL);
            ngpu_pipeline_gl_draw_indexed(cur_pipeline,
                                          cmd->draw_indexed.nb_indices,
                                          cmd->draw_indexed.nb_instances);
            break;
        }
        case NGPU_CMD_TYPE_GL_DISPATCH: {
            ngli_assert(cur_pipeline != NULL);
            ngpu_pipeline_gl_dispatch(cur_pipeline,
                                      cmd->dispatch.nb_group_x,
                                      cmd->dispatch.nb_group_y,
                                      cmd->dispatch.nb_group_z);
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    s->fence = ngpu_fence_gl_create(gpu_ctx);
    if (!s->fence)
        return NGL_ERROR_GRAPHICS_GENERIC;

    ngli_darray_clear(&s->cmds);

    return 0;
}

int ngpu_cmd_buffer_gl_wait(struct ngpu_cmd_buffer_gl *s)
{
    if (s->fence == NULL) {
        return 0;
    }

    int ret = ngpu_fence_gl_wait(s->fence);

    ngpu_fence_gl_freep(&s->fence);
    ngli_darray_clear(&s->refs);

    return ret;
}

