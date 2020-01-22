#
# Copyright 2016 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from libc.stdlib cimport calloc
from libc.string cimport memset
from libc.stdint cimport uint8_t
from libc.stdint cimport uintptr_t

cdef extern from "nodegl.h":
    cdef int NGL_LOG_VERBOSE
    cdef int NGL_LOG_DEBUG
    cdef int NGL_LOG_INFO
    cdef int NGL_LOG_WARNING
    cdef int NGL_LOG_ERROR

    void ngl_log_set_min_level(int level)

    cdef struct ngl_node

    ngl_node *ngl_node_create(int type, ...)
    ngl_node *ngl_node_ref(ngl_node *node)
    void ngl_node_unrefp(ngl_node **nodep)

    int ngl_node_param_add(ngl_node *node, const char *key,
                           int nb_elems, void *elems)
    int ngl_node_param_set(ngl_node *node, const char *key, ...)
    char *ngl_node_dot(const ngl_node *node)
    char *ngl_node_serialize(const ngl_node *node)
    ngl_node *ngl_node_deserialize(const char *s)

    int ngl_anim_evaluate(ngl_node *anim, void *dst, double t)

    cdef int NGL_PLATFORM_AUTO
    cdef int NGL_PLATFORM_XLIB
    cdef int NGL_PLATFORM_ANDROID
    cdef int NGL_PLATFORM_MACOS
    cdef int NGL_PLATFORM_IOS
    cdef int NGL_PLATFORM_WINDOWS

    cdef int NGL_BACKEND_AUTO
    cdef int NGL_BACKEND_OPENGL
    cdef int NGL_BACKEND_OPENGLES

    cdef struct ngl_ctx

    cdef struct ngl_config:
        int  platform
        int  backend
        uintptr_t display
        uintptr_t window
        uintptr_t handle
        int  swap_interval
        int  offscreen
        int  width
        int  height
        int  viewport[4]
        int  samples
        int  set_surface_pts
        float clear_color[4]
        uint8_t *capture_buffer

    ngl_ctx *ngl_create()
    int ngl_configure(ngl_ctx *s, ngl_config *config)
    int ngl_resize(ngl_ctx *s, int width, int height, const int *viewport);
    int ngl_set_scene(ngl_ctx *s, ngl_node *scene)
    int ngl_draw(ngl_ctx *s, double t) nogil
    char *ngl_dot(ngl_ctx *s, double t) nogil
    void ngl_freep(ngl_ctx **ss)

    int ngl_easing_evaluate(const char *name, double *args, int nb_args,
                            double *offsets, double t, double *v)
    int ngl_easing_solve(const char *name, double *args, int nb_args,
                         double *offsets, double v, double *t)

PLATFORM_AUTO    = NGL_PLATFORM_AUTO
PLATFORM_XLIB    = NGL_PLATFORM_XLIB
PLATFORM_ANDROID = NGL_PLATFORM_ANDROID
PLATFORM_MACOS   = NGL_PLATFORM_MACOS
PLATFORM_IOS     = NGL_PLATFORM_IOS
PLATFORM_WINDOWS = NGL_PLATFORM_WINDOWS

BACKEND_AUTO      = NGL_BACKEND_AUTO
BACKEND_OPENGL    = NGL_BACKEND_OPENGL
BACKEND_OPENGLES  = NGL_BACKEND_OPENGLES

LOG_VERBOSE = NGL_LOG_VERBOSE
LOG_DEBUG   = NGL_LOG_DEBUG
LOG_INFO    = NGL_LOG_INFO
LOG_WARNING = NGL_LOG_WARNING
LOG_ERROR   = NGL_LOG_ERROR

cdef _ret_pystr(char *s):
    try:
        pystr = <bytes>s
    finally:
        free(s)
    return pystr

include "nodes_def.pyx"

def log_set_min_level(int level):
    ngl_log_set_min_level(level)


cdef _eval_solve(name, src, args, offsets, evaluate):
    cdef double c_args[2]
    cdef double *c_args_param = NULL
    cdef int nb_args = 0
    if args is not None:
        nb_args = len(args)
        if nb_args > 2:
            raise Exception("Easings do not support more than 2 arguments")
        for i, arg in enumerate(args):
            c_args[i] = arg
        c_args_param = c_args

    cdef double c_offsets[2]
    cdef double *c_offsets_param = NULL
    if offsets is not None:
        c_offsets[0] = offsets[0]
        c_offsets[1] = offsets[1]
        c_offsets_param = c_offsets

    cdef double dst
    cdef int ret
    if evaluate:
        ret = ngl_easing_evaluate(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception("Error evaluating %s" % name)
    else:
        ret = ngl_easing_solve(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception("Error solving %s" % name)

    return dst


def easing_evaluate(name, t, args=None, offsets=None):
    return _eval_solve(name, t, args, offsets, True)


def easing_solve(name, v, args=None, offsets=None):
    return _eval_solve(name, v, args, offsets, False)


cdef class Viewer:
    cdef ngl_ctx *ctx
    cdef object capture_buffer

    def __cinit__(self):
        self.ctx = ngl_create()
        if self.ctx is NULL:
            raise MemoryError()

    def configure(self, **kwargs):
        cdef ngl_config config
        memset(&config, 0, sizeof(config));
        config.platform = kwargs.get('platform', PLATFORM_AUTO)
        config.backend = kwargs.get('backend', BACKEND_AUTO)
        config.display = kwargs.get('display', 0)
        config.window = kwargs.get('window', 0)
        config.handle = kwargs.get('handle', 0)
        config.swap_interval = kwargs.get('swap_interval', -1)
        config.offscreen = kwargs.get('offscreen', 0)
        config.width = kwargs.get('width', 0)
        config.height = kwargs.get('height', 0)
        viewport = kwargs.get('viewport', (0, 0, 0, 0))
        for i in range(4):
            config.viewport[i] = viewport[i]
        config.samples = kwargs.get('samples', 0)
        config.set_surface_pts = kwargs.get('set_surface_pts', 0)
        clear_color = kwargs.get('clear_color', (0.0, 0.0, 0.0, 1.0))
        for i in range(4):
            config.clear_color[i] = clear_color[i]
        self.capture_buffer = kwargs.get('capture_buffer')
        if self.capture_buffer is not None:
            config.capture_buffer = self.capture_buffer
        return ngl_configure(self.ctx, &config)

    def resize(self, width, height, viewport=None):
        if viewport is None:
            return ngl_resize(self.ctx, width, height, NULL)
        cdef int c_viewport[4]
        for i in range(4):
            c_viewport[i] = viewport[i]
        return ngl_resize(self.ctx, width, height, c_viewport)

    def set_scene(self, _Node scene):
        return ngl_set_scene(self.ctx, NULL if scene is None else scene.ctx)

    def set_scene_from_string(self, s):
        cdef ngl_node *scene = ngl_node_deserialize(s);
        ret = ngl_set_scene(self.ctx, scene)
        ngl_node_unrefp(&scene)
        return ret

    def draw(self, double t):
        with nogil:
            ngl_draw(self.ctx, t)

    def dot(self, double t):
        cdef char *s;
        with nogil:
            s = ngl_dot(self.ctx, t)
        return _ret_pystr(s) if s else None

    def __dealloc__(self):
        ngl_freep(&self.ctx)
