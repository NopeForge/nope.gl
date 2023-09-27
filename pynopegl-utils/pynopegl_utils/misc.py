#
# Copyright 2016-2022 GoPro Inc.
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
import os
import os.path as op
import pkgutil
import subprocess
import tempfile
from dataclasses import dataclass
from fractions import Fraction
from typing import Tuple

import pynopegl as ngl

# Temporary kept for compatibility
scene = ngl.scene
SceneCfg = ngl.SceneCfg
SceneInfo = ngl.SceneInfo


@dataclass(frozen=True)
class MediaInfo:
    filename: str
    width: int
    height: int
    pix_fmt: str
    duration: float
    time_base: Fraction
    avg_frame_rate: Fraction

    @classmethod
    def from_filename(cls, filename: str):
        cmd = ["ffprobe", "-show_format", "-show_streams", "-of", "json", "-i", filename]
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
        duration = float(duration)
        avg_frame_rate = Fraction(vst["avg_frame_rate"])

        return cls(
            filename=filename,
            width=vst["width"],
            height=vst["height"],
            pix_fmt=vst["pix_fmt"],
            duration=duration,
            time_base=time_base,
            avg_frame_rate=avg_frame_rate,
        )


def get_nopegl_tempdir() -> str:
    tmpdir = op.join(tempfile.gettempdir(), "nopegl")
    os.makedirs(tmpdir, exist_ok=True)
    return tmpdir


MEDIA_FILES_DB = dict(
    mire="mire-hevc.mp4",
    cat="cat.mp4",
    fallen_leaf="OpenMoji-1F342-Fallen_Leaf.png",
    hamster="OpenMoji-1F439-Hamster.png",
    rooster="Unsplash-Michael_Anfang-Rooster-cropped.jpg",
    panda="Unsplash-Romane_Gautun-Red_Panda-cropped.jpg",
)


def get_shader(name: str) -> str:
    data = pkgutil.get_data("pynopegl_utils.examples.shaders", name)
    if data is None:
        raise FileNotFoundError(f"Unable to find shader {name}")
    return data.decode()


def load_media(cfg: ngl.SceneCfg, file: str):
    db_file = MEDIA_FILES_DB.get(file)
    if db_file:
        file = op.join(op.dirname(__file__), "assets", db_file)
    media = MediaInfo.from_filename(file)
    cfg.files.append(media.filename)  # Register the file for hooks
    return media


def get_viewport(width: int, height: int, aspect_ratio: Tuple[int, int]) -> Tuple[int, int, int, int]:
    view_width = width
    view_height = width * aspect_ratio[1] // aspect_ratio[0]
    if view_height > height:
        view_height = height
        view_width = height * aspect_ratio[0] // aspect_ratio[1]
    view_x = (width - view_width) // 2
    view_y = (height - view_height) // 2
    return (view_x, view_y, view_width, view_height)


def get_backend(backend: str) -> ngl.Backend:
    return ngl.Backend[backend.upper()]
