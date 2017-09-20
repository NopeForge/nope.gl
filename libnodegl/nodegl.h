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

#define NGLI_FOURCC(a,b,c,d) (((uint32_t)(a))<<24 | (b)<<16 | (c)<<8 | (d))
#define NGL_NODE_ANIMATIONSCALAR        NGLI_FOURCC('A','n','m','1')
#define NGL_NODE_ANIMATIONVEC2          NGLI_FOURCC('A','n','m','2')
#define NGL_NODE_ANIMATIONVEC3          NGLI_FOURCC('A','n','m','3')
#define NGL_NODE_ANIMATIONVEC4          NGLI_FOURCC('A','n','m','4')
#define NGL_NODE_ANIMKEYFRAMESCALAR     NGLI_FOURCC('A','K','F','1')
#define NGL_NODE_ANIMKEYFRAMEVEC2       NGLI_FOURCC('A','K','F','2')
#define NGL_NODE_ANIMKEYFRAMEVEC3       NGLI_FOURCC('A','K','F','3')
#define NGL_NODE_ANIMKEYFRAMEVEC4       NGLI_FOURCC('A','K','F','4')
#define NGL_NODE_CAMERA                 NGLI_FOURCC('C','m','r','a')
#define NGL_NODE_FPS                    NGLI_FOURCC('F','P','S',' ')
#define NGL_NODE_GLBLENDSTATE           NGLI_FOURCC('B','l','d','S')
#define NGL_NODE_GLCOLORSTATE           NGLI_FOURCC('C','l','r','S')
#define NGL_NODE_GLSTATE                NGLI_FOURCC('G','L','x','S')
#define NGL_NODE_GLSTENCILSTATE         NGLI_FOURCC('S','t','l','S')
#define NGL_NODE_GROUP                  NGLI_FOURCC('G','r','p',' ')
#define NGL_NODE_IDENTITY               NGLI_FOURCC('I','d',' ',' ')
#define NGL_NODE_MEDIA                  NGLI_FOURCC('M','d','i','a')
#define NGL_NODE_QUAD                   NGLI_FOURCC('S','4',' ',' ')
#define NGL_NODE_RENDERRANGECONTINUOUS  NGLI_FOURCC('R','R','C','t')
#define NGL_NODE_RENDERRANGENORENDER    NGLI_FOURCC('R','R','N','R')
#define NGL_NODE_RENDERRANGEONCE        NGLI_FOURCC('R','R','1',' ')
#define NGL_NODE_ROTATE                 NGLI_FOURCC('T','R','o','t')
#define NGL_NODE_RTT                    NGLI_FOURCC('R','T','T',' ')
#define NGL_NODE_SCALE                  NGLI_FOURCC('T','s','c','l')
#define NGL_NODE_SHADER                 NGLI_FOURCC('S','h','a','d')
#define NGL_NODE_SHAPE                  NGLI_FOURCC('S','h','p','e')
#define NGL_NODE_SHAPEPRIMITIVE         NGLI_FOURCC('S','P','r','m')
#define NGL_NODE_TEXTURE                NGLI_FOURCC('T','e','x',' ')
#define NGL_NODE_TEXTUREDSHAPE          NGLI_FOURCC('T','s','h','p')
#define NGL_NODE_TRANSLATE              NGLI_FOURCC('T','m','o','v')
#define NGL_NODE_TRIANGLE               NGLI_FOURCC('S','3',' ',' ')
#define NGL_NODE_UNIFORMINT             NGLI_FOURCC('U','n','i','1')
#define NGL_NODE_UNIFORMMAT4            NGLI_FOURCC('U','n','M','4')
#define NGL_NODE_UNIFORMSCALAR          NGLI_FOURCC('U','n','f','1')
#define NGL_NODE_UNIFORMVEC2            NGLI_FOURCC('U','n','f','2')
#define NGL_NODE_UNIFORMVEC3            NGLI_FOURCC('U','n','f','3')
#define NGL_NODE_UNIFORMVEC4            NGLI_FOURCC('U','n','f','4')

struct ngl_node *ngl_node_create(int type, ...);
struct ngl_node *ngl_node_ref(struct ngl_node *node);
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

/* Main context */
struct ngl_ctx;

struct ngl_ctx *ngl_create(void);
int ngl_set_glcontext(struct ngl_ctx *s, void *display, void *window, void *handle, int platform, int api);
int ngl_set_glstates(struct ngl_ctx *s, int nb_glstates, struct ngl_node **glstates);
int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene);
int ngl_draw(struct ngl_ctx *s, double t);
void ngl_free(struct ngl_ctx **ss);

/* Android */
int ngl_jni_set_java_vm(void *vm);
void *ngl_jni_get_java_vm(void);

int ngl_android_set_application_context(void *application_context);
void *ngl_android_get_application_context(void);

/* Animation */
void ngl_anim_evaluate(struct ngl_node *anim, float *dst, double t);

#endif
