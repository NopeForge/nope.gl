#
# Copyright 2022 GoPro Inc.
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

import array
import os
import platform
from typing import Mapping, Optional, Sequence, Tuple, Union

if platform.system() == "Windows":
    ngl_dll_dirs = os.getenv("NGL_DLL_DIRS")
    if ngl_dll_dirs:
        dll_dirs = ngl_dll_dirs.split(os.pathsep)
        for dll_dir in dll_dirs:
            if os.path.isdir(dll_dir):
                os.add_dll_directory(dll_dir)


import _pynodegl as _ngl
from _pynodegl import _Node

# fmt: off
ConfigGL          = _ngl.ConfigGL
Context           = _ngl.Context
easing_derivate   = _ngl.easing_derivate
easing_evaluate   = _ngl.easing_evaluate
easing_solve      = _ngl.easing_solve
get_backends      = _ngl.get_backends
get_livectls      = _ngl.get_livectls
log_set_min_level = _ngl.log_set_min_level
probe_backends    = _ngl.probe_backends

PLATFORM_AUTO    = _ngl.PLATFORM_AUTO
PLATFORM_XLIB    = _ngl.PLATFORM_XLIB
PLATFORM_ANDROID = _ngl.PLATFORM_ANDROID
PLATFORM_MACOS   = _ngl.PLATFORM_MACOS
PLATFORM_IOS     = _ngl.PLATFORM_IOS
PLATFORM_WINDOWS = _ngl.PLATFORM_WINDOWS
PLATFORM_WAYLAND = _ngl.PLATFORM_WAYLAND

BACKEND_AUTO      = _ngl.BACKEND_AUTO
BACKEND_OPENGL    = _ngl.BACKEND_OPENGL
BACKEND_OPENGLES  = _ngl.BACKEND_OPENGLES
BACKEND_VULKAN    = _ngl.BACKEND_VULKAN

CAP_BLOCK                          = _ngl.CAP_BLOCK
CAP_COMPUTE                        = _ngl.CAP_COMPUTE
CAP_DEPTH_STENCIL_RESOLVE          = _ngl.CAP_DEPTH_STENCIL_RESOLVE
CAP_INSTANCED_DRAW                 = _ngl.CAP_INSTANCED_DRAW
CAP_MAX_COLOR_ATTACHMENTS          = _ngl.CAP_MAX_COLOR_ATTACHMENTS
CAP_MAX_COMPUTE_GROUP_COUNT_X      = _ngl.CAP_MAX_COMPUTE_GROUP_COUNT_X
CAP_MAX_COMPUTE_GROUP_COUNT_Y      = _ngl.CAP_MAX_COMPUTE_GROUP_COUNT_Y
CAP_MAX_COMPUTE_GROUP_COUNT_Z      = _ngl.CAP_MAX_COMPUTE_GROUP_COUNT_Z
CAP_MAX_COMPUTE_GROUP_INVOCATIONS  = _ngl.CAP_MAX_COMPUTE_GROUP_INVOCATIONS
CAP_MAX_COMPUTE_GROUP_SIZE_X       = _ngl.CAP_MAX_COMPUTE_GROUP_SIZE_X
CAP_MAX_COMPUTE_GROUP_SIZE_Y       = _ngl.CAP_MAX_COMPUTE_GROUP_SIZE_Y
CAP_MAX_COMPUTE_GROUP_SIZE_Z       = _ngl.CAP_MAX_COMPUTE_GROUP_SIZE_Z
CAP_MAX_COMPUTE_SHARED_MEMORY_SIZE = _ngl.CAP_MAX_COMPUTE_SHARED_MEMORY_SIZE
CAP_MAX_SAMPLES                    = _ngl.CAP_MAX_SAMPLES
CAP_MAX_TEXTURE_DIMENSION_1D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_1D
CAP_MAX_TEXTURE_DIMENSION_2D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_2D
CAP_MAX_TEXTURE_DIMENSION_3D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_3D
CAP_MAX_TEXTURE_DIMENSION_CUBE     = _ngl.CAP_MAX_TEXTURE_DIMENSION_CUBE
CAP_NPOT_TEXTURE                   = _ngl.CAP_NPOT_TEXTURE
CAP_SHADER_TEXTURE_LOD             = _ngl.CAP_SHADER_TEXTURE_LOD
CAP_TEXTURE_3D                     = _ngl.CAP_TEXTURE_3D
CAP_TEXTURE_CUBE                   = _ngl.CAP_TEXTURE_CUBE
CAP_UINT_UNIFORMS                  = _ngl.CAP_UINT_UNIFORMS

LOG_VERBOSE = _ngl.LOG_VERBOSE
LOG_DEBUG   = _ngl.LOG_DEBUG
LOG_INFO    = _ngl.LOG_INFO
LOG_WARNING = _ngl.LOG_WARNING
LOG_ERROR   = _ngl.LOG_ERROR
LOG_QUIET   = _ngl.LOG_QUIET
# fmt: on


class Node(_Node):
    def _arg_setter(self, cython_setter, param_name, arg):
        if isinstance(arg, Node):
            return self._param_set_node(param_name, arg)
        return cython_setter(param_name, arg)

    def _args_setter(self, cython_setter, param_name, *args):
        if isinstance(args[0], Node):
            return self._param_set_node(param_name, args[0])
        return cython_setter(param_name, args)

    def _add_nodes(self, param_name, *nodes):
        if hasattr(nodes[0], "__iter__"):
            raise Exception(f"add_{param_name}() takes elements as positional arguments, not list")
        return self._param_add_nodes(param_name, len(nodes), nodes)

    def _add_f64s(self, param_name, *f64s):
        if hasattr(f64s[0], "__iter__"):
            raise Exception(f"add_{param_name}() takes elements as positional arguments, not list")
        return self._param_add_f64s(param_name, len(f64s), f64s)

    def _update_dict(self, param_name, arg, **kwargs):
        data_dict = {}
        if arg is not None:
            if not isinstance(arg, dict):
                raise TypeError(f"{param_name} must be of type dict")
            data_dict.update(arg)
        data_dict.update(**kwargs)
        for key, val in data_dict.items():
            if not isinstance(key, str) or (val is not None and not isinstance(val, Node)):
                raise TypeError(f"update_{param_name}() takes a dictionary of <string, node>")
            ret = self._param_set_dict(param_name, key, val)
            if ret < 0:
                return ret
        return 0

    def _set_rational(self, param_name, ratio):
        return self._param_set_rational(param_name, ratio[0], ratio[1])
