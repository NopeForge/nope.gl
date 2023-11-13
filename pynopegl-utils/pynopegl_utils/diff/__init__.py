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

import os.path as op
import pkgutil
import sys
from pathlib import Path
from typing import Optional

from pynopegl_utils import qml
from pynopegl_utils.misc import MediaInfo
from PySide6.QtCore import QObject, QUrl, Slot

import pynopegl as ngl


class _Diff:
    def __init__(self, qml_engine, ngl_widget, args):
        self._livectls = {}
        self._reframing_scale = 1
        self._reframing_off = (0, 0)

        ngl_widget.livectls_changed.connect(self._livectls_changed)
        self._ngl_widget = ngl_widget

        app_window = qml_engine.rootObjects()[0]
        app_window.diffModeToggled.connect(self._diff_mode_toggled)
        app_window.verticalSplitChanged.connect(self._vertical_split_changed)
        app_window.thresholdMoved.connect(self._threshold_moved)
        app_window.showCompChanged.connect(self._show_comp_changed)
        app_window.premultipliedChanged.connect(self._premultiplied_changed)
        app_window.setFile.connect(self._set_file_refresh)
        self._app_window = app_window

        player = app_window.findChild(QObject, "player")
        player.timeChanged.connect(ngl_widget.set_time)
        player.mouseDown.connect(self._mouse_down)
        player.zoom.connect(self._zoom)
        player.pan.connect(self._pan)
        self._player = player

        self._media0: Optional[MediaInfo] = None
        self._media1: Optional[MediaInfo] = None

        if len(args) > 1:
            self._set_file(0, args[1])
            if len(args) > 2:
                self._set_file(1, args[2])
        self._refresh_scene()

    @Slot(int, str)
    def _set_file_refresh(self, index: int, fname: str):
        self._set_file(index, fname)
        self._refresh_scene()

    def _set_file(self, index: int, fname: str):
        if not fname:
            media = None
            dirname = None
        else:
            path = qml.uri_to_path(fname)
            media = MediaInfo.from_filename(path)
            dirname = Path(path).parent.as_posix()
            dirname = QUrl.fromLocalFile(dirname).url()

        medias = [self._media0, self._media1]
        medias[index] = media
        self._media0, self._media1 = medias

        if dirname:
            self._app_window.set_media_directory(index, dirname)
            if medias[index ^ 1] is None:
                self._app_window.set_media_directory(index ^ 1, dirname)

    def _refresh_scene(self):
        media0 = self._media0
        media1 = self._media1

        self._livectls = {}
        self._reframing_scale = 1
        self._reframing_off = (0, 0)

        if not media0 or not media1:
            scene = self._get_wait_scene()
        else:
            scene = self._get_scene(
                media0,
                media1,
                diff_mode=self._app_window.get_diff_mode(),
                vertical_split=self._app_window.get_vertical_split(),
                threshold=self._app_window.get_threshold(),
                show_r=self._app_window.get_show_r(),
                show_g=self._app_window.get_show_g(),
                show_b=self._app_window.get_show_b(),
                show_a=self._app_window.get_show_a(),
                premultiplied=self._app_window.get_premultiplied(),
            )

        self._ngl_widget.set_scene(scene)

        for i, media in enumerate((media0, media1)):
            if media is None:
                self._app_window.setProperty(f"filename{i}", "")
                continue
            self._app_window.setProperty(f"filename{i}", op.basename(media.filename))
            self._app_window.setProperty(f"width{i}", media.width)
            self._app_window.setProperty(f"height{i}", media.height)
            self._app_window.setProperty(f"pix_fmt{i}", media.pix_fmt)
            self._app_window.setProperty(f"duration{i}", media.duration)
            self._app_window.setProperty(f"avg_frame_rate{i}", float(media.avg_frame_rate))

        self._player.setProperty("duration", scene.duration)
        self._player.setProperty("framerate", list(scene.framerate))
        self._player.setProperty("aspect", list(scene.aspect_ratio))

    @Slot(object)
    def _livectls_changed(self, data):
        self._livectls = {v["label"]: v for v in data}

    def _update_livectl(self, key, val):
        if not self._media0 or not self._media1:
            return
        data = self._livectls[key]
        data["val"] = val
        self._ngl_widget.livectls_changes[key] = data
        self._ngl_widget.update()

    @Slot(bool)
    def _diff_mode_toggled(self, checked):
        self._update_livectl("diff_mode", checked)

    @Slot(bool)
    def _vertical_split_changed(self, enabled):
        self._update_livectl("vertical_split", enabled)

    @Slot(float)
    def _threshold_moved(self, value):
        self._update_livectl("threshold", value)

    @Slot(bool)
    def _show_comp_changed(self, comp, checked):
        self._update_livectl("show_" + "rgba"[comp], checked)

    @Slot(bool)
    def _premultiplied_changed(self, enabled):
        self._update_livectl("premultiplied", enabled)

    @Slot(float, float)
    def _mouse_down(self, x, y):
        self._update_livectl("split", (x, y))

    def _set_framing(self, s, x, y):
        self._reframing_scale = s
        self._reframing_off = (x, y)
        self._update_livectl("reframing_scale", s)
        self._update_livectl("reframing_off", (x, y))

    _MIN_SCALE, _MAX_SCALE = 1, 64
    _SCALE_STEP = 0.2
    _COMMON_STEP = 15 * 8  # 15° in eigth of degrees, most common mouse wheel stepping

    @staticmethod
    def _clamp(x, a, b):
        return min(max(x, a), b)

    @Slot(float, float, float)
    def _zoom(self, angle_delta, x, y):
        scale = 1 + angle_delta / self._COMMON_STEP * self._SCALE_STEP

        # Get current transform "C"
        cs = self._reframing_scale
        cx, cy = self._reframing_off

        # Clamped scale for the new absolute transform "N"
        ns = self._clamp(cs * scale, self._MIN_SCALE, self._MAX_SCALE)

        # Define the relative scale "M" such that N=M×C
        ms = ns / cs  # effective scale factor after clamping
        mx = x - x * ms
        my = y - y * ms

        # Derivate N translate offsets from the M×C operation and bind them
        # using the finale scale
        nx = self._clamp(ms * cx + mx, 1 - ns, ns - 1)
        ny = self._clamp(ms * cy + my, 1 - ns, ns - 1)

        return self._set_framing(ns, nx, ny)

    @Slot(float, float)
    def _pan(self, vx, vy):
        cs = self._reframing_scale
        cx, cy = self._reframing_off
        nx = self._clamp(cx + vx, 1 - cs, cs - 1)
        ny = self._clamp(cy + vy, 1 - cs, cs - 1)
        return self._set_framing(cs, nx, ny)

    def _get_wait_scene(self):
        ar = (16, 9)
        n = 2 - [self._media0, self._media1].count(None)
        root = ngl.Text(
            text=f"{n}/2 media selected",
            fg_color=(1, 1, 1),
            fg_opacity=1,
            bg_color=(0.15, 0.15, 0.15),
            bg_opacity=1,
            aspect_ratio=ar,
            padding=54,
        )
        return ngl.Scene.from_params(root, aspect_ratio=ar)

    def _get_scene(
        self,
        media0: MediaInfo,
        media1: MediaInfo,
        diff_mode,
        vertical_split,
        threshold,
        show_r,
        show_g,
        show_b,
        show_a,
        premultiplied,
        split=(0.5, 0.5),
    ):
        width = max(media0.width, media1.width)
        height = max(media0.height, media1.height)
        duration = max(media0.duration, media1.duration)
        time_base = media0.time_base if media0.duration >= media1.duration else media1.time_base
        avg_frame_rate = max(media0.avg_frame_rate, media1.avg_frame_rate)

        assert time_base > 0

        vert = pkgutil.get_data("pynopegl_utils.diff.shaders", "diff.vert")
        frag = pkgutil.get_data("pynopegl_utils.diff.shaders", "diff.frag")
        assert vert is not None and frag is not None
        quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
        prog = ngl.Program(vertex=vert.decode(), fragment=frag.decode())
        prog.update_vert_out_vars(
            uv=ngl.IOVec2(),
            tex0_coord=ngl.IOVec2(),
            tex1_coord=ngl.IOVec2(),
        )
        scene = ngl.Render(quad, prog)
        scene.update_frag_resources(
            tex0=ngl.Texture2D(data_src=ngl.Media(media0.filename), min_filter="nearest", mag_filter="nearest"),
            tex1=ngl.Texture2D(data_src=ngl.Media(media1.filename), min_filter="nearest", mag_filter="nearest"),
            split=ngl.UniformVec2(split, live_id="split"),
            diff_mode=ngl.UniformBool(diff_mode, live_id="diff_mode"),
            vertical_split=ngl.UniformBool(vertical_split, live_id="vertical_split"),
            show_r=ngl.UniformBool(show_r, live_id="show_r"),
            show_g=ngl.UniformBool(show_g, live_id="show_g"),
            show_b=ngl.UniformBool(show_b, live_id="show_b"),
            show_a=ngl.UniformBool(show_a, live_id="show_a"),
            premultiplied=ngl.UniformBool(premultiplied, live_id="premultiplied"),
            threshold=ngl.UniformFloat(threshold, live_id="threshold", live_max=0.1),
        )
        scene.update_vert_resources(
            reframing_scale=ngl.UniformFloat(
                self._reframing_scale,
                live_id="reframing_scale",
                live_min=self._MIN_SCALE,
                live_max=self._MAX_SCALE,
            ),
            reframing_off=ngl.UniformVec2(
                self._reframing_off,
                live_id="reframing_off",
                live_min=(-self._MAX_SCALE, -self._MAX_SCALE),
                live_max=(self._MAX_SCALE, self._MAX_SCALE),
            ),
        )
        return ngl.Scene.from_params(
            scene,
            duration=duration,
            framerate=avg_frame_rate.as_integer_ratio(),
            aspect_ratio=(width, height),
        )


def run():
    qml_file = op.join(op.dirname(qml.__file__), "diff.qml")
    app, engine = qml.create_app_engine(sys.argv, qml_file)
    ngl_widget = qml.create_ngl_widget(engine)
    diff = _Diff(engine, ngl_widget, sys.argv)
    ret = app.exec()
    del diff
    sys.exit(ret)
