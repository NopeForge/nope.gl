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
    cdef enum ngl_log_level:
        NGL_LOG_VERBOSE,
        NGL_LOG_DEBUG,
        NGL_LOG_INFO,
        NGL_LOG_WARNING,
        NGL_LOG_ERROR,
        NGL_LOG_QUIET
        NGL_LOG_MAX_ENUM

    void ngl_log_set_min_level(ngl_log_level level)

    cdef struct ngl_node

    ngl_node *ngl_node_create(uint32_t type)
    ngl_node *ngl_node_ref(ngl_node *node)
    void ngl_node_unrefp(ngl_node **nodep)
    int ngl_node_param_add_nodes(ngl_node *node, const char *key, size_t nb_nodes, ngl_node **nodes)
    int ngl_node_param_add_f64s(ngl_node *node, const char *key, size_t nb_f64s, double *f64s)
    int ngl_node_param_set_bool(ngl_node *node, const char *key, int value)
    int ngl_node_param_set_data(ngl_node *node, const char *key, size_t size, const void *data)
    int ngl_node_param_set_dict(ngl_node *node, const char *key, const char *name, ngl_node *value)
    int ngl_node_param_set_f32(ngl_node *node, const char *key, float value)
    int ngl_node_param_set_f64(ngl_node *node, const char *key, double value)
    int ngl_node_param_set_flags(ngl_node *node, const char *key, const char *value)
    int ngl_node_param_set_i32(ngl_node *node, const char *key, int32_t value)
    int ngl_node_param_set_ivec2(ngl_node *node, const char *key, const int32_t *value)
    int ngl_node_param_set_ivec3(ngl_node *node, const char *key, const int32_t *value)
    int ngl_node_param_set_ivec4(ngl_node *node, const char *key, const int32_t *value)
    int ngl_node_param_set_mat4(ngl_node *node, const char *key, const float *value)
    int ngl_node_param_set_node(ngl_node *node, const char *key, ngl_node *value)
    int ngl_node_param_set_rational(ngl_node *node, const char *key, int32_t num, int32_t den)
    int ngl_node_param_set_select(ngl_node *node, const char *key, const char *value)
    int ngl_node_param_set_str(ngl_node *node, const char *key, const char *value)
    int ngl_node_param_set_u32(ngl_node *node, const char *key, const uint32_t value)
    int ngl_node_param_set_uvec2(ngl_node *node, const char *key, const uint32_t *value)
    int ngl_node_param_set_uvec3(ngl_node *node, const char *key, const uint32_t *value)
    int ngl_node_param_set_uvec4(ngl_node *node, const char *key, const uint32_t *value)
    int ngl_node_param_set_vec2(ngl_node *node, const char *key, const float *value)
    int ngl_node_param_set_vec3(ngl_node *node, const char *key, const float *value)
    int ngl_node_param_set_vec4(ngl_node *node, const char *key, const float *value)
    int ngl_anim_evaluate(ngl_node *anim, void *dst, double t)

    cdef enum ngl_platform_type:
        NGL_PLATFORM_AUTO,
        NGL_PLATFORM_XLIB,
        NGL_PLATFORM_ANDROID,
        NGL_PLATFORM_MACOS,
        NGL_PLATFORM_IOS,
        NGL_PLATFORM_WINDOWS,
        NGL_PLATFORM_WAYLAND
        NGL_PLATFORM_MAX_ENUM

    cdef enum ngl_backend_type:
        NGL_BACKEND_AUTO,
        NGL_BACKEND_OPENGL,
        NGL_BACKEND_OPENGLES,
        NGL_BACKEND_VULKAN
        NGL_BACKEND_MAX_ENUM

    cdef enum ngl_capture_buffer_type:
        NGL_CAPTURE_BUFFER_TYPE_CPU,
        NGL_CAPTURE_BUFFER_TYPE_COREVIDEO,
        NGL_CAPTURE_BUFFER_TYPE_MAX_ENUM

    cdef int NGL_CAP_COMPUTE
    cdef int NGL_CAP_DEPTH_STENCIL_RESOLVE
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
    cdef int NGL_CAP_MAX_TEXTURE_ARRAY_LAYERS
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_1D
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_2D
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_3D
    cdef int NGL_CAP_MAX_TEXTURE_DIMENSION_CUBE
    cdef int NGL_CAP_TEXT_LIBRARIES

    cdef struct ngl_cap:
        unsigned id
        const char *string_id
        uint32_t value

    cdef struct ngl_backend:
        ngl_backend_type id
        const char *string_id
        const char *name
        int is_default
        ngl_cap *caps
        size_t nb_caps

    cdef struct ngl_scene

    cdef struct ngl_scene_params:
        ngl_node *root
        double duration
        int32_t framerate[2]
        int32_t aspect_ratio[2]

    ngl_scene_params ngl_scene_default_params(ngl_node *root)
    ngl_scene *ngl_scene_create()
    int ngl_scene_init(ngl_scene *s, const ngl_scene_params *params)
    const ngl_scene_params *ngl_scene_get_params(const ngl_scene *s)
    int ngl_scene_get_filepaths(ngl_scene *s, char ***filepathsp, size_t *nb_filepathsp)
    int ngl_scene_update_filepath(ngl_scene *s, size_t index, const char *filepath)
    int ngl_scene_init_from_str(ngl_scene *s, const char *str)
    char *ngl_scene_serialize(const ngl_scene *scene)
    char *ngl_scene_dot(const ngl_scene *scene)
    void ngl_scene_unrefp(ngl_scene **sp)

    cdef struct ngl_ctx

    cdef struct ngl_config_gl:
        int external
        uint32_t external_framebuffer

    cdef struct ngl_config:
        ngl_platform_type  platform
        ngl_backend_type backend
        void *backend_config
        uintptr_t display
        uintptr_t window
        int  swap_interval
        int  offscreen
        uint32_t width
        uint32_t height
        uint32_t samples
        int  set_surface_pts
        float clear_color[4]
        void *capture_buffer
        ngl_capture_buffer_type capture_buffer_type
        int hud
        uint32_t hud_measure_window
        uint32_t hud_refresh_rate[2]
        const char *hud_export_filename
        uint32_t hud_scale
        int debug

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
    int ngl_backends_probe(const ngl_config *user_config, size_t *nb_backendsp, ngl_backend **backendsp)
    int ngl_backends_get(const ngl_config *user_config, size_t *nb_backendsp, ngl_backend **backendsp)
    void ngl_backends_freep(ngl_backend **backendsp)
    int ngl_configure(ngl_ctx *s, ngl_config *config)
    int ngl_get_backend(ngl_ctx *s, ngl_backend *backend)
    void ngl_reset_backend(ngl_backend *backend)
    int ngl_resize(ngl_ctx *s, int32_t width, int32_t height)
    int ngl_get_viewport(ngl_ctx *s, int32_t *viewport)
    int ngl_set_capture_buffer(ngl_ctx *s, void *capture_buffer)
    int ngl_set_scene(ngl_ctx *s, ngl_scene *scene)
    int ngl_draw(ngl_ctx *s, double t) nogil
    char *ngl_dot(ngl_ctx *s, double t) nogil
    int ngl_livectls_get(ngl_scene *scene, size_t *nb_livectlsp, ngl_livectl **livectlsp)
    void ngl_livectls_freep(ngl_livectl **livectlsp)
    void ngl_freep(ngl_ctx **ss)

    int ngl_easing_evaluate(const char *name, const double *args, size_t nb_args,
                            const double *offsets, double t, double *v)
    int ngl_easing_derivate(const char *name, const double *args, size_t nb_args,
                            const double *offsets, double t, double *v)
    int ngl_easing_solve(const char *name, const double *args, size_t nb_args,
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

CAP_COMPUTE                        = NGL_CAP_COMPUTE
CAP_DEPTH_STENCIL_RESOLVE          = NGL_CAP_DEPTH_STENCIL_RESOLVE
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
CAP_MAX_TEXTURE_ARRAY_LAYERS       = NGL_CAP_MAX_TEXTURE_ARRAY_LAYERS
CAP_MAX_TEXTURE_DIMENSION_1D       = NGL_CAP_MAX_TEXTURE_DIMENSION_1D
CAP_MAX_TEXTURE_DIMENSION_2D       = NGL_CAP_MAX_TEXTURE_DIMENSION_2D
CAP_MAX_TEXTURE_DIMENSION_3D       = NGL_CAP_MAX_TEXTURE_DIMENSION_3D
CAP_MAX_TEXTURE_DIMENSION_CUBE     = NGL_CAP_MAX_TEXTURE_DIMENSION_CUBE
CAP_TEXT_LIBRARIES                 = NGL_CAP_TEXT_LIBRARIES

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

log_set_min_level = ngl_log_set_min_level


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

    def _param_set_i32(self, const char *key, int32_t value):
        return ngl_node_param_set_i32(self.ctx, key, value)

    def _param_set_ivec2(self, const char *key, value):
        cdef int32_t[2] ivec = value
        return ngl_node_param_set_ivec2(self.ctx, key, ivec)

    def _param_set_ivec3(self, const char *key, value):
        cdef int32_t[3] ivec = value
        return ngl_node_param_set_ivec3(self.ctx, key, ivec)

    def _param_set_ivec4(self, const char *key, value):
        cdef int32_t[4] ivec = value
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

    def _param_set_u32(self, const char *key, const uint32_t value):
        return ngl_node_param_set_u32(self.ctx, key, value)

    def _param_set_uvec2(self, const char *key, value):
        cdef uint32_t[2] uvec = value
        return ngl_node_param_set_uvec2(self.ctx, key, uvec)

    def _param_set_uvec3(self, const char *key, value):
        cdef uint32_t[3] uvec = value
        return ngl_node_param_set_uvec3(self.ctx, key, uvec)

    def _param_set_uvec4(self, const char *key, value):
        cdef uint32_t[4] uvec = value
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

    def _param_add_nodes(self, const char *key, size_t nb_nodes, nodes):
        nodes_c = <ngl_node **>calloc(nb_nodes, sizeof(ngl_node *))
        if nodes_c is NULL:
            raise MemoryError()
        cdef size_t i
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

    def _param_add_f64s(self, const char *key, size_t nb_f64s, f64s):
        f64s_c = <double *>calloc(nb_f64s, sizeof(double))
        if f64s_c is NULL:
            raise MemoryError()
        cdef size_t i
        for i, f64 in enumerate(f64s):
            f64s_c[i] = f64
        ret = ngl_node_param_add_f64s(self.ctx, key, nb_f64s, f64s_c)
        free(f64s_c)
        return ret


ANIM_EVALUATE, ANIM_DERIVATE, ANIM_SOLVE = range(3)


def animate(const char *name, double src, args, offsets, mode):
    cdef double c_args[2]
    cdef double *c_args_param = NULL
    cdef size_t nb_args = 0
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
    if mode == ANIM_EVALUATE:
        ret = ngl_easing_evaluate(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception(f'Error evaluating {name}')
    elif mode == ANIM_DERIVATE:
        ret = ngl_easing_derivate(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception(f'Error derivating {name}')
    elif mode == ANIM_SOLVE:
        ret = ngl_easing_solve(name, c_args_param, nb_args, c_offsets_param, src, &dst)
        if ret < 0:
            raise Exception(f'Error solving {name}')
    else:
        raise Exception(f'Unknown mode {mode}')

    return dst


PROBE_MODE_FULL = 0
PROBE_MODE_NO_GRAPHICS = 1


def probe_backends(mode, py_config):
    cdef ngl_config *configp = NULL
    cdef uintptr_t ptr
    if py_config is not None:
        ptr = py_config.cptr
        configp = <ngl_config *>ptr
    cdef size_t nb_backends = 0
    cdef ngl_backend *backend = NULL
    cdef ngl_backend *backends = NULL
    cdef ngl_cap *cap = NULL
    cdef int ret
    if mode == PROBE_MODE_NO_GRAPHICS:
        ret = ngl_backends_get(configp, &nb_backends, &backends)
    else:
        ret = ngl_backends_probe(configp, &nb_backends, &backends)
    if ret < 0:
        raise Exception("Error probing backends")
    backend_set = {}
    for i in range(nb_backends):
        backend = &backends[i]
        caps = {}
        for j in range(backend.nb_caps):
            cap = &backend.caps[j]
            caps[cap.string_id] = cap.value
        backend_set[backend.string_id] = dict(
            id=backend.id,
            name=backend.name,
            is_default=True if backend.is_default else False,
            caps=caps,
        )
    ngl_backends_freep(&backends)
    return backend_set


LIVECTL_INFO = {}  # Filled dynamically by the Python side

_TYPES_COUNT = {
    'bool': 1, 'mat4': 16, 'str': 0,
    'f32': 1, 'vec2': 2, 'vec3': 3, 'vec4': 4,
    'i32': 1, 'ivec2': 2, 'ivec3': 3, 'ivec4': 4,
    'u32': 1, 'uvec2': 2, 'uvec3': 3, 'uvec4': 4,
}


def get_livectls(Scene scene):
    cdef size_t nb_livectls = 0
    cdef ngl_livectl *livectls = NULL
    cdef int ret = ngl_livectls_get(scene.ctx, &nb_livectls, &livectls)
    if ret < 0:
        raise Exception('Error getting live controls')

    livectl_dict = {}
    for j in range(nb_livectls):
        livectl = &livectls[j]
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


cdef class Scene:
    cdef ngl_scene *ctx
    cdef object root

    def __cinit__(self):
        self.ctx = ngl_scene_create()
        if self.ctx is NULL:
            raise MemoryError()

    @classmethod
    def from_params(cls, _Node root, duration, framerate, aspect_ratio):
        scene = cls()
        cdef uintptr_t sptr = scene.cptr
        cdef ngl_scene *scenep = <ngl_scene *>sptr
        cdef uintptr_t rptr = root.cptr
        cdef ngl_scene_params params = ngl_scene_default_params(<ngl_node *>rptr)
        if duration is not None:
            params.duration = duration
        if aspect_ratio is not None:
            params.aspect_ratio[0] = aspect_ratio[0]
            params.aspect_ratio[1] = aspect_ratio[1]
        if framerate is not None:
            params.framerate[0] = framerate[0]
            params.framerate[1] = framerate[1]
        cdef int ret = ngl_scene_init(scenep, &params)
        if ret < 0:
            raise Exception("unable to initialize scene")
        scene.root = root
        return scene

    @classmethod
    def from_string(cls, const char *s):
        scene = cls()
        cdef uintptr_t sptr = scene.cptr
        cdef ngl_scene *scenep = <ngl_scene *>sptr
        cdef int ret = ngl_scene_init_from_str(scenep, s)
        if ret < 0:
            raise Exception("unable to initialize scene from string")
        cdef const ngl_scene_params *params = ngl_scene_get_params(scenep);
        # FIXME: this is limited because the node won't even have set_label()
        scene.root = _Node(ctx=<uintptr_t>params.root)
        return scene

    def serialize(self):
        return _ret_pystr(ngl_scene_serialize(self.ctx))

    def dot(self):
        return _ret_pystr(ngl_scene_dot(self.ctx))

    def update_filepath(self, size_t index, const char *filepath):
        cdef int ret = ngl_scene_update_filepath(self.ctx, index, filepath)
        if ret < 0:
            raise Exception(f'unable to update filepath at index {index} with "{filepath}"')

    @property
    def duration(self):
        assert self.ctx != NULL, "Scene not initialized"
        cdef const ngl_scene_params *params = ngl_scene_get_params(self.ctx);
        return params.duration

    @property
    def framerate(self):
        assert self.ctx != NULL, "Scene not initialized"
        cdef const ngl_scene_params *params = ngl_scene_get_params(self.ctx);
        return (params.framerate[0], params.framerate[1])

    @property
    def aspect_ratio(self):
        assert self.ctx != NULL, "Scene not initialized"
        cdef const ngl_scene_params *params = ngl_scene_get_params(self.ctx);
        return (params.aspect_ratio[0], params.aspect_ratio[1])

    @property
    def files(self):
        cdef char **filepaths
        cdef size_t nb_filepaths
        cdef int ret = ngl_scene_get_filepaths(self.ctx, &filepaths, &nb_filepaths)
        if ret < 0:
            raise Exception("unable to get filepaths")
        return [<bytes>filepaths[i] for i in range(nb_filepaths)]

    @property
    def cptr(self):
        return <uintptr_t>self.ctx

    def __dealloc__(self):
        ngl_scene_unrefp(&self.ctx)


cdef class ConfigGL:
    cdef ngl_config_gl config

    def __cinit__(self):
        memset(&self.config, 0, sizeof(self.config))

    def __init__(self, external, external_framebuffer):
        self.config.external = external
        self.config.external_framebuffer = external_framebuffer

    @property
    def cptr(self):
        return <uintptr_t>&self.config


cdef class Config:
    cdef ngl_config config

    def __cinit__(self):
        memset(&self.config, 0, sizeof(self.config))

    def __init__(
        self,
        platform,
        backend,
        backend_config,
        display,
        window,
        swap_interval,
        offscreen,
        width,
        height,
        samples,
        set_surface_pts,
        clear_color,
        capture_buffer,
        capture_buffer_type,
        hud,
        hud_measure_window,
        hud_refresh_rate,
        hud_export_filename,
        hud_scale,
        debug,
    ):
        self.config.platform = platform.value
        self.config.backend = backend.value
        cdef uintptr_t ptr
        if backend_config is not None:
            ptr = backend_config.cptr
            self.config.backend_config = <void *>ptr
        self.config.display = display
        self.config.window = window
        self.config.swap_interval = swap_interval
        self.config.offscreen = offscreen
        self.config.width = width
        self.config.height = height
        self.config.samples = samples
        self.config.set_surface_pts = set_surface_pts
        for i in range(4):
            self.config.clear_color[i] = clear_color[i]
        if capture_buffer is not None:
            self.config.capture_buffer = <uint8_t *>capture_buffer
        self.config.capture_buffer_type = capture_buffer_type
        self.config.hud = hud
        self.config.hud_measure_window = hud_measure_window
        self.config.hud_refresh_rate[0] = hud_refresh_rate[0]
        self.config.hud_refresh_rate[1] = hud_refresh_rate[1]
        if hud_export_filename is not None:
            self.config.hud_export_filename = hud_export_filename
        self.config.hud_scale = hud_scale
        self.config.debug = debug

    @property
    def cptr(self):
        return <uintptr_t>&self.config


cdef class Context:
    cdef ngl_ctx *ctx
    cdef object capture_buffer

    def __cinit__(self):
        self.ctx = ngl_create()
        if self.ctx is NULL:
            raise MemoryError()

    def configure(self, py_config):
        self.capture_buffer = py_config.capture_buffer
        cdef uintptr_t ptr = py_config.cptr
        cdef ngl_config *configp = <ngl_config *>ptr
        return ngl_configure(self.ctx, configp)

    def get_backend(self):
        cdef ngl_backend backend
        memset(&backend, 0, sizeof(backend))
        cdef int ret = ngl_get_backend(self.ctx, &backend)
        if ret < 0:
            raise Exception("Error getting backend information")
        caps = {}
        for i in range(backend.nb_caps):
            cap = &backend.caps[i]
            caps[cap.string_id] = cap.value
        py_backend = dict(
            id=backend.id,
            name=backend.name,
            is_default=True if backend.is_default else False,
            caps=caps,
        )
        ngl_reset_backend(&backend)
        return py_backend

    def resize(self, width, height):
        return ngl_resize(self.ctx, width, height)

    def get_viewport(self):
        cdef int32_t vp[4]
        cdef int ret = ngl_get_viewport(self.ctx, vp)
        if ret < 0:
            raise Exception("Error getting the viewport")
        return tuple(v for v in vp)

    def set_capture_buffer(self, capture_buffer):
        self.capture_buffer = capture_buffer
        cdef uint8_t *ptr = NULL
        if self.capture_buffer is not None:
            ptr = <uint8_t *>self.capture_buffer
        return ngl_set_capture_buffer(self.ctx, ptr)

    def set_scene(self, Scene scene):
        cdef ngl_scene *c_scene = NULL
        cdef uintptr_t ptr
        if scene is not None:
            ptr = scene.cptr
            c_scene = <ngl_scene *>ptr
        return ngl_set_scene(self.ctx, c_scene)

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
