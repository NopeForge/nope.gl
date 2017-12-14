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

/**
 * Logging levels
 */
enum {
    NGL_LOG_VERBOSE,
    NGL_LOG_DEBUG,
    NGL_LOG_INFO,
    NGL_LOG_WARNING,
    NGL_LOG_ERROR,
};

/**
 * Logging callback prototype.
 *
 * @param arg       forwarded opaque user argument
 * @param level     log level (any of NGL_LOG_*)
 * @param filename  source filename from where the message originates
 * @param ln        line in the source filename
 * @param fn        calling function name
 * @param fmt       printf format string
 * @param vl        variable argument list object associated with the fmt format string
 */
typedef void (*ngl_log_callback_type)(void *arg, int level, const char *filename,
                                      int ln, const char *fn, const char *fmt, va_list vl);

/**
 * Set a global custom logging callback.
 *
 * @param arg       opaque user argument to be forwarded to the callback
 *                  (typically a user context)
 * @param callback  callback function to be called when logging a message
 */
void ngl_log_set_callback(void *arg, ngl_log_callback_type callback);

/**
 * Set the minimum global logging level.
 *
 * No message with its level inferior to the specified level will be logged
 * (with or without the callback set).
 *
 * @param level log level (any of NGL_LOG_*)
 */
void ngl_log_set_min_level(int level);

/**
 * Opaque structure identifying a node
 */
struct ngl_node;

#define NGLI_FOURCC(a,b,c,d) (((uint32_t)(a))<<24 | (b)<<16 | (c)<<8 | (d))

/**
 * Node FOURCC identifiers
 */
#define NGL_NODE_ANIMATEDBUFFERFLOAT    NGLI_FOURCC('A','B','f','1')
#define NGL_NODE_ANIMATEDBUFFERVEC2     NGLI_FOURCC('A','B','f','2')
#define NGL_NODE_ANIMATEDBUFFERVEC3     NGLI_FOURCC('A','B','f','3')
#define NGL_NODE_ANIMATEDBUFFERVEC4     NGLI_FOURCC('A','B','f','4')
#define NGL_NODE_ANIMATEDFLOAT          NGLI_FOURCC('A','n','m','1')
#define NGL_NODE_ANIMATEDVEC2           NGLI_FOURCC('A','n','m','2')
#define NGL_NODE_ANIMATEDVEC3           NGLI_FOURCC('A','n','m','3')
#define NGL_NODE_ANIMATEDVEC4           NGLI_FOURCC('A','n','m','4')
#define NGL_NODE_ANIMKEYFRAMEBUFFER     NGLI_FOURCC('A','K','F','B')
#define NGL_NODE_ANIMKEYFRAMEFLOAT      NGLI_FOURCC('A','K','F','1')
#define NGL_NODE_ANIMKEYFRAMEVEC2       NGLI_FOURCC('A','K','F','2')
#define NGL_NODE_ANIMKEYFRAMEVEC3       NGLI_FOURCC('A','K','F','3')
#define NGL_NODE_ANIMKEYFRAMEVEC4       NGLI_FOURCC('A','K','F','4')
#define NGL_NODE_BUFFERBYTE             NGLI_FOURCC('B','s','b','1')
#define NGL_NODE_BUFFERBVEC2            NGLI_FOURCC('B','s','b','2')
#define NGL_NODE_BUFFERBVEC3            NGLI_FOURCC('B','s','b','3')
#define NGL_NODE_BUFFERBVEC4            NGLI_FOURCC('B','s','b','4')
#define NGL_NODE_BUFFERINT              NGLI_FOURCC('B','s','i','1')
#define NGL_NODE_BUFFERIVEC2            NGLI_FOURCC('B','s','i','2')
#define NGL_NODE_BUFFERIVEC3            NGLI_FOURCC('B','s','i','3')
#define NGL_NODE_BUFFERIVEC4            NGLI_FOURCC('B','s','i','4')
#define NGL_NODE_BUFFERSHORT            NGLI_FOURCC('B','s','s','1')
#define NGL_NODE_BUFFERSVEC2            NGLI_FOURCC('B','s','s','2')
#define NGL_NODE_BUFFERSVEC3            NGLI_FOURCC('B','s','s','3')
#define NGL_NODE_BUFFERSVEC4            NGLI_FOURCC('B','s','s','4')
#define NGL_NODE_BUFFERUBYTE            NGLI_FOURCC('B','u','b','1')
#define NGL_NODE_BUFFERUBVEC2           NGLI_FOURCC('B','u','b','2')
#define NGL_NODE_BUFFERUBVEC3           NGLI_FOURCC('B','u','b','3')
#define NGL_NODE_BUFFERUBVEC4           NGLI_FOURCC('B','u','b','4')
#define NGL_NODE_BUFFERUINT             NGLI_FOURCC('B','u','i','1')
#define NGL_NODE_BUFFERUIVEC2           NGLI_FOURCC('B','u','i','2')
#define NGL_NODE_BUFFERUIVEC3           NGLI_FOURCC('B','u','i','3')
#define NGL_NODE_BUFFERUIVEC4           NGLI_FOURCC('B','u','i','4')
#define NGL_NODE_BUFFERUSHORT           NGLI_FOURCC('B','u','s','1')
#define NGL_NODE_BUFFERUSVEC2           NGLI_FOURCC('B','u','s','2')
#define NGL_NODE_BUFFERUSVEC3           NGLI_FOURCC('B','u','s','3')
#define NGL_NODE_BUFFERUSVEC4           NGLI_FOURCC('B','u','s','4')
#define NGL_NODE_BUFFERFLOAT            NGLI_FOURCC('B','f','v','1')
#define NGL_NODE_BUFFERVEC2             NGLI_FOURCC('B','f','v','2')
#define NGL_NODE_BUFFERVEC3             NGLI_FOURCC('B','f','v','3')
#define NGL_NODE_BUFFERVEC4             NGLI_FOURCC('B','f','v','4')
#define NGL_NODE_CAMERA                 NGLI_FOURCC('C','m','r','a')
#define NGL_NODE_CIRCLE                 NGLI_FOURCC('C','r','c','l')
#define NGL_NODE_COMPUTE                NGLI_FOURCC('C','p','t',' ')
#define NGL_NODE_COMPUTEPROGRAM         NGLI_FOURCC('C','p','t','P')
#define NGL_NODE_FPS                    NGLI_FOURCC('F','P','S',' ')
#define NGL_NODE_GEOMETRY               NGLI_FOURCC('G','e','o','m')
#define NGL_NODE_GRAPHICCONFIG          NGLI_FOURCC('G','r','C','f')
#define NGL_NODE_CONFIGBLEND            NGLI_FOURCC('C','g','S','c')
#define NGL_NODE_CONFIGCOLORMASK        NGLI_FOURCC('C','g','C','M')
#define NGL_NODE_CONFIGPOLYGONMODE      NGLI_FOURCC('C','g','P','M')
#define NGL_NODE_CONFIGDEPTH            NGLI_FOURCC('C','g','D','t')
#define NGL_NODE_CONFIGSTENCIL          NGLI_FOURCC('C','g','S','t')
#define NGL_NODE_GROUP                  NGLI_FOURCC('G','r','p',' ')
#define NGL_NODE_IDENTITY               NGLI_FOURCC('I','d',' ',' ')
#define NGL_NODE_MEDIA                  NGLI_FOURCC('M','d','i','a')
#define NGL_NODE_PROGRAM                NGLI_FOURCC('P','r','g','m')
#define NGL_NODE_QUAD                   NGLI_FOURCC('Q','u','a','d')
#define NGL_NODE_RENDER                 NGLI_FOURCC('R','n','d','r')
#define NGL_NODE_RENDERTOTEXTURE        NGLI_FOURCC('R','T','T',' ')
#define NGL_NODE_ROTATE                 NGLI_FOURCC('T','R','o','t')
#define NGL_NODE_SCALE                  NGLI_FOURCC('T','s','c','l')
#define NGL_NODE_TEXTURE2D              NGLI_FOURCC('T','e','x','2')
#define NGL_NODE_TEXTURE3D              NGLI_FOURCC('T','e','x','3')
#define NGL_NODE_TIMERANGEFILTER        NGLI_FOURCC('T','R','F','l')
#define NGL_NODE_TIMERANGEMODECONT      NGLI_FOURCC('T','R','M','C')
#define NGL_NODE_TIMERANGEMODENOOP      NGLI_FOURCC('T','R','M','N')
#define NGL_NODE_TIMERANGEMODEONCE      NGLI_FOURCC('T','R','M','1')
#define NGL_NODE_TRANSLATE              NGLI_FOURCC('T','m','o','v')
#define NGL_NODE_TRIANGLE               NGLI_FOURCC('T','r','g','l')
#define NGL_NODE_UNIFORMINT             NGLI_FOURCC('U','n','i','1')
#define NGL_NODE_UNIFORMMAT4            NGLI_FOURCC('U','n','M','4')
#define NGL_NODE_UNIFORMFLOAT           NGLI_FOURCC('U','n','f','1')
#define NGL_NODE_UNIFORMVEC2            NGLI_FOURCC('U','n','f','2')
#define NGL_NODE_UNIFORMVEC3            NGLI_FOURCC('U','n','f','3')
#define NGL_NODE_UNIFORMVEC4            NGLI_FOURCC('U','n','f','4')

/**
 * Allocate a node.
 *
 * This function does not perform any OpenGL operation.
 *
 * The reference counter of the allocated node is set to 1.
 *
 * Must be destroyed using ngl_node_unrefp().
 *
 * @param type  identify the node (any of NGL_NODE_*)
 * @param ...   variable arguments specific to the node type, refer to the
 *              constructors in the reference documentation for the expected
 *              parameters
 *
 * @return a new allocated node or NULL on error
 */
struct ngl_node *ngl_node_create(int type, ...);

/**
 * Increment the reference counter of a given node by 1.
 *
 * This function is NOT thread-safe.
 *
 * This function does not perform any OpenGL operation.
 *
 * @param node  pointer to the node to reference count
 *
 * @return node with its reference counter incremented.
 */
struct ngl_node *ngl_node_ref(struct ngl_node *node);

/**
 * Decrement the reference counter of a given node by 1, and destroy its
 * content if the reference counter reaches 0. The passed node pointer will
 * also be set to NULL.
 *
 * @warning Make sure to call this function from the OpenGL context for its
 *          destruction to prevent memory leaks (or worse).
 *
 * @param nodep  pointer to the pointer to the target node
 */
void ngl_node_unrefp(struct ngl_node **nodep);

/**
 * Add entries to a list-based parameter of an allocated node.
 *
 * If the type of the parameter is node based, the reference counter of the
 * passed nodes will be incremented.
 *
 * @param node      pointer to the target node
 * @param key       string identifying the parameter
 * @param nb_elems  number of elements to append
 * @param elems     pointer to an array of values in parameter type
 *
 * @return 0 on success, < 0 on error
 */
int ngl_node_param_add(struct ngl_node *node, const char *key,
                       int nb_elems, void *elems);

/**
 * Set a parameter value of an allocated node.
 *
 * If the type of the parameter is node based, the reference counter of the
 * passed node will be incremented.
 *
 * @param node      pointer to the target node
 * @param key       string identifying the parameter
 * @param ...       the value in parameter type
 *
 * @return 0 on success, < 0 on error
 */
int ngl_node_param_set(struct ngl_node *node, const char *key, ...);

/**
 * Serialize in Graphviz format (.dot) a node graph.
 *
 * Must be destroyed using free().
 *
 * @return an allocated string in dot format or NULL on error
 */
char *ngl_node_dot(const struct ngl_node *node);

/**
 * Serialize in node.gl format (.ngl).
 *
 * Must be destroyed using free().
 *
 * @return an allocated string in node.gl format or NULL on error
 */
char *ngl_node_serialize(const struct ngl_node *node);

/**
 * De-serialize a scene.
 *
 * @param s  string in node.gl serialized format.
 *
 * Must be destroyed using ngl_node_unrefp().
 *
 * @return a pointer to the de-serialized node graph or NULL on error
 */
struct ngl_node *ngl_node_deserialize(const char *s);

/**
 * OpenGL platforms identifiers
 */
enum {
    NGL_GLPLATFORM_AUTO,
    NGL_GLPLATFORM_GLX,
    NGL_GLPLATFORM_EGL,
    NGL_GLPLATFORM_CGL,
    NGL_GLPLATFORM_EAGL,
    NGL_GLPLATFORM_WGL,
};

/**
 * OpenGL API versions
 */
enum {
    NGL_GLAPI_AUTO,
    NGL_GLAPI_OPENGL3,
    NGL_GLAPI_OPENGLES2,
};

/**
 * Opaque structure identifying a node.gl context
 */
struct ngl_ctx;

/**
 * Allocate a new node.gl context.
 *
 * Must be destroyed using ngl_free().
 *
 * This function does not perform any OpenGL operation.
 *
 * @return a pointer to the context, or NULL on error
 */
struct ngl_ctx *ngl_create(void);

/**
 * Associate a native OpenGL context with a node.gl context and load the
 * required OpenGL functions and extensions.
 *
 * The supplied OpenGL context must be bound before calling this function.
 *
 * The OpenGL context is currently used to load dynamically OpenGL functions
 * and extensions as well as some native helpers depending on the system.
 *
 * This function must be called before any ngl_draw() call.
 *
 * @param s        pointer to a node.gl context
 * @param display  pointer to a native display handle or NULL to
 *                 automatically get the current display
 * @param window   pointer to a native window handle or NULL to
 *                 automatically get the current window
 * @param handle   pointer to a native OpenGL context handle or NULL to
 *                 automatically get the currently bound OpenGL context
 * @param platform OpenGL platform (any of NGL_GLPLATFORM_*). It must be
 *                 compatible with the system on which the code is executed.
 *                 NGL_GLPLATFORM_AUTO can be used to auto-detect it
 * @param api      OpenGL API level (any of NGL_GLAPI_*) which defines the
 *                 minimum OpenGL API to be used. NGL_GLAPI_AUTO can be used
 *                 to choose it automatically
 *
 * @return 0 on success, < 0 on error
 */
int ngl_set_glcontext(struct ngl_ctx *s, void *display, void *window, void *handle, int platform, int api);

/**
 * Associate a scene with a node.gl context.
 *
 * The reference counter of the root node will be incremented and all its node
 * will be associated with the specified node.gl context.
 *
 * The nodes can be associated with only one node.gl context.
 *
 * If any scene was previously associated with the context, it is detached from
 * it and its reference counter decremented.
 *
 * @param s      pointer to the configured node.gl context
 * @param scene  pointer to the scene
 *
 * @return 0 on success, < 0 on error
 */
int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene);

/**
 * Draw at the specified time.
 *
 * @param s     pointer to the configured node.gl context
 * @param t     target draw time in seconds
 *
 * @return 0 on success, < 0 on error
 */
int ngl_draw(struct ngl_ctx *s, double t);

/**
 * Destroy a node.gl context. The passed context pointer will also be set to
 * NULL.
 *
 * @param ss    pointer to the pointer to the node.gl context
 */
void ngl_free(struct ngl_ctx **ss);

/**
 * Evaluate an animation at a given time t.
 *
 * @param anim  the animation node can be any of AnimatedFloat, AnimatedVec2,
 *              AnimatedVec3, or AnimatedVec4
 * @param dst   pointer to the destination for the interpolated value(s), needs
 *              to hold enough space depending on the type of anim
 * @param t     the target time at which to interpolate the value(s)
 *
 * @return 0 on success, < 0 on error
 */
int ngl_anim_evaluate(struct ngl_node *anim, float *dst, double t);

/**
 * Android
 */

/**
 * Set a Java virtual machine that will be used to retrieve the JNI
 * environment.
 *
 * @param vm    pointer to the Java virtual machine
 *
 * @return 0 on success, < 0 on error
 */
int ngl_jni_set_java_vm(void *vm);

/**
 * Get the Java virtual machine pointer that has been set with
 * ngl_jni_set_java_vm().
 *
 * @return a pointer to the Java virtual machine or NULL if none has been set
 */
void *ngl_jni_get_java_vm(void);

/**
 * Set the Android application context.
 *
 * @param application_context   JNI global reference of the Android application
 *                              context
 */
int ngl_android_set_application_context(void *application_context);

/**
 * Get the Android application context that has been set with
 * ngl_android_set_application_context().
 *
 * @return a pointer to the JNI global reference of the Android application
 *         context or NULL if none has been set
 */
void *ngl_android_get_application_context(void);

#endif
