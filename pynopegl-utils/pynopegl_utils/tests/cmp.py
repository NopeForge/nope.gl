#
# Copyright 2020-2022 GoPro Inc.
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

import difflib
import os
import os.path as op
from typing import Any, Callable, Generator, Sequence, Tuple

from pynopegl_utils.misc import SceneCfg, get_backend, get_nopegl_tempdir

import pynopegl as ngl


class CompareBase:
    @staticmethod
    def serialize(data: Any) -> str:
        return data

    @staticmethod
    def deserialize(data: str) -> Any:
        return data

    def get_out_data(self, dump=False, func_name=None):
        raise NotImplementedError

    def compare_data(self, test_name: str, ref_data: Any, out_data: Any) -> Sequence[str]:
        ref_data = self.serialize(ref_data)
        out_data = self.serialize(out_data)

        err = []
        if ref_data != out_data:
            ref_data = ref_data.splitlines(True)
            out_data = out_data.splitlines(True)
            diff = "".join(
                difflib.unified_diff(ref_data, out_data, fromfile=test_name + "-ref", tofile=test_name + "-out", n=10)
            )
            err.append(f"{test_name} fail:\n{diff}")
        return err

    @staticmethod
    def dump_image(img, dump_index: int, func_name=None):
        test_tmpdir = op.join(get_nopegl_tempdir(), "tests")
        backend = os.environ.get("BACKEND")
        if backend:
            test_tmpdir = op.join(test_tmpdir, backend)
        os.makedirs(test_tmpdir, exist_ok=True)
        filename = op.join(test_tmpdir, f"{func_name}_{dump_index:03}.png")
        print(f"Dumping output image to {filename}")
        img.save(filename)
        dump_index += 1


class CompareSceneBase(CompareBase):
    def __init__(
        self,
        scene_func: Callable[..., dict],
        width: int = 1280,
        height: int = 800,
        keyframes: int | Sequence[float] = 1,  # either a number of keyframes or a sequence of absolute times
        keyframes_callback=None,
        clear_color: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0),
        exercise_serialization: bool = True,
        exercise_dot: bool = True,
        samples: int = 0,
        **scene_kwargs,
    ):
        self._width = width
        self._height = height
        self._keyframes = keyframes
        self._keyframes_callback = keyframes_callback
        self._clear_color = clear_color
        self._scene_func = scene_func
        self._scene_kwargs = scene_kwargs
        self._exercise_serialization = exercise_serialization
        self._exercise_dot = exercise_dot
        self._samples = samples
        self._hud = False
        self._hud_export_filename = None

    def render_frames(self) -> Generator[Tuple[int, int, bytearray], None, None]:
        cfg = SceneCfg()

        backend = os.environ.get("BACKEND")
        if backend:
            cfg.backend = backend

        ret = self._scene_func(cfg, **self._scene_kwargs)
        width, height = self._width, self._height
        scene = ret["scene"]
        duration = scene.duration

        capture_buffer = bytearray(width * height * 4)
        ctx = ngl.Context()
        ret = ctx.configure(
            ngl.Config(
                offscreen=True,
                width=width,
                height=height,
                backend=get_backend(backend) if backend else ngl.Backend.AUTO,
                samples=self._samples,
                clear_color=self._clear_color,
                capture_buffer=capture_buffer,
                hud=self._hud,
                hud_export_filename=self._hud_export_filename,
            )
        )
        assert ret == 0

        if isinstance(self._keyframes, int):
            timescale = duration / self._keyframes
            keyframes = [t_id * timescale for t_id in range(self._keyframes)]
        else:
            keyframes = self._keyframes
        assert all(t <= duration for t in keyframes)

        if self._exercise_dot:
            assert scene.dot()

        if self._exercise_serialization:
            scene = ngl.Scene.from_string(scene.serialize())

        assert ctx.set_scene(scene) == 0

        for t_id, t in enumerate(keyframes):
            if self._keyframes_callback:
                self._keyframes_callback(t_id)
            ctx.draw(t)

            yield (width, height, capture_buffer)

            if not self._exercise_serialization and self._exercise_dot:
                scene.dot()


def get_test_decorator(cls):
    def test_func(*args, **kwargs):
        def test_decorator(user_func):
            # Inject a tester for ngl-test
            user_func.tester = cls(user_func, *args, **kwargs)
            return user_func

        return test_decorator

    return test_func
