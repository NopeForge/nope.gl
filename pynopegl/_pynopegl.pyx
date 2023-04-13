# cython: c_string_type=unicode, c_string_encoding=utf8
#
# Copyright 2016-2022 GoPro Inc.
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

from cpython cimport array
from libc.stdint cimport int32_t, uint8_t, uint32_t, uintptr_t
from libc.stdlib cimport calloc, free
from libc.string cimport memset


cdef extern from "nopegl.h":
    cdef int NGL_LOG_VERBOSE
    cdef int NGL_LOG_DEBUG
    cdef int NGL_LOG_INFO
    cdef int NGL_LOG_WARNING
    cdef int NGL_LOG_ERROR
    cdef int NGL_LOG_QUIET

    void ngl_log_set_min_level(int level)

    cdef struct ngl_node

    ngl_node *ngl_node_create(uint32_t type)
    ngl_node *ngl_node_ref(ngl_node *node)
    void ngl_node_unrefp(ngl_node **nodep)
    int ngl_node_param_add_nodes(ngl_node *node, const char *key, int nb_nodes, ngl_node **nodes)
    int ngl_node_param_add_f64s(ngl_node *node, const char *key, int nb_f64s, double *f64s)
    int ngl_node_param_set_bool(ngl_node *node, const char *key, int value)
    int ngl_node_param_set_data(ngl_node *node, const char *key, int size, const void *data)
    int ngl_node_param_set_dict(ngl_node *node, const char *key, const char *name, ngl_node *value)
    int ngl_node_param_set_f32(ngl_node *node, const char *key, float value)
    int ngl_node_param_set_f64(ngl_node *node, const char *key, double value)
    int ngl_node_param_set_flags(ngl_node *node, const char *key, const char *value)
    int ngl_node_param_set_i32(ngl_node *node, const char *key, int value)
    int ngl_node_param_set_ivec2(ngl_node *node, const char *key, const int *value)
    int ngl_node_param_set_ivec3(ngl_node *node, const char *key, const int *value)
    int ngl_node_param_set_ivec4(ngl_node *node, const char *key, const int *value)
    int ngl_node_param_set_mat4(ngl_node *node, const char *key, const float *value)
    int ngl_node_param_set_node(ngl_node *node, const char *key, ngl_node *value)
    int ngl_node_param_set_rational(ngl_node *node, const char *key, int num, int den)
    int ngl_node_param_set_select(ngl_node *node, const char *key, const char *value)
    int ngl_node_param_set_str(ngl_node *node, const char *key, const char *value)
    int ngl_node_param_set_u32(ngl_node *node, const char *key, const unsigned value)
    int ngl_node_param_set_uvec2(ngl_node *node, const char *key, const unsigned *value)
    int ngl_node_param_set_uvec3(ngl_node *node, const char *key, const unsigned *value)
    int ngl_node_param_set_uvec4(ngl_node *node, const char *key, const unsigned *value)
    int ngl_node_param_set_vec2(ngl_node *node, const char *key, const float *value)
    int ngl_node_param_set_vec3(ngl_node *node, const char *key, const float *value)
    int ngl_node_param_set_vec4(ngl_node *node, const char *key, const float *value)
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
    cdef int NGL_PLATFORM_WAYLAND

    cdef int NGL_BACKEND_AUTO
    cdef int NGL_BACKEND_OPENGL
    cdef int NGL_BACKEND_OPENGLES
    cdef int NGL_BACKEND_VULKAN

    cdef int NGL_CAP_BLOCK
    cdef int NGL_CAP_COMPUTE
    cdef int NGL_CAP_DEPTH_STENCIL_RESOLVE
    cdef int NGL_CAP_INSTANCED_DRAW
    cdef int NGL_CAP_MAX_COLOR_ATTACHMENTS
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_COUNT_X
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Y
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Z
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_INVOCATIONS
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_SIZE_X
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Y
    cdef int NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Z
    cdef int NGL_CAP_MAX_COMPUTE_SHARED_MEMORY_SIZE
    cdef int NGL_CAP_MAX_SAMPLES
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_1D
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_2D
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_3D
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_CUBE
    cdef int NGL_CAP_NPOT_TEXTURE
    cdef int NGL_CAP_SHADER_TEXTURE_LOD
    cdef int NGL_CAP_TEXTURE_3D
    cdef int NGL_CAP_TEXTURE_CUBE
    cdef int NGL_CAP_UINT_UNIFORMS

    cdef struct ngl_cap:
        unsigned id
        const char *string_id
        uint32_t value

    cdef struct ngl_backend:
        int id
        const char *string_id
        const char *name
        int is_default
        ngl_cap *caps
        int nb_caps

    cdef struct ngl_ctx

    cdef struct ngl_config_gl:
        int external
        uint32_t external_framebuffer

    cdef struct ngl_config:
        int  platform
        int  backend
        void *backend_config
        uintptr_t display
        uintptr_t window
        int  swap_interval
        int  offscreen
        int  width
        int  height
        int  viewport[4]
        int  samples
        int  set_surface_pts
        float clear_color[4]
        void *capture_buffer
        int capture_buffer_type
        int hud
        int hud_measure_window
        int hud_refresh_rate[2]
        const char *hud_export_filename
        int hud_scale

    cdef union ngl_livectl_data:
        float f[4]
        int32_t i[4]
        uint32_t u[4]
        float m[16]
        char *s

    cdef struct ngl_livectl:
        char *id
        uint32_t node_type
        ngl_node *node
        ngl_livectl_data val
        ngl_livectl_data min
        ngl_livectl_data max

    ngl_ctx *ngl_create()
    int ngl_backends_probe(const ngl_config *user_config, int *nb_backendsp, ngl_backend **backendsp)
    int ngl_backends_get(const ngl_config *user_config, int *nb_backendsp, ngl_backend **backendsp)
    void ngl_backends_freep(ngl_backend **backendsp)
    int ngl_configure(ngl_ctx *s, ngl_config *config)
    int ngl_resize(ngl_ctx *s, int width, int height, const int *viewport)
    int ngl_set_capture_buffer(ngl_ctx *s, void *capture_buffer)
    int ngl_set_scene(ngl_ctx *s, ngl_node *scene)
    int ngl_draw(ngl_ctx *s, double t) nogil
    char *ngl_dot(ngl_ctx *s, double t) nogil
    int ngl_livectls_get(ngl_node *scene, int *nb_livectlsp, ngl_livectl **livectlsp)
    void ngl_livectls_freep(ngl_livectl **livectlsp)
    void ngl_freep(ngl_ctx **ss)

    int ngl_easing_evaluate(const char *name, const double *args, int nb_args,
                            const double *offsets, double t, double *v)
    int ngl_easing_derivate(const char *name, const double *args, int nb_args,
                            const double *offsets, double t, double *v)
    int ngl_easing_solve(const char *name, const double *args, int nb_args,
                         const double *offsets, double v, double *t)

    int ngl_gl_wrap_framebuffer(ngl_ctx *s, uint32_t framebuffer)

PLATFORM_AUTO    = NGL_PLATFORM_AUTO
PLATFORM_XLIB    = NGL_PLATFORM_XLIB
PLATFORM_ANDROID = NGL_PLATFORM_ANDROID
PLATFORM_MACOS   = NGL_PLATFORM_MACOS
PLATFORM_IOS     = NGL_PLATFORM_IOS
PLATFORM_WINDOWS = NGL_PLATFORM_WINDOWS
PLATFORM_WAYLAND = NGL_PLATFORM_WAYLAND

BACKEND_AUTO      = NGL_BACKEND_AUTO
BACKEND_OPENGL    = NGL_BACKEND_OPENGL
BACKEND_OPENGLES  = NGL_BACKEND_OPENGLES
BACKEND_VULKAN    = NGL_BACKEND_VULKAN

CAP_BLOCK                          = NGL_CAP_BLOCK
CAP_COMPUTE                        = NGL_CAP_COMPUTE
CAP_DEPTH_STENCIL_RESOLVE          = NGL_CAP_DEPTH_STENCIL_RESOLVE
CAP_INSTANCED_DRAW                 = NGL_CAP_INSTANCED_DRAW
CAP_MAX_COLOR_ATTACHMENTS          = NGL_CAP_MAX_COLOR_ATTACHMENTS
CAP_MAX_COMPUTE_GROUP_COUNT_X      = NGL_CAP_MAX_COMPUTE_GROUP_COUNT_X
CAP_MAX_COMPUTE_GROUP_COUNT_Y      = NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Y
CAP_MAX_COMPUTE_GROUP_COUNT_Z      = NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Z
CAP_MAX_COMPUTE_GROUP_INVOCATIONS  = NGL_CAP_MAX_COMPUTE_GROUP_INVOCATIONS
CAP_MAX_COMPUTE_GROUP_SIZE_X       = NGL_CAP_MAX_COMPUTE_GROUP_SIZE_X
CAP_MAX_COMPUTE_GROUP_SIZE_Y       = NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Y
CAP_MAX_COMPUTE_GROUP_SIZE_Z       = NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Z
CAP_MAX_COMPUTE_SHARED_MEMORY_SIZE = NGL_CAP_MAX_COMPUTE_SHARED_MEMORY_SIZE
CAP_MAX_SAMPLES                    = NGL_CAP_MAX_SAMPLES
CAP_MAX_TEXTURE_DIMENSION_1D       = NGL_CAP_MAX_TEXTURE_DIMENSION_1D
CAP_MAX_TEXTURE_DIMENSION_2D       = NGL_CAP_MAX_TEXTURE_DIMENSION_2D
CAP_MAX_TEXTURE_DIMENSION_3D       = NGL_CAP_MAX_TEXTURE_DIMENSION_3D
CAP_MAX_TEXTURE_DIMENSION_CUBE     = NGL_CAP_MAX_TEXTURE_DIMENSION_CUBE
CAP_NPOT_TEXTURE                   = NGL_CAP_NPOT_TEXTURE
CAP_SHADER_TEXTURE_LOD             = NGL_CAP_SHADER_TEXTURE_LOD
CAP_TEXTURE_3D                     = NGL_CAP_TEXTURE_3D
CAP_TEXTURE_CUBE                   = NGL_CAP_TEXTURE_CUBE
CAP_UINT_UNIFORMS                  = NGL_CAP_UINT_UNIFORMS

LOG_VERBOSE = NGL_LOG_VERBOSE
LOG_DEBUG   = NGL_LOG_DEBUG
LOG_INFO    = NGL_LOG_INFO
LOG_WARNING = NGL_LOG_WARNING
LOG_ERROR   = NGL_LOG_ERROR
LOG_QUIET   = NGL_LOG_QUIET

cdef _ret_pystr(char *s):
    try:
        pystr = <bytes>s
    finally:
        free(s)
    return pystr

include "nodes_def.pyx"

def log_set_min_level(int level):
    ngl_log_set_min_level(level)


cdef class _Node:
    cdef ngl_node *ctx

    @property
    def cptr(self):
        return <uintptr_t>self.ctx

    def __init__(self, uintptr_t ctx=0):
        assert self.ctx is NULL
        if ctx:
            self.ctx = ngl_node_ref(<ngl_node *>ctx)
            return
        self.ctx = ngl_node_create(self.type_id)
        if self.ctx is NULL:
            raise MemoryError()

    def serialize(self):
        return _ret_pystr(ngl_node_serialize(self.ctx))

    def dot(self):
        return _ret_pystr(ngl_node_dot(self.ctx))

    def __dealloc__(self):
        ngl_node_unrefp(&self.ctx)

    def _param_set_bool(self, const char *key, bint value):
        return ngl_node_param_set_bool(self.ctx, key, value)

    def _param_set_data(self, const char *key, array.array arg):
        return ngl_node_param_set_data(self.ctx, key,
                                       arg.buffer_info()[1] * arg.itemsize,
                                       arg.data.as_voidptr)

    def _param_set_dict(self, const char *key, const char *name, _Node value):
        cdef ngl_node *node = value.ctx if value is not None else NULL
        return ngl_node_param_set_dict(self.ctx, key, name, node)

    def _param_set_f32(self, const char *key, float value):
        return ngl_node_param_set_f32(self.ctx, key, value)

    def _param_set_f64(self, const char *key, double value):
        return ngl_node_param_set_f64(self.ctx, key, value)

    def _param_set_flags(self, const char *key, const char *value):
        return ngl_node_param_set_flags(self.ctx, key, value)

    def _param_set_i32(self, const char *key, int value):
        return ngl_node_param_set_i32(self.ctx, key, value)

    def _param_set_ivec2(self, const char *key, value):
        cdef int[2] ivec = value
        return ngl_node_param_set_ivec2(self.ctx, key, ivec)

    def _param_set_ivec3(self, const char *key, value):
        cdef int[3] ivec = value
        return ngl_node_param_set_ivec3(self.ctx, key, ivec)

    def _param_set_ivec4(self, const char *key, value):
        cdef int[4] ivec = value
        return ngl_node_param_set_ivec4(self.ctx, key, ivec)

    def _param_set_mat4(self, const char *key, value):
        cdef float[16] mat = value
        return ngl_node_param_set_mat4(self.ctx, key, mat)

    def _param_set_node(self, const char *key, _Node value):
        return ngl_node_param_set_node(self.ctx, key, value.ctx)

    def _param_set_rational(self, const char *key, int num, int den):
        return ngl_node_param_set_rational(self.ctx, key, num, den)

    def _param_set_select(self, const char *key, const char *value):
        return ngl_node_param_set_select(self.ctx, key, value)

    def _param_set_str(self, const char *key, const char *value):
        return ngl_node_param_set_str(self.ctx, key, value)

    def _param_set_u32(self, const char *key, const unsigned value):
        return ngl_node_param_set_u32(self.ctx, key, value)

    def _param_set_uvec2(self, const char *key, value):
        cdef unsigned[2] uvec = value
        return ngl_node_param_set_uvec2(self.ctx, key, uvec)

    def _param_set_uvec3(self, const char *key, value):
        cdef unsigned[3] uvec = value
        return ngl_node_param_set_uvec3(self.ctx, key, uvec)

    def _param_set_uvec4(self, const char *key, value):
        cdef unsigned[4] uvec = value
        return ngl_node_param_set_uvec4(self.ctx, key, uvec)

    def _param_set_vec2(self, const char *key, value):
        cdef float[2] vec = value
        return ngl_node_param_set_vec2(self.ctx, key, vec)

    def _param_set_vec3(self, const char *key, value):
        cdef float[3] vec = value
        return ngl_node_param_set_vec3(self.ctx, key, vec)

    def _param_set_vec4(self, const char *key, value):
        cdef float[4] vec = value
        return ngl_node_param_set_vec4(self.ctx, key, vec)

    def _param_add_nodes(self, const char *key, int nb_nodes, nodes):
        nodes_c = <ngl_node **>calloc(nb_nodes, sizeof(ngl_node *))
        if nodes_c is NULL:
            raise MemoryError()
        cdef int i
        for i, node in enumerate(nodes):
            nodes_c[i] = (<_Node>node).ctx
        ret = ngl_node_param_add_nodes(self.ctx, key, nb_nodes, nodes_c)
        free(nodes_c)
        return ret

    def _eval_f32(self, double t):
        cdef float f32
        ngl_anim_evaluate(self.ctx, &f32, t)
        return f32

    def _eval_vec2(self, double t):
        cdef float[2] vec
        ngl_anim_evaluate(self.ctx, vec, t)
        return (vec[0], vec[1])

    def _eval_vec3(self, double t):
        cdef float[3] vec
        ngl_anim_evaluate(self.ctx, vec, t)
        return (vec[0], vec[1], vec[2])

    def _eval_vec4(self, double t):
        cdef float[4] vec
        ngl_anim_evaluate(self.ctx, vec, t)
        return (vec[0], vec[1], vec[2], vec[3])

    def _param_add_f64s(self, const char *key, int nb_f64s, f64s):
        f64s_c = <double *>calloc(nb_f64s, sizeof(double))
        if f64s_c is NULL:
            raise MemoryError()
        cdef int i
        for i, f64 in enumerate(f64s):
            f64s_c[i] = f64
        ret = ngl_node_param_add_f64s(self.ctx, key, nb_f64s, f64s_c)
        free(f64s_c)
        return ret


_ANIM_EVALUATE, _ANIM_DERIVATE, _ANIM_SOLVE = range(3)


cdef _animate(name, src, args, offsets, mode):
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
    if mode == _ANIM_EVALUATE:
        ret = ngl_easing_evaluate(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception(f'Error evaluating {name}')
    elif mode == _ANIM_DERIVATE:
        ret = ngl_easing_derivate(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception(f'Error derivating {name}')
    elif mode == _ANIM_SOLVE:
        ret = ngl_easing_solve(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception(f'Error solving {name}')
    else:
        raise Exception(f'Unknown mode {mode}')

    return dst


def easing_evaluate(name, t, args=None, offsets=None):
    return _animate(name, t, args, offsets, _ANIM_EVALUATE)


def easing_derivate(name, t, args=None, offsets=None):
    return _animate(name, t, args, offsets, _ANIM_DERIVATE)


def easing_solve(name, v, args=None, offsets=None):
    return _animate(name, v, args, offsets, _ANIM_SOLVE)


_PROBE_MODE_FULL = 0
_PROBE_MODE_NO_GRAPHICS = 1


def _probe_backends(mode, **kwargs):
    cdef ngl_config config
    cdef ngl_config *configp = NULL
    if kwargs:
        configp = &config
        Context._init_ngl_config_from_dict(configp, kwargs)
    cdef int nb_backends = 0
    cdef ngl_backend *backend = NULL
    cdef ngl_backend *backends = NULL
    cdef ngl_cap *cap = NULL
    cdef int ret
    if mode == _PROBE_MODE_NO_GRAPHICS:
        ret = ngl_backends_get(configp, &nb_backends, &backends)
    else:
        ret = ngl_backends_probe(configp, &nb_backends, &backends)
    if ret < 0:
        raise Exception("Error probing backends")
    backend_set = []
    for i in range(nb_backends):
        backend = &backends[i]
        caps = []
        for j in range(backend.nb_caps):
            cap = &backend.caps[j]
            caps.append(dict(
                id=cap.id,
                string_id=cap.string_id,
                value=cap.value,
            ))
        backend_set.append(dict(
            id=backend.id,
            string_id=backend.string_id,
            name=backend.name,
            is_default=True if backend.is_default else False,
            caps=caps,
        ))
    ngl_backends_freep(&backends)
    return backend_set


def probe_backends(**kwargs):
    return _probe_backends(_PROBE_MODE_FULL, **kwargs)


def get_backends(**kwargs):
    return _probe_backends(_PROBE_MODE_NO_GRAPHICS, **kwargs)


LIVECTL_INFO = {}  # Filled dynamically by the Python side

_TYPES_COUNT = {
    'bool': 1, 'mat4': 16, 'str': 0,
    'f32': 1, 'vec2': 2, 'vec3': 3, 'vec4': 4,
    'i32': 1, 'ivec2': 2, 'ivec3': 3, 'ivec4': 4,
    'u32': 1, 'uvec2': 2, 'uvec3': 3, 'uvec4': 4,
}


def get_livectls(_Node scene):
    cdef int nb_livectls = 0
    cdef ngl_livectl *livectls = NULL
    cdef int ret = ngl_livectls_get(scene.ctx, &nb_livectls, &livectls)
    if ret < 0:
        raise Exception('Error getting live controls')

    livectl_dict = {}
    for i in range(nb_livectls):
        livectl = &livectls[i]
        py_cls, data_type = LIVECTL_INFO[livectl.node_type]
        data_count = _TYPES_COUNT[data_type]
        py_node = py_cls(ctx=<uintptr_t>livectl.node)
        py_data = dict(node=py_node, node_type=py_cls.__name__)
        if data_type[0] in 'fvm':
            py_data['val'] = [livectl.val.f[i] for i in range(data_count)]
            py_data['min'] = [livectl.min.f[i] for i in range(data_count)]
            py_data['max'] = [livectl.max.f[i] for i in range(data_count)]
        elif data_type[0] == 'i':
            py_data['val'] = [livectl.val.i[i] for i in range(data_count)]
            py_data['min'] = [livectl.min.i[i] for i in range(data_count)]
            py_data['max'] = [livectl.max.i[i] for i in range(data_count)]
        elif data_type[0] == 'u':
            py_data['val'] = [livectl.val.u[i] for i in range(data_count)]
            py_data['min'] = [livectl.min.u[i] for i in range(data_count)]
            py_data['max'] = [livectl.max.u[i] for i in range(data_count)]
        elif data_type == 'bool':
            py_data['val'] = bool(livectl.val.i[0])
        elif data_type == 'str':
            py_data['val'] = livectl.val.s
        if data_type[0] in 'fiu' and data_count == 1:
            py_data['val'] = py_data['val'][0]
            py_data['min'] = py_data['min'][0]
            py_data['max'] = py_data['max'][0]
        livectl_dict[livectl.id] = py_data

    ngl_livectls_freep(&livectls)
    return livectl_dict


cdef class ConfigGL:
    cdef ngl_config_gl config

    def __cinit__(self, external=False, external_framebuffer=0):
        memset(&self.config, 0, sizeof(self.config))
        self.config.external = external
        self.config.external_framebuffer = external_framebuffer

    def cptr(self):
        return <uintptr_t>&self.config


cdef class Context:
    cdef ngl_ctx *ctx
    cdef object capture_buffer

    def __cinit__(self):
        self.ctx = ngl_create()
        if self.ctx is NULL:
            raise MemoryError()

    @staticmethod
    cdef void _init_ngl_config_from_dict(ngl_config *config, kwargs):
        memset(config, 0, sizeof(ngl_config))
        config.platform = kwargs.get('platform', PLATFORM_AUTO)
        config.backend = kwargs.get('backend', BACKEND_AUTO)
        backend_config = kwargs.get('backend_config')
        cdef uintptr_t ptr
        if backend_config is not None:
            ptr = backend_config.cptr()
            config.backend_config = <void *>ptr
        config.display = kwargs.get('display', 0)
        config.window = kwargs.get('window', 0)
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
        capture_buffer = kwargs.get('capture_buffer')
        if capture_buffer is not None:
            config.capture_buffer = <uint8_t *>capture_buffer
        config.hud = kwargs.get('hud', 0)
        config.hud_measure_window = kwargs.get('hud_measure_window', 0)
        hud_refresh_rate = kwargs.get('hud_refresh_rate', (0, 0))
        config.hud_refresh_rate[0] = hud_refresh_rate[0]
        config.hud_refresh_rate[1] = hud_refresh_rate[1]
        hud_export_filename = kwargs.get('hud_export_filename')
        if hud_export_filename is not None:
            config.hud_export_filename = hud_export_filename
        config.hud_scale = kwargs.get('hud_scale', 0)

    def configure(self, **kwargs):
        self.capture_buffer = kwargs.get('capture_buffer')
        cdef ngl_config config
        Context._init_ngl_config_from_dict(&config, kwargs)
        return ngl_configure(self.ctx, &config)

    def resize(self, width, height, viewport=None):
        if viewport is None:
            return ngl_resize(self.ctx, width, height, NULL)
        cdef int c_viewport[4]
        for i in range(4):
            c_viewport[i] = viewport[i]
        return ngl_resize(self.ctx, width, height, c_viewport)

    def set_capture_buffer(self, capture_buffer):
        self.capture_buffer = capture_buffer
        cdef uint8_t *ptr = NULL
        if self.capture_buffer is not None:
            ptr = <uint8_t *>self.capture_buffer
        return ngl_set_capture_buffer(self.ctx, ptr)

    def set_scene(self, _Node scene):
        return ngl_set_scene(self.ctx, NULL if scene is None else scene.ctx)

    def set_scene_from_string(self, s):
        cdef ngl_node *scene = ngl_node_deserialize(s)
        ret = ngl_set_scene(self.ctx, scene)
        ngl_node_unrefp(&scene)
        return ret

    def draw(self, double t):
        with nogil:
            ret = ngl_draw(self.ctx, t)
        return ret

    def dot(self, double t):
        cdef char *s
        with nogil:
            s = ngl_dot(self.ctx, t)
        return _ret_pystr(s) if s else None

    def __dealloc__(self):
        ngl_freep(&self.ctx)

    def gl_wrap_framebuffer(self, uint32_t framebuffer):
        return ngl_gl_wrap_framebuffer(self.ctx, framebuffer)
