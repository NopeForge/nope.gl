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

import json
import os.path as op
import pkgutil
import subprocess
import sys
from dataclasses import dataclass
from fractions import Fraction

from pynodegl_utils import qml
from PySide6.QtCore import QObject, Slot

import pynodegl as ngl


@dataclass
class _MediaInfo:
    fname: str
    width: int
    height: int
    pix_fmt: str
    duration: Fraction
    time_base: Fraction
    avg_frame_rate: Fraction

    @classmethod
    def from_filename(cls, fname: str):
        cmd = ["ffprobe", "-show_format", "-show_streams", "-of", "json", "-i", fname]
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
        avg_frame_rate = Fraction(vst["avg_frame_rate"])

        return cls(
            fname=fname,
            width=vst["width"],
            height=vst["height"],
            pix_fmt=vst["pix_fmt"],
            duration=duration,
            time_base=time_base,
            avg_frame_rate=avg_frame_rate,
        )


class _Diff:
    def __init__(self, qml_engine, ngl_widget, args):
        if len(args) != 3:
            print(f"Usage: {args[0]} <media0> <media1>")
            sys.exit(1)

        self._livectls = {}
        self._reframing_scale = 1
        self._reframing_off = (0, 0)

        self._ngl_widget = ngl_widget
        self._ngl_widget.livectls_changed.connect(self._livectls_changed)

        fname0, fname1 = args[1], args[2]
        media0 = _MediaInfo.from_filename(fname0)
        media1 = _MediaInfo.from_filename(fname1)

        width = max(media0.width, media1.width)
        height = max(media0.height, media1.height)
        duration = max(media0.duration, media1.duration)
        time_base = media0.time_base if media0.duration >= media1.duration else media1.time_base
        avg_frame_rate = max(media0.avg_frame_rate, media1.avg_frame_rate)

        assert time_base > 0
        scene = self._get_scene(fname0, fname1)
        self._ngl_widget.set_scene(scene)

        app_window = qml_engine.rootObjects()[0]
        self._player = app_window.findChild(QObject, "player")

        for i, media in enumerate((media0, media1)):
            app_window.setProperty(f"filename{i}", op.basename(media.fname))
            app_window.setProperty(f"width{i}", media.width)
            app_window.setProperty(f"height{i}", media.height)
            app_window.setProperty(f"pix_fmt{i}", media.pix_fmt)
            app_window.setProperty(f"duration{i}", float(media.duration))
            app_window.setProperty(f"avg_frame_rate{i}", float(media.avg_frame_rate))

        self._player.setProperty("duration", float(duration))
        self._player.setProperty("framerate", list(avg_frame_rate.as_integer_ratio()))
        self._player.setProperty("aspect", [width, height])

        app_window.diffModeToggled.connect(self._diff_mode_toggled)
        app_window.verticalSplitChanged.connect(self._vertical_split_changed)
        app_window.thresholdMoved.connect(self._threshold_moved)
        app_window.showCompChanged.connect(self._show_comp_changed)
        app_window.premultipliedChanged.connect(self._premultiplied_changed)

        self._player.timeChanged.connect(ngl_widget.set_time)
        self._player.mouseDown.connect(self._mouse_down)
        self._player.zoom.connect(self._zoom)
        self._player.pan.connect(self._pan)

    @Slot(object)
    def _livectls_changed(self, data):
        self._livectls = {v["label"]: v for v in data}

    def _update_livectl(self, key, val):
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

    def _get_scene(
        self,
        file0,
        file1,
        diff_mode=False,
        split=(0.5, 0.5),
        vertical_split=True,
        show_r=True,
        show_g=True,
        show_b=True,
        show_a=True,
        premultiplied=False,
        threshold=0.001,
    ):
        vert = pkgutil.get_data("pynodegl_utils.diff.shaders", "diff.vert")
        frag = pkgutil.get_data("pynodegl_utils.diff.shaders", "diff.frag")
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
            tex0=ngl.Texture2D(data_src=ngl.Media(file0)),
            tex1=ngl.Texture2D(data_src=ngl.Media(file1)),
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
        return scene


def run():
    qml_file = op.join(op.dirname(qml.__file__), "diff.qml")
    app, engine = qml.create_app_engine(sys.argv, qml_file)
    ngl_widget = qml.create_ngl_widget(engine)
    diff = _Diff(engine, ngl_widget, sys.argv)  # noqa: need to assign for correct lifetime
    sys.exit(app.exec())
