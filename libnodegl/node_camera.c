/*
 * Copyright 2016-2017 GoPro Inc.
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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "gpu_ctx.h"
#include "log.h"
#include "nodegl.h"
#include "internal.h"
#include "math_utils.h"
#include "transforms.h"

struct camera_opts {
    struct ngl_node *child;
    float eye[3];
    float center[3];
    float up[3];
    struct ngl_node *perspective_node;
    float perspective[2];
    float orthographic[4];
    float clipping[2];
    struct ngl_node *eye_transform;
    struct ngl_node *center_transform;
    struct ngl_node *up_transform;
};

struct camera_priv {
    struct camera_opts opts;

    int use_perspective;
    int use_orthographic;

    NGLI_ALIGNED_MAT(modelview_matrix);
    NGLI_ALIGNED_MAT(projection_matrix);
};

static int apply_transform(float *v, struct ngl_node *transform, double t)
{
    if (!transform)
        return 0;

    int ret = ngli_node_update(transform, t);
    if (ret < 0)
        return ret;
    NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    ngli_transform_chain_compute(transform, matrix);
    ngli_mat4_mul_vec4(v, matrix, v);

    return 0;
}

static int update_matrices(struct ngl_node *node, double t)
{
    struct ngl_ctx *ctx = node->ctx;
    struct camera_priv *s = node->priv_data;
    const struct camera_opts *o = &s->opts;

    NGLI_ALIGNED_VEC(eye)    = {NGLI_ARG_VEC3(o->eye),    1.0f};
    NGLI_ALIGNED_VEC(center) = {NGLI_ARG_VEC3(o->center), 1.0f};
    NGLI_ALIGNED_VEC(up)     = {NGLI_ARG_VEC3(o->up),     1.0f};

    int ret;
    if ((ret = apply_transform(eye, o->eye_transform, t)) < 0 ||
        (ret = apply_transform(center, o->center_transform, t)) < 0 ||
        (ret = apply_transform(up, o->up_transform, t)) < 0)
        return ret;

    ngli_mat4_look_at(s->modelview_matrix, eye, center, up);

    const float *perspective;
    if (o->perspective_node) {
        struct ngl_node *anim_node = o->perspective_node;
        struct variable_priv *anim = anim_node->priv_data;
        int ret = ngli_node_update(anim_node, t);
        if (ret < 0)
            return ret;
        perspective = anim->data;
    } else {
        perspective = o->perspective;
    }

    if (s->use_perspective) {
        ngli_mat4_perspective(s->projection_matrix,
                              perspective[0],
                              perspective[1],
                              o->clipping[0],
                              o->clipping[1]);
    } else if (s->use_orthographic) {
        ngli_mat4_orthographic(s->projection_matrix,
                               o->orthographic[0],
                               o->orthographic[1],
                               o->orthographic[2],
                               o->orthographic[3],
                               o->clipping[0],
                               o->clipping[1]);
    } else {
        ngli_mat4_identity(s->projection_matrix);
    }

    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    ngli_gpu_ctx_transform_projection_matrix(gpu_ctx, s->projection_matrix);

    return 0;
}

static int update_params(struct ngl_node *node)
{
    struct camera_priv *s = node->priv_data;
    struct camera_opts *o = &s->opts;

    ngli_vec3_norm(o->up, o->up);

    static const float zvec[4];
    s->use_perspective = memcmp(o->perspective, zvec, sizeof(o->perspective)) || o->perspective_node;
    s->use_orthographic = memcmp(o->orthographic, zvec, sizeof(o->orthographic));

    return 0;
}

#define OFFSET(x) offsetof(struct camera_priv, opts.x)
static const struct node_param camera_params[] = {
    {"child", NGLI_PARAM_TYPE_NODE, OFFSET(child), .flags=NGLI_PARAM_FLAG_NON_NULL,
              .desc=NGLI_DOCSTRING("scene to observe through the lens of the camera")},
    {"eye", NGLI_PARAM_TYPE_VEC3,  OFFSET(eye), {.vec={0.0f, 0.0f, 0.0f}},
            .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
            .update_func=update_params,
            .desc=NGLI_DOCSTRING("eye position")},
    {"center", NGLI_PARAM_TYPE_VEC3,  OFFSET(center), {.vec={0.0f, 0.0f, -1.0f}},
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=update_params,
               .desc=NGLI_DOCSTRING("center position")},
    {"up", NGLI_PARAM_TYPE_VEC3,  OFFSET(up), {.vec={0.0f, 1.0f, 0.0f}},
           .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
           .update_func=update_params,
           .desc=NGLI_DOCSTRING("up vector, must not be parallel to the line of sight from the eye point to the center point")},
    {"perspective", NGLI_PARAM_TYPE_VEC2,  OFFSET(perspective_node),
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                    .update_func=update_params,
                    .desc=NGLI_DOCSTRING("the 2 following values: *fov*, *aspect*")},
    {"orthographic", NGLI_PARAM_TYPE_VEC4, OFFSET(orthographic),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .update_func=update_params,
                     .desc=NGLI_DOCSTRING("the 4 following values: *left*, *right*, *bottom*, *top*")},
    {"clipping", NGLI_PARAM_TYPE_VEC2, OFFSET(clipping),
                 .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .update_func=update_params,
                 .desc=NGLI_DOCSTRING("the 2 following values: *near clipping plane*, *far clipping plane*")},
    {"eye_transform", NGLI_PARAM_TYPE_NODE, OFFSET(eye_transform),
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .node_types=TRANSFORM_TYPES_LIST,
                     .desc=NGLI_DOCSTRING("`eye` transformation chain")},
    {"center_transform", NGLI_PARAM_TYPE_NODE, OFFSET(center_transform),
                         .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                         .node_types=TRANSFORM_TYPES_LIST,
                         .desc=NGLI_DOCSTRING("`center` transformation chain")},
    {"up_transform", NGLI_PARAM_TYPE_NODE, OFFSET(up_transform),
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .node_types=TRANSFORM_TYPES_LIST,
                     .desc=NGLI_DOCSTRING("`up` transformation chain")},
    {NULL}
};

static int camera_init(struct ngl_node *node)
{
    struct camera_priv *s = node->priv_data;
    struct camera_opts *o = &s->opts;

    ngli_vec3_norm(o->up, o->up);

    float ground[3];
    ngli_vec3_sub(ground, o->eye, o->center);
    ngli_vec3_norm(ground, ground);
    ngli_vec3_cross(ground, ground, o->up);

    if (!ground[0] && !ground[1] && !ground[2]) {
        LOG(ERROR, "view and up are collinear");
        return NGL_ERROR_INVALID_ARG;
    }

    static const float zvec[4];
    s->use_perspective = memcmp(o->perspective, zvec, sizeof(o->perspective)) || o->perspective_node;
    s->use_orthographic = memcmp(o->orthographic, zvec, sizeof(o->orthographic));

    if ((s->use_perspective || s->use_orthographic) &&
        !memcmp(o->clipping, zvec, sizeof(o->clipping))) {
        LOG(ERROR, "clipping must be set when perspective or orthographic is used");
        return NGL_ERROR_INVALID_ARG;
    }

    int ret;
    if ((ret = ngli_transform_chain_check(o->eye_transform)) < 0 ||
        (ret = ngli_transform_chain_check(o->center_transform)) < 0 ||
        (ret = ngli_transform_chain_check(o->up_transform)) < 0)
        return ret;

    return 0;
}

static int camera_update(struct ngl_node *node, double t)
{
    struct camera_priv *s = node->priv_data;
    struct camera_opts *o = &s->opts;
    struct ngl_node *child = o->child;

    int ret = update_matrices(node, t);
    if (ret < 0)
        return ret;

    return ngli_node_update(child, t);
}

static void camera_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct camera_priv *s = node->priv_data;
    struct camera_opts *o = &s->opts;

    if (!ngli_darray_push(&ctx->modelview_matrix_stack, s->modelview_matrix) ||
        !ngli_darray_push(&ctx->projection_matrix_stack, s->projection_matrix))
        return;

    ngli_node_draw(o->child);

    ngli_darray_pop(&ctx->modelview_matrix_stack);
    ngli_darray_pop(&ctx->projection_matrix_stack);
}

const struct node_class ngli_camera_class = {
    .id        = NGL_NODE_CAMERA,
    .name      = "Camera",
    .init      = camera_init,
    .update    = camera_update,
    .draw      = camera_draw,
    .priv_size = sizeof(struct camera_priv),
    .params    = camera_params,
    .file      = __FILE__,
};
