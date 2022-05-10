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
from functools import wraps

import pynodegl as ngl


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


class Media:
    def __init__(self, filename):
        self._filename = filename
        self._set_media_dimensions()

    def _set_media_dimensions(self):
        data = subprocess.check_output(
            [
                # fmt: off
                "ffprobe",
                "-v", "0",
                "-select_streams", "v:0",
                "-of", "json",
                "-show_streams", "-show_format",
                self._filename,
                # fmt: on
            ]
        )
        data = json.loads(data)
        st = data["streams"][0]
        self._dimensions = (st["width"], st["height"])
        self._duration = float(data["format"].get("duration", 1))
        self._framerate = tuple(int(x) for x in st["avg_frame_rate"].split("/"))

    @property
    def filename(self):
        return self._filename

    @property
    def width(self):
        return self._dimensions[0]

    @property
    def height(self):
        return self._dimensions[1]

    @property
    def dimensions(self):
        return self._dimensions

    @property
    def duration(self):
        return self._duration

    @property
    def framerate(self):
        return self._framerate

    @property
    def framerate_float(self):
        return self._framerate[0] / float(self._framerate[1])


def get_nodegl_tempdir():
    tmpdir = op.join(tempfile.gettempdir(), "nodegl")
    os.makedirs(tmpdir, exist_ok=True)
    return tmpdir


_MEDIA_PATHS = [
    op.join(op.dirname(__file__), "assets", name)
    for name in (
        "mire-hevc.mp4",
        "mire-h264.mp4",
        "OpenMoji-1F342-Fallen_Leaf.png",
        "OpenMoji-1F439-Hamster.png",
        "Unsplash-Michael_Anfang-Rooster-cropped.jpg",
        "Unsplash-Romane_Gautun-Red_Panda-cropped.jpg",
    )
]


class SceneCfg:

    _DEFAULT_FIELDS = {
        "aspect_ratio": (16, 9),
        "duration": 30.0,
        "framerate": (60, 1),
        "backend": "opengl",
        "samples": 0,
        "system": platform.system(),
        "files": [],
        "medias": None,
        "clear_color": (0.0, 0.0, 0.0, 1.0),
    }

    def __init__(self, **kwargs):
        for field, def_val in self._DEFAULT_FIELDS.items():
            val = kwargs.get(field, def_val)
            setattr(self, field, val)

        if self.medias is None:
            self.medias = [Media(m) for m in _MEDIA_PATHS]

        # Predictible random number generator
        self.rng = random.Random(0)

    @property
    def aspect_ratio_float(self):
        return self.aspect_ratio[0] / float(self.aspect_ratio[1])

    def as_dict(self):
        odict = {}
        for field in self._DEFAULT_FIELDS.keys():
            odict[field] = getattr(self, field)
        return odict

    def _get_shader(self, name, stype, module):
        if module is None:
            module = "pynodegl_utils.examples.shaders"
        return pkgutil.get_data(module, f"{name}.{stype}")

    def get_frag(self, name, module=None):
        return self._get_shader(name, "frag", module)

    def get_vert(self, name, module=None):
        return self._get_shader(name, "vert", module)

    def get_comp(self, name, module=None):
        return self._get_shader(name, "comp", module)


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
    backend_map = {
        "opengl": ngl.BACKEND_OPENGL,
        "opengles": ngl.BACKEND_OPENGLES,
        "vulkan": ngl.BACKEND_VULKAN,
    }
    return backend_map[backend]
