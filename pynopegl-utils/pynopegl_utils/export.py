#
# Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Foundry
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
import platform
import subprocess
import sys
from typing import List, Optional

from pynopegl_utils.misc import SceneInfo, get_backend, get_viewport

import pynopegl as ngl


def export_worker(
    scene_info: SceneInfo,
    filename: str,
    width: int,
    height: int,
    extra_enc_args: Optional[List[str]] = None,
):
    scene = scene_info.scene
    fps = scene.framerate
    duration = scene.duration
    samples = scene_info.samples

    fd_r, fd_w = os.pipe()

    ffmpeg = ["ffmpeg"]
    input = f"pipe:{fd_r}"

    if platform.system() == "Windows":
        import msvcrt

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
    ]
    # fmt: on
    if extra_enc_args:
        cmd += extra_enc_args
    cmd += ["-y", filename]

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
