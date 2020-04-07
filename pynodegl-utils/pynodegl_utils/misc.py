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

import os.path as op
import tempfile
import platform
import math
import inspect
import json
import subprocess
import pynodegl as ngl
from pynodegl_utils import controls


def scene(**widgets_specs):
    def real_decorator(scene_func):
        def func_wrapper(idict=None, **extra_args):
            if idict is None:
                idict = {}
            elif isinstance(idict, SceneCfg):
                idict = idict.as_dict()
            scene_cfg = SceneCfg(**idict)
            scene = scene_func(scene_cfg, **extra_args)
            odict = scene_cfg.as_dict()
            odict['scene'] = scene
            return odict

        final_specs = []

        # Construct a arg -> default dict
        func_specs = inspect.getargspec(scene_func)
        if func_specs.defaults:
            nb_optionnals = len(func_specs.defaults)
            for i, key in enumerate(func_specs.args[-nb_optionnals:]):
                # Set controller defaults according to the function prototype
                if key in widgets_specs:
                    widgets_specs[key].set_default(func_specs.defaults[i])

        # Transfers the widgets controls to the UI.
        # We could use the return value but it's better if the user can still
        # call its decorated scene function transparently inside his own code
        # without getting garbage along the return value.
        func_wrapper.widgets_specs = list(widgets_specs.items())

        # Flag the scene as a scene function so it's registered in the UI.
        func_wrapper.iam_a_ngl_scene_func = True

        # Inherit doc from original function
        func_wrapper.__doc__ = scene_func.__doc__

        return func_wrapper

    return real_decorator


scene.Range = controls.Range
scene.Vector = controls.Vector
scene.Color = controls.Color
scene.Bool = controls.Bool
scene.File = controls.File
scene.List = controls.List
scene.Text = controls.Text


class Media:

    def __init__(self, filename):
        self._filename = filename
        self._set_media_dimensions()

    def _set_media_dimensions(self):
        data = subprocess.check_output(['ffprobe', '-v', '0',
                                        '-select_streams', 'v:0',
                                        '-of', 'json',
                                        '-show_streams', '-show_format',
                                        self._filename])
        data = json.loads(data)
        st = data['streams'][0]
        self._dimensions = (st['width'], st['height'])
        self._duration = float(data['format'].get('duration', 1))
        self._framerate = tuple(int(x) for x in st['avg_frame_rate'].split('/'))

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


class SceneCfg:

    _DEFAULT_MEDIA_FILE = op.join(tempfile.gettempdir(), 'ngl-media.mp4')
    _DEFAULT_FIELDS = {
        'aspect_ratio': (16, 9),
        'duration': 30.0,
        'framerate': (60, 1),
        'backend': 'gl',
        'samples': 0,
        'system': platform.system(),
        'files': [],
        'medias': None,
        'clear_color': (0.0, 0.0, 0.0, 1.0),
    }

    def __init__(self, **kwargs):
        for field, def_val in self._DEFAULT_FIELDS.items():
            val = kwargs.get(field, def_val)
            setattr(self, field, val)

        if self.medias is None:
            media_file = self._DEFAULT_MEDIA_FILE
            if not op.exists(self._DEFAULT_MEDIA_FILE):
                ret = subprocess.call(['ffmpeg', '-nostdin', '-nostats', '-f', 'lavfi', '-i',
                                       'testsrc2=d=%d:r=%d/%d' % (int(math.ceil(self.duration)),
                                                                  self.framerate[0], self.framerate[1]),
                                       media_file])
                if ret:
                    raise Exception('Unable to create a media file using ffmpeg (ret=%d)' % ret)
            self.medias = [Media(media_file)]

    @property
    def aspect_ratio_float(self):
        return self.aspect_ratio[0] / float(self.aspect_ratio[1])

    def as_dict(self):
        odict = {}
        for field in self._DEFAULT_FIELDS.keys():
            odict[field] = getattr(self, field)
        return odict

    def _get_shader(self, name, stype, shader_path):
        filename = '%s.%s' % (name, stype)
        if shader_path is None:
            shader_path = op.join(op.dirname(__file__), 'examples', 'shaders')
        return open(op.join(shader_path, filename)).read()

    def get_frag(self, name, shader_path=None):
        return self._get_shader(name, 'frag', shader_path)

    def get_vert(self, name, shader_path=None):
        return self._get_shader(name, 'vert', shader_path)

    def get_comp(self, name, shader_path=None):
        return self._get_shader(name, 'comp', shader_path)


def get_viewport(width, height, aspect_ratio):
    view_width = width
    view_height = width * aspect_ratio[1] / aspect_ratio[0]
    if view_height > height:
        view_height = height
        view_width = height * aspect_ratio[0] / aspect_ratio[1]
    view_x = (width - view_width) // 2
    view_y = (height - view_height) // 2
    return (view_x, view_y, view_width, view_height)


def get_backend(backend):
    backend_map = {
        'gl': ngl.BACKEND_OPENGL,
        'gles': ngl.BACKEND_OPENGLES,
    }
    return backend_map[backend]
