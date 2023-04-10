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
from enum import IntEnum
from typing import Any, Mapping, Optional, Sequence, Tuple, Union

if platform.system() == "Windows":
    ngl_dll_dirs = os.getenv("NGL_DLL_DIRS")
    if ngl_dll_dirs:
        dll_dirs = ngl_dll_dirs.split(os.pathsep)
        for dll_dir in dll_dirs:
            if os.path.isdir(dll_dir):
                os.add_dll_directory(dll_dir)


import _pynopegl as _ngl
from _pynopegl import _Node

# fmt: off
ConfigGL          = _ngl.ConfigGL
get_livectls      = _ngl.get_livectls


class Platform(IntEnum):
    AUTO    = _ngl.PLATFORM_AUTO
    XLIB    = _ngl.PLATFORM_XLIB
    ANDROID = _ngl.PLATFORM_ANDROID
    MACOS   = _ngl.PLATFORM_MACOS
    IOS     = _ngl.PLATFORM_IOS
    WINDOWS = _ngl.PLATFORM_WINDOWS
    WAYLAND = _ngl.PLATFORM_WAYLAND


class Backend(IntEnum):
    AUTO     = _ngl.BACKEND_AUTO
    OPENGL   = _ngl.BACKEND_OPENGL
    OPENGLES = _ngl.BACKEND_OPENGLES
    VULKAN   = _ngl.BACKEND_VULKAN


class Cap(IntEnum):
    BLOCK                          = _ngl.CAP_BLOCK
    COMPUTE                        = _ngl.CAP_COMPUTE
    DEPTH_STENCIL_RESOLVE          = _ngl.CAP_DEPTH_STENCIL_RESOLVE
    INSTANCED_DRAW                 = _ngl.CAP_INSTANCED_DRAW
    MAX_COLOR_ATTACHMENTS          = _ngl.CAP_MAX_COLOR_ATTACHMENTS
    MAX_COMPUTE_GROUP_COUNT_X      = _ngl.CAP_MAX_COMPUTE_GROUP_COUNT_X
    MAX_COMPUTE_GROUP_COUNT_Y      = _ngl.CAP_MAX_COMPUTE_GROUP_COUNT_Y
    MAX_COMPUTE_GROUP_COUNT_Z      = _ngl.CAP_MAX_COMPUTE_GROUP_COUNT_Z
    MAX_COMPUTE_GROUP_INVOCATIONS  = _ngl.CAP_MAX_COMPUTE_GROUP_INVOCATIONS
    MAX_COMPUTE_GROUP_SIZE_X       = _ngl.CAP_MAX_COMPUTE_GROUP_SIZE_X
    MAX_COMPUTE_GROUP_SIZE_Y       = _ngl.CAP_MAX_COMPUTE_GROUP_SIZE_Y
    MAX_COMPUTE_GROUP_SIZE_Z       = _ngl.CAP_MAX_COMPUTE_GROUP_SIZE_Z
    MAX_COMPUTE_SHARED_MEMORY_SIZE = _ngl.CAP_MAX_COMPUTE_SHARED_MEMORY_SIZE
    MAX_SAMPLES                    = _ngl.CAP_MAX_SAMPLES
    MAX_TEXTURE_DIMENSION_1D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_1D
    MAX_TEXTURE_DIMENSION_2D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_2D
    MAX_TEXTURE_DIMENSION_3D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_3D
    MAX_TEXTURE_DIMENSION_CUBE     = _ngl.CAP_MAX_TEXTURE_DIMENSION_CUBE
    NPOT_TEXTURE                   = _ngl.CAP_NPOT_TEXTURE
    SHADER_TEXTURE_LOD             = _ngl.CAP_SHADER_TEXTURE_LOD
    TEXTURE_3D                     = _ngl.CAP_TEXTURE_3D
    TEXTURE_CUBE                   = _ngl.CAP_TEXTURE_CUBE
    UINT_UNIFORMS                  = _ngl.CAP_UINT_UNIFORMS


class Log(IntEnum):
    VERBOSE = _ngl.LOG_VERBOSE
    DEBUG   = _ngl.LOG_DEBUG
    INFO    = _ngl.LOG_INFO
    WARNING = _ngl.LOG_WARNING
    ERROR   = _ngl.LOG_ERROR
    QUIET   = _ngl.LOG_QUIET
# fmt: on


def log_set_min_level(level: Log):
    return _ngl.log_set_min_level(level.value)


class Config(_ngl.Config):
    def __init__(
        self,
        platform: Platform = Platform.AUTO,
        backend: Backend = Backend.AUTO,
        backend_config: Optional[ConfigGL] = None,
        display: int = 0,
        window: int = 0,
        swap_interval: int = -1,
        offscreen: bool = False,
        width: int = 0,
        height: int = 0,
        viewport: Tuple[int, int, int, int] = (0, 0, 0, 0),
        samples: int = 0,
        set_surface_pts: bool = False,
        clear_color: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0),
        capture_buffer: Optional[bytearray] = None,
        # capture_buffer_type: int = 0,
        hud: bool = False,
        hud_measure_window: int = 0,
        hud_refresh_rate: Tuple[int, int] = (0, 0),
        hud_export_filename: Optional[str] = None,
        hud_scale: int = 0,
    ):
        self.capture_buffer = capture_buffer
        super().__init__(
            platform,
            backend,
            backend_config,
            display,
            window,
            swap_interval,
            offscreen,
            width,
            height,
            viewport,
            samples,
            set_surface_pts,
            clear_color,
            capture_buffer,
            0,
            hud,
            hud_measure_window,
            hud_refresh_rate,
            hud_export_filename,
            hud_scale,
        )


def _pythonize_backends(backends: Mapping[str, Any]) -> Mapping[Backend, Any]:
    """Replace key string identifiers with their corresponding enum (Backend and Cap)"""
    ret = {}
    for backend_str, backend_data in backends.items():
        backend_id = Backend[backend_str.upper()]
        new_caps = {}
        for cap_str, cap_data in backend_data["caps"].items():
            cap_id = Cap[cap_str.upper()]
            new_caps[cap_id] = cap_data
        backend_data["caps"] = new_caps
        ret[backend_id] = backend_data
    return ret


def get_backends(config: Optional[Config] = None) -> Mapping[Backend, Any]:
    return _pythonize_backends(_ngl.probe_backends(_ngl.PROBE_MODE_FULL, config))


def probe_backends(config: Optional[Config] = None) -> Mapping[Backend, Any]:
    return _pythonize_backends(_ngl.probe_backends(_ngl.PROBE_MODE_NO_GRAPHICS, config))


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


class Context(_ngl.Context):
    def configure(self, config: Config) -> int:
        return super().configure(config)

    def resize(
        self,
        width: int,
        height: int,
        viewport: Optional[Tuple[int, int, int, int]] = None,
    ) -> int:
        return super().resize(width, height, viewport)

    def set_capture_buffer(self, capture_buffer: Optional[bytearray]) -> int:
        return super().set_capture_buffer(capture_buffer)

    def set_scene(self, scene: Optional[Node]) -> int:
        return super().set_scene(scene)

    def set_scene_from_string(self, s: str) -> int:
        return super().set_scene_from_string(s)

    def draw(self, t: float) -> int:
        return super().draw(t)

    def dot(self, t: float) -> Optional[str]:
        return super().dot(t)

    def gl_wrap_framebuffer(self, framebuffer: int) -> int:
        return super().gl_wrap_framebuffer(framebuffer)


def easing_evaluate(
    name: str,
    t: float,
    args: Optional[Sequence[float]] = None,
    offsets: Optional[Tuple[float, float]] = None,
) -> float:
    return _ngl.animate(name, t, args, offsets, _ngl.ANIM_EVALUATE)


def easing_derivate(
    name: str,
    t: float,
    args: Optional[Sequence[float]] = None,
    offsets: Optional[Tuple[float, float]] = None,
) -> float:
    return _ngl.animate(name, t, args, offsets, _ngl.ANIM_DERIVATE)


def easing_solve(
    name: str,
    v: float,
    args: Optional[Sequence[float]] = None,
    offsets: Optional[Tuple[float, float]] = None,
) -> float:
    return _ngl.animate(name, v, args, offsets, _ngl.ANIM_SOLVE)
