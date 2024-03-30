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

from pathlib import Path

from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.toolbox.colors import COLORS

import pynopegl as ngl

_FONT_DIR = Path(__file__).resolve().parent / "assets" / "fonts"
_ARABIC_FONT = _FONT_DIR / "Cairo-SemiBold.ttf"
_JAPANESE_FONT = _FONT_DIR / "TsukimiRounded-Regular.ttf"
_LATIN_FONT = _FONT_DIR / "Quicksand-Medium.ttf"


def _text(cfg: ngl.SceneCfg, **params):
    cfg.aspect_ratio = (1, 1)
    return ngl.Text("This\nis\nnope.gl", font_scale=0.7, padding=8, **params)


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_colors(cfg: ngl.SceneCfg):
    return _text(cfg, fg_color=COLORS.rose, bg_color=COLORS.cgreen, bg_opacity=1)


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_cc(cfg: ngl.SceneCfg):
    return _text(cfg, valign="center", halign="center")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_cr(cfg: ngl.SceneCfg):
    return _text(cfg, valign="center", halign="right")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_cl(cfg: ngl.SceneCfg):
    return _text(cfg, valign="center", halign="left")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_bc(cfg: ngl.SceneCfg):
    return _text(cfg, valign="bottom", halign="center")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_br(cfg: ngl.SceneCfg):
    return _text(cfg, valign="bottom", halign="right")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_bl(cfg: ngl.SceneCfg):
    return _text(cfg, valign="bottom", halign="left")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_tc(cfg: ngl.SceneCfg):
    return _text(cfg, valign="top", halign="center")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_tr(cfg: ngl.SceneCfg):
    return _text(cfg, valign="top", halign="right")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_align_tl(cfg: ngl.SceneCfg):
    return _text(cfg, valign="top", halign="left")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_vertical_rl(cfg: ngl.SceneCfg):
    return _text(cfg, writing_mode="vertical-rl")


@test_fingerprint(width=320, height=320, tolerance=1)
@ngl.scene()
def text_vertical_lr(cfg: ngl.SceneCfg):
    return _text(cfg, writing_mode="vertical-lr")


@test_fingerprint(width=320, height=240, tolerance=1)
@ngl.scene()
def text_arabic_shaping(cfg: ngl.SceneCfg):
    """Advanced shaping that typically relies on the positioning of the diacritics"""
    cfg.aspect_ratio = (4, 3)
    return ngl.Text(
        text="رَسْميّ",
        font_faces=[ngl.FontFace(_ARABIC_FONT.as_posix())],
    )


@test_fingerprint(width=320, height=240, tolerance=1)
@ngl.scene()
def text_bidi_arabic_english(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)
    return ngl.Text(
        text="هذا مزيج من English و نص عربي",
        # The Latin font is placed first so the fallback is actually tested (the
        # Arabic font contains Latin letters)
        font_faces=[ngl.FontFace(p.as_posix()) for p in [_LATIN_FONT, _ARABIC_FONT]],
    )


@test_fingerprint(width=480, height=640, tolerance=1)
@ngl.scene()
def text_vertical_japanese(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (3, 4)
    return ngl.Text(
        # "Only kana, because that's a small font" (this is not a haiku)
        text="かなだけ、\nちいさいのフォント\nだから。",
        writing_mode="vertical-rl",
        font_faces=[ngl.FontFace(_JAPANESE_FONT.as_posix())],
    )


@test_fingerprint(width=640, height=480, tolerance=1)
@ngl.scene()
def text_fixed(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)
    return ngl.Text(
        "Fix",
        scale_mode="fixed",
        font_scale=4.5,
    )


@test_fingerprint(width=640, height=480, tolerance=1, keyframes=5)
@ngl.scene()
def text_animated(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)
    cfg.duration = 2
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 360),
    ]
    return ngl.Rotate(ngl.Text("Hey"), angle=ngl.AnimatedFloat(animkf), axis=(1, 1, 1))


@test_fingerprint(width=360, height=640, tolerance=1, keyframes=5)
@ngl.scene()
def text_animated_fixed(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (0, 1)
    cfg.duration = 2
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 360),
    ]
    return ngl.Rotate(
        ngl.Text("Hey", scale_mode="fixed", font_scale=8), angle=ngl.AnimatedFloat(animkf), axis=(1, 1, 1)
    )
