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

from pynopegl_utils.misc import SceneCfg, scene
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.toolbox.colors import COLORS

import pynopegl as ngl

_FONT_DIR = Path(__file__).resolve().parent / "assets" / "fonts"
_ARABIC_FONT = _FONT_DIR / "Cairo-SemiBold.ttf"
_JAPANESE_FONT = _FONT_DIR / "TsukimiRounded-Regular.ttf"
_LATIN_FONT = _FONT_DIR / "Quicksand-Medium.ttf"


def _text(**params):
    return ngl.Text("This\nis\nnope.gl", font_scale=0.7, padding=8, **params)


@test_fingerprint(tolerance=1)
@scene()
def text_colors(_):
    return _text(fg_color=COLORS.rose, bg_color=COLORS.cgreen, bg_opacity=1)


@test_fingerprint(tolerance=1)
@scene()
def text_align_cc(_):
    return _text(valign="center", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_cr(_):
    return _text(valign="center", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_cl(_):
    return _text(valign="center", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_align_bc(_):
    return _text(valign="bottom", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_br(_):
    return _text(valign="bottom", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_bl(_):
    return _text(valign="bottom", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tc(_):
    return _text(valign="top", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tr(_):
    return _text(valign="top", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tl(_):
    return _text(valign="top", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_vertical_rl(_):
    return _text(writing_mode="vertical-rl")


@test_fingerprint(tolerance=1)
@scene()
def text_vertical_lr(_):
    return _text(writing_mode="vertical-lr")


@test_fingerprint(tolerance=1)
@scene()
def text_arabic_shaping(cfg: SceneCfg):
    """Advanced shaping that typically relies on the positioning of the diacritics"""
    return ngl.Text(
        text="رَسْميّ",
        font_files=_ARABIC_FONT.as_posix(),
        aspect_ratio=cfg.aspect_ratio,
    )


@test_fingerprint(tolerance=1)
@scene()
def text_bidi_arabic_english(cfg: SceneCfg):
    return ngl.Text(
        text="هذا مزيج من English و نص عربي",
        # The Latin font is placed first so the fallback is actually tested (the
        # Arabic font contains Latin letters)
        font_files=",".join(p.as_posix() for p in [_LATIN_FONT, _ARABIC_FONT]),
        aspect_ratio=cfg.aspect_ratio,
    )


@test_fingerprint(tolerance=1)
@scene()
def text_vertical_japanese(cfg: SceneCfg):
    return ngl.Text(
        # "Only kana, because that's a small font" (this is not a haiku)
        text="かなだけ、\nちいさいのフォント\nだから。",
        writing_mode="vertical-rl",
        font_files=_JAPANESE_FONT.as_posix(),
        aspect_ratio=cfg.aspect_ratio,
    )


@test_fingerprint(width=640, height=480, tolerance=1)
@scene()
def text_fixed(cfg: SceneCfg):
    cfg.aspect_ratio = (4, 3)
    return ngl.Text(
        "Fix",
        scale_mode="fixed",
        font_scale=50,
        aspect_ratio=cfg.aspect_ratio,
    )
