#
# Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Forge
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
import tempfile
from dataclasses import dataclass
from typing import List, Optional

from pynopegl_utils.misc import get_backend, get_viewport

import pynopegl as ngl

RESOLUTIONS = {
    "144p": 144,
    "240p": 240,
    "360p": 360,
    "480p": 480,
    "720p": 720,
    "1080p": 1080,
    "1440p": 1440,
    "4k": 2160,
}


@dataclass
class EncodeProfile:
    name: str
    format: str
    encoder: str
    args: List[str]


ENCODE_PROFILES = dict(
    mp4_h264_420=EncodeProfile(
        name="MP4 / H264 4:2:0",
        format="mp4",
        # Since 4:2:0 is used for portability (over the Internet typically), we also use faststart
        args=["-pix_fmt", "yuv420p", "-crf", "18", "-movflags", "+faststart"],
        encoder="libx264",
    ),
    mp4_h264_444=EncodeProfile(
        name="MP4 / H264 4:4:4",
        format="mp4",
        args=["-pix_fmt", "yuv444p", "-crf", "18"],
        encoder="libx264",
    ),
    mp4_av1_420=EncodeProfile(
        name="MP4 / AV1 4:2:0",
        format="mp4",
        # Since 4:2:0 is used for portability (most hardware decoders only support the main profile (4:2:0)), we also use faststart
        args=["-pix_fmt", "yuv420p", "-crf", "18", "-movflags", "+faststart"],
        encoder="libsvtav1",
    ),
    mov_qtrle=EncodeProfile(
        name="MOV / QTRLE (Lossless)",
        format="mov",
        encoder="qtrle",
        args=[],
    ),
    nut_ffv1=EncodeProfile(
        name="NUT / FFV1 (Lossless)",
        format="nut",
        encoder="ffv1",
        args=[],
    ),
    gif=EncodeProfile(
        name="Animated GIF",
        format="gif",
        encoder="gif",
        args=[],
    ),
)


def export_workers(scene_info: ngl.SceneInfo, filename: str, resolution: str, profile_id: str):
    profile = ENCODE_PROFILES[profile_id]
    if profile.format == "gif":
        with tempfile.NamedTemporaryFile(prefix="palette-", suffix=".png") as palette:
            # pass 1
            extra_enc_args = ["-vf", "palettegen", "-update", "1", "-frames:v", "1", "-c:v", "png", "-f", "image2"]
            export = _export_worker(scene_info, palette.name, resolution, extra_enc_args)
            for progress in export:
                yield progress / 2

            # pass 2
            extra_enc_args = ["-i", palette.name, "-lavfi", "paletteuse", "-c:v", profile.encoder, "-f", profile.format]
            export = _export_worker(scene_info, filename, resolution, extra_enc_args)
            for progress in export:
                yield 50 + progress / 2
    else:
        extra_enc_args = profile.args + ["-c:v", profile.encoder, "-f", profile.format]
        export = _export_worker(scene_info, filename, resolution, extra_enc_args)
        for progress in export:
            yield progress


def _export_worker(
    scene_info: ngl.SceneInfo,
    filename: str,
    resolution: str,
    extra_enc_args: Optional[List[str]] = None,
):
    scene = scene_info.scene
    fps = scene.framerate
    duration = scene.duration
    samples = scene_info.samples

    ar = scene.aspect_ratio
    height = RESOLUTIONS[resolution]
    width = int(height * ar[0] / ar[1])
    width &= ~1  # make sure it's a multiple of 2 for the h264 codec

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
