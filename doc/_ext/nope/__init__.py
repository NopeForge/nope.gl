#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Forge
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

import importlib
import os
import platform
import subprocess
from pathlib import Path
from textwrap import dedent
from typing import Any

import pynopegl as ngl
from docutils import nodes
from docutils.parsers.rst import directives
from sphinx.application import Sphinx
from sphinx.util.docutils import SphinxDirective
from sphinx.util.fileutil import copy_asset_file
from sphinx.writers.html5 import HTML5Translator


class _nope(nodes.Element):
    pass


# We inherit from SphinxDirective instead of Directive to get the .env property
class _Nope(SphinxDirective):
    _RES = 360

    @staticmethod
    def _export_type(argument: Any) -> str:
        return directives.choice(argument, ("image", "video"))

    required_arguments = 1
    optional_arguments = 0
    option_spec = dict(export_type=_export_type)
    has_content = True

    def run(self):
        module_name = self.arguments[0]
        module = importlib.import_module(f"nope.{module_name}")
        func_name = module_name.rsplit(".", 1)[-1]
        scene_func = getattr(module, func_name)
        content = "\n".join(self.content)

        cfg = ngl.SceneCfg(samples=4)
        data = scene_func(cfg)
        scene = data.scene

        ar = scene.aspect_ratio
        fps = scene.framerate
        height = self._RES
        width = int(height * ar[0] / ar[1])
        width &= ~1  # make sure it's a multiple of 2 for the h264 codec

        export_type = self.options.get("export_type", "video")
        if export_type == "image":
            ext = "png"
            ffmpeg_options = ["-frames:v", "1", "-update", "1"]
            times = [0.0]  # request only one frame at t=0
        elif export_type == "video":
            ext = "mp4"
            ffmpeg_options = ["-pix_fmt", "yuv420p"]  # for compatibility
            nb_frame = int(scene.duration * fps[0] / fps[1])
            times = [i * fps[1] / float(fps[0]) for i in range(nb_frame)]
        else:
            assert False

        out_visual_rel, _ = self.env.relfn2path(f"{module_name}.{ext}")
        out_graph_rel, _ = self.env.relfn2path(f"{module_name}_graph.png")

        out_dir = Path(self.env.app.outdir)
        out_visual = out_dir / out_visual_rel
        out_graph = out_dir / out_graph_rel

        out_visual.parent.mkdir(parents=True, exist_ok=True)
        out_graph.parent.mkdir(parents=True, exist_ok=True)

        out_dot = out_graph.as_posix()[:-3] + "dot"
        with open(out_dot, "wb") as f:
            f.write(scene.dot())
        subprocess.run(["dot", "-Tpng", out_dot, "-o", out_graph])
        Path(out_dot).unlink()

        fd_r, fd_w = os.pipe()

        ffmpeg = ["ffmpeg"]
        input = f"pipe:{fd_r}"

        if platform.system() == "Windows":
            import msvcrt
            import sys

            handle = msvcrt.get_osfhandle(fd_r)
            os.set_handle_inheritable(handle, True)
            input = f"handle:{handle}"
            ffmpeg = [sys.executable, "-m", "pynopegl_utils.viewer.ffmpeg_win32"]

        # fmt: off
        cmd = ffmpeg + [
            "-r", "%d/%d" % fps,
            "-v", "warning",
            "-nostats", "-nostdin",
            "-f", "rawvideo",
            "-video_size", "%dx%d" % (width, height),
            "-pixel_format", "rgba",
            "-i", input,
        ] + ffmpeg_options + [
            "-y", out_visual,
        ]
        # fmt: on

        if platform.system() == "Windows":
            reader = subprocess.Popen(cmd, close_fds=False)
        else:
            reader = subprocess.Popen(cmd, pass_fds=(fd_r,))
        os.close(fd_r)

        capture_buffer = bytearray(width * height * 4)

        ctx = ngl.Context()
        ctx.configure(
            ngl.Config(
                platform=ngl.Platform.AUTO,
                backend=ngl.Backend.AUTO,
                offscreen=True,
                width=width,
                height=height,
                samples=cfg.samples,
                capture_buffer=capture_buffer,
            )
        )
        ctx.set_scene(scene)

        for t in times:
            ctx.draw(t)
            os.write(fd_w, capture_buffer)

        os.close(fd_w)
        reader.wait()

        source_file = module.__file__
        assert source_file is not None
        self.env.note_dependency(source_file)
        code = open(source_file).read()

        node = _nope()
        node["text"] = content
        node["code"] = code
        node["visual"] = out_visual.name
        node["graph"] = out_graph.name
        return [node]


def _visit_nope_html(self: HTML5Translator, node: _nope):
    text = node["text"]
    code = node["code"]
    visual = node["visual"]
    graph = node["graph"]

    if visual.endswith(".png"):
        visual_html = f'<img src="{visual}" alt="">'
    elif visual.endswith(".mp4"):
        visual_html = f'<video class="nope-video" loop muted autoplay><source src="{visual}" type="video/mp4"></video>'
    else:
        assert False

    highlighted_code = self.highlighter.highlight_block(code, "python")

    html = dedent(
        f"""
        <div class="nope-snippet">
            <div class="nope-title">{text}</div>
            <div class="nope-visual">{visual_html}</div>
            <div class="nope-tabs">
                <div class="nope-tabs-bar">
                    <div class="nope-btn-on nope-btn-code" onclick="show_tab(this.parentNode, 'code')">Code</div>
                    <div class="nope-btn-off nope-btn-graph" onclick="show_tab(this.parentNode, 'graph')">Graph</div>
                </div>
                <div class="nope-tab-on nope-tab-code">
                    <div class="highlight-python notranslate nope-python">{highlighted_code}</div>
                </div>
                <div class="nope-tab-off nope-tab-graph">
                    <img src="{graph}" alt="graph">
                </div>
            </div>
        </div>
        """
    )
    self.body.append(html)
    raise nodes.SkipNode


def _copy_custom_files(app: Sphinx, exc):
    if app.builder.format == "html" and not exc:
        dst_dir = Path(app.builder.outdir) / "_static"
        cur_dir = Path(__file__).resolve().parent
        for fname in {"nope.css", "nope.js"}:
            src_path = cur_dir / fname
            copy_asset_file(src_path.as_posix(), dst_dir.as_posix())


def setup(app: Sphinx):
    app.add_node(_nope, html=(_visit_nope_html, None))
    app.add_directive("nope", _Nope)
    app.add_js_file("nope.js")
    app.add_css_file("nope.css")
    app.connect("build-finished", _copy_custom_files)
