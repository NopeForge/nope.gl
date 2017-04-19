/*
 * Copyright 2016 GoPro Inc.
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

#ifndef NODEGL_H
#define NODEGL_H

#define NODEGL_VERSION_MAJOR 0
#define NODEGL_VERSION_MINOR 0
#define NODEGL_VERSION_MICRO 0

#define NODEGL_GET_VERSION(major, minor, micro) ((major)<<16 | (minor)<<8 | (micro))

#define NODEGL_VERSION_INT NODEGL_GET_VERSION(NODEGL_VERSION_MAJOR, \
                                              NODEGL_VERSION_MINOR, \
                                              NODEGL_VERSION_MICRO)

#include <stdarg.h>
#include <stdint.h>

/*
 * Logging
 */
enum {
    NGL_LOG_VERBOSE,
    NGL_LOG_DEBUG,
    NGL_LOG_INFO,
    NGL_LOG_WARNING,
    NGL_LOG_ERROR,
};

typedef void (*ngl_log_callback_type)(void *arg, int level, const char *filename,
                                      int ln, const char *fn, const char *fmt, va_list vl);

void ngl_log_set_callback(void *arg, ngl_log_callback_type callback);
void ngl_log_set_min_level(int level);

/* Nodes */
struct ngl_node;

enum {
    NGL_NODE_ATTRIBUTEVEC2,
    NGL_NODE_ATTRIBUTEVEC3,
    NGL_NODE_ATTRIBUTEVEC4,
    NGL_NODE_CAMERA,
    NGL_NODE_TEXTURE,
    NGL_NODE_MEDIA,
    NGL_NODE_GLSTATE,
    NGL_NODE_GLBLENDSTATE,
    NGL_NODE_GLCOLORSTATE,
    NGL_NODE_GLSTENCILSTATE,
    NGL_NODE_GROUP,
    NGL_NODE_TEXTUREDSHAPE,
    NGL_NODE_QUAD,
    NGL_NODE_TRIANGLE,
    NGL_NODE_SHAPEPRIMITIVE,
    NGL_NODE_SHAPE,
    NGL_NODE_SHADER,
    NGL_NODE_RENDERRANGECONTINUOUS,
    NGL_NODE_RENDERRANGENORENDER,
    NGL_NODE_RENDERRANGEONCE,
    NGL_NODE_ROTATE,
    NGL_NODE_RTT,
    NGL_NODE_TRANSLATE,
    NGL_NODE_SCALE,
    NGL_NODE_ANIMKEYFRAMESCALAR,
    NGL_NODE_ANIMKEYFRAMEVEC2,
    NGL_NODE_ANIMKEYFRAMEVEC3,
    NGL_NODE_ANIMKEYFRAMEVEC4,
    NGL_NODE_UNIFORMSCALAR,
    NGL_NODE_UNIFORMVEC2,
    NGL_NODE_UNIFORMVEC3,
    NGL_NODE_UNIFORMVEC4,
    NGL_NODE_UNIFORMMAT4,
    NGL_NODE_UNIFORMSAMPLER,
    NGL_NODE_UNIFORMINT,
    NGL_NODE_FPS,
    NGL_NODE_IDENTITY,
};

struct ngl_node *ngl_node_create(int type, ...);
void ngl_node_ref(struct ngl_node *node);
void ngl_node_unrefp(struct ngl_node **nodep);

int ngl_node_param_add(struct ngl_node *node, const char *key,
                       int nb_elems, void *elems);
int ngl_node_param_set(struct ngl_node *node, const char *key, ...);
char *ngl_node_dot(const struct ngl_node *node);
char *ngl_node_serialize(const struct ngl_node *node);
struct ngl_node *ngl_node_deserialize(const char *s);

/* GL context */
enum {
    NGL_GLPLATFORM_AUTO,
    NGL_GLPLATFORM_GLX,
    NGL_GLPLATFORM_EGL,
    NGL_GLPLATFORM_CGL,
    NGL_GLPLATFORM_EAGL,
    NGL_GLPLATFORM_WGL,
};

/* GL API version */
enum {
    NGL_GLAPI_AUTO,
    NGL_GLAPI_OPENGL3,
    NGL_GLAPI_OPENGLES2,
};

/* Uniform sampler type */
enum {
    NGL_UNIFORM_SAMPLER_2D,
    NGL_UNIFORM_SAMPLER_EXTERNAL_OES,
};

/* Main context */
struct ngl_ctx;

struct ngl_ctx *ngl_create(void);
int ngl_set_glcontext(struct ngl_ctx *s, void *display, void *window, void *handle, int platform, int api);
int ngl_set_glstates(struct ngl_ctx *s, int nb_glstates, struct ngl_node **glstates);
int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene);
int ngl_draw(struct ngl_ctx *s, double t);
void ngl_free(struct ngl_ctx **ss);

#endif
