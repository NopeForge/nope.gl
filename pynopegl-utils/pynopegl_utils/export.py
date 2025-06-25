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
import os.path as op
import platform
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import List, Optional

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
        # tempfile.NamedTemporaryFile has limitations on Windows, see
        # https://docs.python.org/3/library/tempfile.html#tempfile.NamedTemporaryFile
        with tempfile.TemporaryDirectory(prefix="Nope_") as td:
            palette_name = op.join(td, "palette.png")

            # pass 1
            extra_enc_args = ["-vf", "palettegen", "-update", "1", "-frames:v", "1", "-c:v", "png", "-f", "image2"]
            export = _export_worker(scene_info, palette_name, resolution, extra_enc_args)
            for progress in export:
                yield progress / 2

            # pass 2
            extra_enc_args = ["-i", palette_name, "-lavfi", "paletteuse", "-c:v", profile.encoder, "-f", profile.format]
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
            backend=scene_info.backend,
            offscreen=True,
            width=width,
            height=height,
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


def run():
    import argparse
    import re
    from inspect import getmembers
    from pathlib import Path

    from pynopegl_utils.misc import SceneCfg, get_backend
    from pynopegl_utils.module import load_script

    parser = argparse.ArgumentParser()
    parser.add_argument(
        dest="input",
        type=Path,
        help="path to the script containing scenes",
    )
    parser.add_argument(
        "-f",
        dest="filter",
        default=r".*",
        help="filter functions using a regex",
    )
    parser.add_argument(
        "-n",
        dest="dry_run",
        action="store_true",
        help="perform a dry run (list what files would be written)",
    )
    parser.add_argument(
        "-o",
        dest="output",
        type=Path,
        default="ngl-exports",
        help="path to the export directory",
    )
    parser.add_argument(
        "-w",
        dest="overwrite",
        action="store_true",
        help="overwrite existing files",
    )
    parser.add_argument(
        "-b",
        dest="backend",
        default="auto",
        choices=[x.name.lower() for x in ngl.Backend],
    )
    parser.add_argument(
        "-r",
        dest="resolution",
        default="1080p",
        choices=RESOLUTIONS.keys(),
        help="output video resolution",
    )
    parser.add_argument(
        "-p",
        dest="profile",
        default="nut_ffv1",
        choices=ENCODE_PROFILES.keys(),
        help="output video profile",
    )
    parser.add_argument(
        "-m",
        dest="samples",
        type=int,
        default=1,
        choices=[0, 1, 2, 4, 8],
        help="number of samples used for multisample anti-aliasing",
    )
    args = parser.parse_args()

    outdir = args.output
    outdir.mkdir(exist_ok=True)

    jobs = []
    module = load_script(args.input)
    for func_name, func in getmembers(module, callable):
        if not hasattr(func, "iam_a_ngl_scene_func"):
            continue
        if not re.match(args.filter, func_name):
            continue
        ext = ENCODE_PROFILES[args.profile].format
        filename = outdir / f"{func_name}.{ext}"
        if filename.exists():
            if args.overwrite:
                print(f"{filename.name}: already present, will overwrite")
            else:
                print(f"{filename.name}: already present, skipping")
                continue
        jobs.append((func, filename))

    n = len(jobs)
    if args.dry_run:
        for i, job in enumerate(jobs):
            _, filename = job
            print(f"[{i+1}/{n}] {filename.name}: would encode (dry run)")
    else:
        for i, job in enumerate(jobs):
            func, filename = job
            cfg = SceneCfg(samples=args.samples, backend=get_backend(args.backend))
            data = func(cfg)
            export = export_workers(data, filename.as_posix(), resolution=args.resolution, profile_id=args.profile)
            for progress in export:
                sys.stdout.write(f"\r[{i+1}/{n}] {filename.name}: {progress:.1f}%")
                sys.stdout.flush()
            sys.stdout.write("\n")
