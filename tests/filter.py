#
# Copyright 2021-2022 GoPro Inc.
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

import math

from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.toolbox.colors import COLORS

import pynopegl as ngl

_CUEPOINTS = dict(c=(0, 0), bl=(-0.5, -0.5), br=(0.5, -0.5), tr=(0.5, 0.5), tl=(-0.5, 0.5))


def _base_scene(cfg: ngl.SceneCfg, *filters):
    cfg.aspect_ratio = (1, 1)
    return ngl.DrawGradient4(
        opacity_tl=0.3,
        opacity_tr=0.4,
        opacity_br=0.5,
        opacity_bl=0.6,
        filters=list(filters),
    )


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_alpha(cfg: ngl.SceneCfg):
    return _base_scene(cfg, ngl.FilterAlpha(0.4321))


@test_cuepoints(
    width=128,
    height=128,
    points={f"x{i}": (i / (5 - 1) * 2 - 1, 0) for i in range(5)},
    keyframes=10,
    tolerance=1,
)
@ngl.scene()
def filter_colormap(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 5.0
    d = cfg.duration

    # Try to animate slightly out of bound to test the clamping
    off = 0.1

    p0_animkf = [
        ngl.AnimKeyFrameFloat(0, -off),
        ngl.AnimKeyFrameFloat(d / 2, 2 / 3),
        ngl.AnimKeyFrameFloat(d, -off),
    ]
    p1_animkf = [
        ngl.AnimKeyFrameFloat(0, 1 / 4),
        ngl.AnimKeyFrameFloat(d / 2, 3 / 4),
        ngl.AnimKeyFrameFloat(d, 1 / 4),
    ]
    p2_animkf = [
        ngl.AnimKeyFrameFloat(0, 1 + off),
        ngl.AnimKeyFrameFloat(d / 2, 1 / 3),
        ngl.AnimKeyFrameFloat(d, 1 + off),
    ]

    p0 = ngl.AnimatedFloat(keyframes=p0_animkf)
    p1 = ngl.AnimatedFloat(keyframes=p1_animkf)
    p2 = ngl.AnimatedFloat(keyframes=p2_animkf)

    p0 = ngl.AnimatedFloat(keyframes=p0_animkf)
    p1 = ngl.AnimatedFloat(keyframes=p1_animkf)
    p2 = ngl.AnimatedFloat(keyframes=p2_animkf)

    c0 = ngl.UniformColor(value=(1, 0.4, 0.4))
    c1 = ngl.UniformColor(value=(0.3, 0.8, 0.3))
    c2 = ngl.UniformColor(value=(0.5, 0.5, 1))

    bg = ngl.DrawColor(color=(1, 0.5, 1))
    remapped = ngl.DrawGradient4(
        # Black to white grayscale from left to right
        color_tl=(0, 0, 0),
        color_bl=(0, 0, 0),
        color_tr=(1, 1, 1),
        color_br=(1, 1, 1),
        linear=False,
        blending="src_over",
        filters=[
            ngl.FilterColorMap(
                colorkeys=[
                    ngl.ColorKey(position=p0, color=c0, opacity=0.2),
                    ngl.ColorKey(position=p1, color=c1),
                    ngl.ColorKey(position=p2, color=c2, opacity=0.8),
                ]
            )
        ],
    )
    return ngl.Group(children=[bg, remapped])


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_contrast(cfg: ngl.SceneCfg):
    return _base_scene(cfg, ngl.FilterContrast(1.2, pivot=0.3))


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_exposure(cfg: ngl.SceneCfg):
    return _base_scene(cfg, ngl.FilterExposure(0.7))


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_inversealpha(cfg: ngl.SceneCfg):
    return _base_scene(cfg, ngl.FilterInverseAlpha())


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_opacity(cfg: ngl.SceneCfg):
    return _base_scene(cfg, ngl.FilterOpacity(0.4321))


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_saturation(cfg: ngl.SceneCfg):
    return _base_scene(cfg, ngl.FilterSaturation(1.5))


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_selector_light(cfg: ngl.SceneCfg):
    return _base_scene(
        cfg,
        ngl.FilterSelector(
            range=(0.41, 0.5),
            component="lightness",
            drop_mode="inside",
            output_mode="colorholes",
        ),
    )


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_selector_chroma(cfg: ngl.SceneCfg):
    return _base_scene(
        cfg,
        ngl.FilterSelector(
            range=(0.06, 0.1),
            component="chroma",
            drop_mode="outside",
            smoothedges=True,
        ),
    )


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_selector_hue(cfg: ngl.SceneCfg):
    return _base_scene(
        cfg,
        ngl.FilterSelector(
            range=(-0.1 * math.tau, 0.45 * math.tau),
            component="hue",
            drop_mode="outside",
            output_mode="binary",
        ),
    )


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_composition_colors(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    return ngl.DrawGradient4(
        filters=[
            ngl.FilterExposure(exposure=0.9),
            ngl.FilterContrast(contrast=1.5),
            ngl.FilterSaturation(saturation=1.1),
            ngl.FilterOpacity(opacity=0.8),
        ],
    )


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, keyframes=1, tolerance=1)
@ngl.scene()
def filter_composition_alpha(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    return ngl.DrawGradient(
        color0=(1, 0.5, 0),
        color1=(0, 1, 0.5),
        opacity0=0.8,
        opacity1=0.9,
        mode="radial",
        filters=[
            ngl.FilterAlpha(alpha=1.0),
            ngl.FilterInverseAlpha(),
            ngl.FilterAlpha(alpha=0.1),
            ngl.FilterInverseAlpha(),
            ngl.FilterPremult(),
        ],
    )


@test_cuepoints(points=_CUEPOINTS, width=320, height=240, keyframes=1, tolerance=1)
@ngl.scene(controls=dict(linear=ngl.scene.Bool()))
def filter_gamma_correct(cfg: ngl.SceneCfg, linear=True):
    """This test operates a gamma correct blending (the blending happens in linear space)"""
    cfg.aspect_ratio = (4, 3)

    # Hue colors rotated clockwise
    dst = ngl.DrawGradient4(
        color_tl=COLORS.rose,
        color_tr=COLORS.blue,
        color_br=COLORS.sgreen,
        color_bl=COLORS.yellow,
    )

    # Hue colors rotated counter-clockwise started with another color
    src = ngl.DrawGradient4(
        color_tl=COLORS.orange,
        color_tr=COLORS.magenta,
        color_br=COLORS.azure,
        color_bl=COLORS.green,
    )

    # Screen blending so that working in linear space makes a significant
    # difference
    blend = ngl.GraphicConfig(
        ngl.Group(children=[dst, src]),
        blend=True,
        blend_src_factor="one",
        blend_dst_factor="one_minus_src_color",
        blend_src_factor_a="one",
        blend_dst_factor_a="zero",
    )

    # Intermediate RTT so that we can gamma correct the result
    tex = ngl.Texture2D(width=320, height=240, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(blend, color_textures=[tex])
    draw = ngl.DrawTexture(tex)

    if linear:
        # The result of the combination is linear
        dst.add_filters(ngl.FilterSRGB2Linear())
        src.add_filters(ngl.FilterSRGB2Linear())
        # ...and we compress it back to sRGB
        draw.add_filters(ngl.FilterLinear2sRGB())

    return ngl.Group(children=[rtt, draw])
