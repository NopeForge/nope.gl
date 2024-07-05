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
import inspect
import os
import platform
import random
from collections import namedtuple
from dataclasses import asdict, dataclass, field
from enum import IntEnum
from functools import lru_cache, wraps
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence, Tuple, Union

from packaging.specifiers import SpecifierSet
from packaging.version import Version

if platform.system() == "Windows":
    ngl_dll_dirs = os.getenv("NGL_DLL_DIRS")
    if ngl_dll_dirs:
        dll_dirs = ngl_dll_dirs.split(os.pathsep)
        for dll_dir in dll_dirs:
            if os.path.isdir(dll_dir):
                os.add_dll_directory(dll_dir)


import _pynopegl as _ngl
from _pynopegl import _Node

__version__ = _ngl.__version__

# fmt: off
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
    COMPUTE                        = _ngl.CAP_COMPUTE
    DEPTH_STENCIL_RESOLVE          = _ngl.CAP_DEPTH_STENCIL_RESOLVE
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
    MAX_TEXTURE_ARRAY_LAYERS       = _ngl.CAP_MAX_TEXTURE_ARRAY_LAYERS
    MAX_TEXTURE_DIMENSION_1D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_1D
    MAX_TEXTURE_DIMENSION_2D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_2D
    MAX_TEXTURE_DIMENSION_3D       = _ngl.CAP_MAX_TEXTURE_DIMENSION_3D
    MAX_TEXTURE_DIMENSION_CUBE     = _ngl.CAP_MAX_TEXTURE_DIMENSION_CUBE
    TEXT_LIBRARIES                 = _ngl.CAP_TEXT_LIBRARIES


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


class ConfigGL(_ngl.ConfigGL):
    def __init__(self, external: bool = False, external_framebuffer: int = 0):
        super().__init__(external, external_framebuffer)


class Config(_ngl.Config):
    def __init__(
        self,
        platform: Platform = Platform.AUTO,
        backend: Backend = Backend.AUTO,
        backend_config: Optional[ConfigGL] = None,
        display: int = 0,
        window: int = 0,
        swap_interval: int = -1,
        disable_depth: bool = False,
        offscreen: bool = False,
        width: int = 0,
        height: int = 0,
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
            disable_depth,
            offscreen,
            width,
            height,
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


def _pythonize_backend(backend: Dict[str, Any]) -> Dict[str, Any]:
    new_caps = {}
    for cap_str, cap_data in backend["caps"].items():
        cap_id = Cap[cap_str.upper()]
        new_caps[cap_id] = cap_data
    backend["id"] = Backend(backend["id"])
    backend["caps"] = new_caps
    return backend


def _pythonize_backends(backends: Mapping[str, Any]) -> Mapping[Backend, Any]:
    """Replace key string identifiers with their corresponding enum (Backend and Cap)"""
    ret = {}
    for backend_str, backend_data in backends.items():
        backend_id = Backend[backend_str.upper()]
        ret[backend_id] = _pythonize_backend(backend_data)
    return ret


@lru_cache
def get_backends(config: Optional[Config] = None) -> Mapping[Backend, Any]:
    return _pythonize_backends(_ngl.probe_backends(_ngl.PROBE_MODE_NO_GRAPHICS, config))


@lru_cache
def probe_backends(config: Optional[Config] = None) -> Mapping[Backend, Any]:
    return _pythonize_backends(_ngl.probe_backends(_ngl.PROBE_MODE_FULL, config))


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
        if not nodes:
            return 0
        if hasattr(nodes[0], "__iter__"):
            raise Exception(f"add_{param_name}() takes elements as positional arguments, not list")
        return self._param_add_nodes(param_name, len(nodes), nodes)

    def _add_f64s(self, param_name, *f64s):
        if not f64s:
            return 0
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


class Scene(_ngl.Scene):
    @classmethod
    def from_params(
        cls,
        root: Node,
        duration: Optional[float] = None,
        framerate: Optional[Tuple[int, int]] = None,
        aspect_ratio: Optional[Tuple[int, int]] = None,
    ) -> "Scene":
        return super().from_params(root, duration, framerate, aspect_ratio)

    @classmethod
    def from_string(cls, s: Union[str, bytes]) -> "Scene":
        return super().from_string(s)

    def serialize(self) -> bytes:
        return super().serialize()

    def dot(self) -> bytes:
        return super().dot()

    def update_filepath(self, index: int, filepath: str):
        super().update_filepath(index, filepath)

    @property
    def duration(self) -> float:
        return super().duration

    @property
    def framerate(self) -> Tuple[int, int]:
        return super().framerate

    @property
    def aspect_ratio(self) -> Tuple[int, int]:
        return super().aspect_ratio

    @property
    def files(self) -> List[str]:
        return [f.decode("utf-8") for f in super().files]


class Context(_ngl.Context):
    def configure(self, config: Config) -> int:
        return super().configure(config)

    def get_backend(self) -> Dict[str, Any]:
        backend = super().get_backend()
        return _pythonize_backend(backend)

    def resize(self, width: int, height: int) -> int:
        return super().resize(width, height)

    @property
    def viewport(self) -> Tuple[int, int, int, int]:
        return super().get_viewport()

    def set_capture_buffer(self, capture_buffer: Optional[bytearray]) -> int:
        return super().set_capture_buffer(capture_buffer)

    def set_scene(self, scene: Optional[Scene]) -> int:
        return super().set_scene(scene)

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


def get_livectls(scene: Node) -> Mapping[str, Mapping[str, Any]]:
    return _ngl.get_livectls(scene)


@dataclass
class SceneCfg:
    aspect_ratio: Tuple[int, int] = (16, 9)
    duration: float = 30.0
    framerate: Tuple[int, int] = (60, 1)
    backend: Backend = Backend.AUTO
    samples: int = 0
    system: str = platform.system()
    clear_color: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    caps: Optional[Mapping[Cap, int]] = None

    def __post_init__(self):
        # Predictible random number generator
        self.rng = random.Random(0)

    @property
    def aspect_ratio_float(self) -> float:
        return self.aspect_ratio[0] / self.aspect_ratio[1]

    def as_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class SceneInfo:
    scene: Scene
    backend: Backend
    samples: int
    clear_color: Tuple[float, float, float, float]


def scene(controls: Optional[Dict[str, Any]] = None, compat_specs: Optional[str] = None):
    def real_decorator(scene_func: Callable[..., Node]) -> Callable[..., SceneInfo]:
        @wraps(scene_func)
        def func_wrapper(scene_cfg: Optional[SceneCfg] = None, **extra_args):
            version = Version(__version__)
            if compat_specs and version not in SpecifierSet(compat_specs):
                raise Exception(
                    f"{scene_func.__name__} needs libnopegl{compat_specs} but libnopegl is currently at version {version}"
                )

            if scene_cfg is None:
                scene_cfg = SceneCfg()

            # Make sure the SceneCfg backend has a concrete value
            if scene_cfg.backend == Backend.AUTO:
                scene_cfg.backend = next(k for k, v in get_backends().items() if v["is_default"])

            # Provide the capabilities of the selected backend
            if scene_cfg.caps is None:
                backends = probe_backends()
                scene_cfg.caps = backends[scene_cfg.backend]["caps"]
            else:
                ref_set = set(cap.value for cap in Cap)
                usr_set = set(scene_cfg.caps.keys())
                if ref_set != usr_set:
                    raise Exception("the specified capabilities set does not match the available capabilities")

            root = scene_func(scene_cfg, **extra_args)
            scene = Scene.from_params(root, scene_cfg.duration, scene_cfg.framerate, scene_cfg.aspect_ratio)
            return SceneInfo(
                scene=scene,
                backend=scene_cfg.backend,
                samples=scene_cfg.samples,
                clear_color=scene_cfg.clear_color,
            )

        # Construct widgets specs
        widgets_specs = []
        func_specs = inspect.getfullargspec(scene_func)
        if controls is not None and func_specs.defaults:
            nb_optionnals = len(func_specs.defaults)
            for i, key in enumerate(func_specs.args[-nb_optionnals:]):
                # Set controller defaults according to the function prototype
                control = controls.get(key)
                if control is not None:
                    default = func_specs.defaults[i]
                    ctl_id = control.__class__.__name__
                    ctl_data = control._asdict()
                    widgets_specs.append((key, default, ctl_id, ctl_data))

        # Transfers the widget specs to the UI.
        # We could use the return value but it's better if the user can still
        # call its decorated scene function transparently inside his own code
        # without getting garbage along the return value.
        func_wrapper.widgets_specs = widgets_specs

        # Flag the scene as a scene function so it's registered in the UI.
        func_wrapper.iam_a_ngl_scene_func = True

        return func_wrapper

    return real_decorator


scene.Range = namedtuple("Range", "range unit_base", defaults=([0, 1], 1))
scene.Vector = namedtuple("Vector", "n minv maxv", defaults=(None, None))
scene.Color = namedtuple("Color", "")
scene.Bool = namedtuple("Bool", "")
scene.File = namedtuple("File", "filter", defaults=("",))
scene.List = namedtuple("List", "choices")
scene.Text = namedtuple("Text", "")
