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

import inspect
import json
import os
import os.path as op
import pkgutil
import platform
import random
import subprocess
import tempfile
from collections import namedtuple
from dataclasses import asdict, dataclass, field
from fractions import Fraction
from functools import partialmethod, wraps
from typing import List, Tuple

import pynopegl as ngl


def scene(**controls):
    def real_decorator(scene_func):
        @wraps(scene_func)
        def func_wrapper(idict=None, **extra_args):
            if idict is None:
                idict = {}
            elif isinstance(idict, SceneCfg):
                idict = idict.as_dict()
            scene_cfg = SceneCfg(**idict)
            scene = scene_func(scene_cfg, **extra_args)
            odict = scene_cfg.as_dict()
            odict["scene"] = scene
            return odict

        # Construct widgets specs
        widgets_specs = []
        func_specs = inspect.getfullargspec(scene_func)
        if func_specs.defaults:
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


@dataclass(frozen=True)
class MediaInfo:
    filename: str
    width: int
    height: int
    pix_fmt: str
    duration: float
    time_base: Fraction
    avg_frame_rate: Fraction

    @classmethod
    def from_filename(cls, filename: str):
        cmd = ["ffprobe", "-show_format", "-show_streams", "-of", "json", "-i", filename]
        out = subprocess.run(cmd, capture_output=True).stdout
        data = json.loads(out)

        vst = next(st for st in data["streams"] if st["codec_type"] == "video")
        time_base = Fraction(vst["time_base"])
        if "duration_ts" in vst:
            duration = vst["duration_ts"] * time_base
        elif "duration" in data["format"]:
            duration = Fraction(data["format"]["duration"])
        else:
            duration = Fraction(1)
        duration = float(duration)
        avg_frame_rate = Fraction(vst["avg_frame_rate"])

        return cls(
            filename=filename,
            width=vst["width"],
            height=vst["height"],
            pix_fmt=vst["pix_fmt"],
            duration=duration,
            time_base=time_base,
            avg_frame_rate=avg_frame_rate,
        )


def get_nopegl_tempdir():
    tmpdir = op.join(tempfile.gettempdir(), "nopegl")
    os.makedirs(tmpdir, exist_ok=True)
    return tmpdir


def _get_default_medias():
    return [
        MediaInfo.from_filename(op.join(op.dirname(__file__), "assets", name))
        for name in (
            "mire-hevc.mp4",
            "cat.mp4",
            "OpenMoji-1F342-Fallen_Leaf.png",
            "OpenMoji-1F439-Hamster.png",
            "Unsplash-Michael_Anfang-Rooster-cropped.jpg",
            "Unsplash-Romane_Gautun-Red_Panda-cropped.jpg",
        )
    ]


@dataclass
class SceneCfg:
    aspect_ratio: Tuple[int, int] = (16, 9)
    duration: float = 30.0
    framerate: Tuple[int, int] = (60, 1)
    backend: str = "opengl"
    samples: int = 0
    system: str = platform.system()
    files: List[str] = field(default_factory=list)
    medias: List[MediaInfo] = field(default_factory=_get_default_medias)
    autofill_medias: bool = True
    clear_color: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    shaders_module: str = "pynopegl_utils.examples.shaders"

    def __post_init__(self):
        # Predictible random number generator
        self.rng = random.Random(0)

    @property
    def aspect_ratio_float(self):
        return self.aspect_ratio[0] / self.aspect_ratio[1]

    def as_dict(self):
        return asdict(self)

    def _get_shader(self, stype, name):
        data = pkgutil.get_data(self.shaders_module, f"{name}.{stype}")
        if data is None:
            raise FileNotFoundError(f"Unable to find shader {name}")
        return data.decode()

    get_frag = partialmethod(_get_shader, "frag")
    get_vert = partialmethod(_get_shader, "vert")
    get_comp = partialmethod(_get_shader, "comp")


def get_viewport(width, height, aspect_ratio):
    view_width = width
    view_height = width * aspect_ratio[1] // aspect_ratio[0]
    if view_height > height:
        view_height = height
        view_width = height * aspect_ratio[0] // aspect_ratio[1]
    view_x = (width - view_width) // 2
    view_y = (height - view_height) // 2
    return (view_x, view_y, view_width, view_height)


def get_backend(backend):
    return ngl.Backend[backend.upper()]
