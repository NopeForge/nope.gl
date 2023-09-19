#
# Copyright 2017-2022 GoPro Inc.
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

import os
import os.path as op
import subprocess
from typing import Callable, Optional

from pynopegl_utils.com import query_scene
from pynopegl_utils.export import ExportWorker
from pynopegl_utils.misc import SceneCfg, SceneInfo, get_backend, get_nopegl_tempdir, get_viewport
from PySide6 import QtCore, QtGui

import pynopegl as ngl


class Exporter(QtCore.QThread):
    progressed = QtCore.Signal(int)
    failed = QtCore.Signal(str)
    export_finished = QtCore.Signal()

    def __init__(
        self,
        get_scene_info: Callable[..., Optional[SceneInfo]],
        filename,
        width,
        height,
        extra_enc_args=None,
    ):
        super().__init__()
        self._get_scene_info = get_scene_info
        self._filename = filename
        self._width = width
        self._height = height
        self._extra_enc_args = extra_enc_args if extra_enc_args is not None else []
        self._cancelled = False

    def run(self):
        filename, width, height = self._filename, self._width, self._height

        try:
            scene_info = self._get_scene_info()
            if not scene_info:
                self.failed.emit("You didn't select any scene to export.")
                return

            if filename.endswith("gif"):
                palette_filename = op.join(get_nopegl_tempdir(), "palette.png")
                pass1_args = ["-vf", "palettegen"]
                pass2_args = self._extra_enc_args + [
                    # fmt: off
                    "-i", palette_filename,
                    "-lavfi", "paletteuse",
                    # fmt: on
                ]
                pass1 = self._export(scene_info, palette_filename, width, height, pass1_args)
                for progress in pass1:
                    self.progressed.emit(progress)
                    if self._cancelled:
                        break
                export = self._export(scene_info, filename, width, height, pass2_args)
            else:
                export = self._export(scene_info, filename, width, height, self._extra_enc_args)
            for progress in export:
                self.progressed.emit(progress)
                if self._cancelled:
                    break
            self.export_finished.emit()
        except Exception:
            self.failed.emit("Something went wrong while trying to encode, check encoding parameters")

    def _export(self, scene_info: SceneInfo, filename, width, height, extra_enc_args=None):
        fd_r, fd_w = os.pipe()

        scene = scene_info.scene
        fps = scene.framerate
        duration = scene.duration
        samples = scene_info.samples

        cmd = [
            # fmt: off
            "ffmpeg", "-r", "%d/%d" % fps,
            "-nostats", "-nostdin",
            "-f", "rawvideo",
            "-video_size", "%dx%d" % (width, height),
            "-pixel_format", "rgba",
            "-i", "pipe:%d" % fd_r
            # fmt: on
        ]
        if extra_enc_args:
            cmd += extra_enc_args
        cmd += ["-y", filename]

        reader = subprocess.Popen(cmd, pass_fds=(fd_r,))
        os.close(fd_r)

        capture_buffer = bytearray(width * height * 4)

        ctx = ngl.Context()
        ctx.configure(
            ngl.Config(
                platform=ngl.Platform.AUTO,
                backend=get_backend(scene_info.backend),
                offscreen=True,
                width=width,
                height=height,
                viewport=get_viewport(width, height, scene.aspect_ratio),
                samples=samples,
                clear_color=scene_info.clear_color,
                capture_buffer=capture_buffer,
            )
        )
        ctx.set_scene(scene)

        # Draw every frame
        nb_frame = int(duration * fps[0] / fps[1])
        for i in range(nb_frame):
            time = i * fps[1] / float(fps[0])
            ctx.draw(time)
            os.write(fd_w, capture_buffer)
            yield i * 100 / nb_frame
        yield 100

        os.close(fd_w)
        reader.wait()

    def cancel(self):
        self._cancelled = True


def test_export():
    import sys

    def _get_scene(**cfg_overrides):
        from pynopegl_utils.examples.misc import triangle

        cfg = SceneCfg(duration=5, **cfg_overrides)
        query_info = query_scene("pynopegl_utils.examples", triangle, cfg)
        if query_info.error is not None:
            print(query_info.error)
            return None
        return query_info.ret

    def print_progress(progress):
        sys.stdout.write("\r%d%%" % progress)
        sys.stdout.flush()
        if progress == 100:
            sys.stdout.write("\n")

    if len(sys.argv) != 2:
        print("Usage: %s <outfile>" % sys.argv[0])
        sys.exit(0)

    filename = sys.argv[1]
    app = QtGui.QGuiApplication(sys.argv)

    exporter = Exporter(_get_scene, filename, 320, 240)
    exporter.progressed.connect(print_progress)
    exporter.start()
    exporter.wait()

    app.exit()


if __name__ == "__main__":
    test_export()
