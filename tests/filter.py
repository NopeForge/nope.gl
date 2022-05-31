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

from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.toolbox.colors import COLORS

import pynodegl as ngl

_CUEPOINTS = dict(c=(0, 0), bl=(-0.5, -0.5), br=(0.5, -0.5), tr=(0.5, 0.5), tl=(-0.5, 0.5))


def _base_scene(*filters):
    return ngl.RenderGradient4(
        opacity_tl=0.3,
        opacity_tr=0.4,
        opacity_br=0.5,
        opacity_bl=0.6,
        filters=filters,
    )


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_alpha(_):
    return _base_scene(ngl.FilterAlpha(0.4321))


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_contrast(_):
    return _base_scene(ngl.FilterContrast(1.2, pivot=0.3))


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_exposure(_):
    return _base_scene(ngl.FilterExposure(0.7))


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_inversealpha(_):
    return _base_scene(ngl.FilterInverseAlpha())


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_opacity(_):
    return _base_scene(ngl.FilterOpacity(0.4321))


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_saturation(_):
    return _base_scene(ngl.FilterSaturation(1.5))


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_composition_colors(_):
    return ngl.RenderGradient4(
        filters=(
            ngl.FilterExposure(exposure=0.9),
            ngl.FilterContrast(contrast=1.5),
            ngl.FilterSaturation(saturation=1.1),
            ngl.FilterOpacity(opacity=0.8),
        ),
    )


@test_cuepoints(points=_CUEPOINTS, nb_keyframes=1, tolerance=1)
@scene()
def filter_composition_alpha(_):
    return ngl.RenderGradient(
        color0=(1, 0.5, 0),
        color1=(0, 1, 0.5),
        opacity0=0.8,
        opacity1=0.9,
        mode="radial",
        filters=(
            ngl.FilterAlpha(alpha=1.0),
            ngl.FilterInverseAlpha(),
            ngl.FilterAlpha(alpha=0.1),
            ngl.FilterInverseAlpha(),
            ngl.FilterPremult(),
        ),
    )


@test_cuepoints(points=_CUEPOINTS, width=320, height=240, nb_keyframes=1, tolerance=1)
@scene(linear=scene.Bool())
def filter_gamma_correct(_, linear=True):
    """This test operates a gamma correct blending (the blending happens in linear space)"""

    # Hue colors rotated clockwise
    dst = ngl.RenderGradient4(
        color_tl=COLORS.rose,
        color_tr=COLORS.blue,
        color_br=COLORS.sgreen,
        color_bl=COLORS.yellow,
    )

    # Hue colors rotated counter-clockwise started with another color
    src = ngl.RenderGradient4(
        color_tl=COLORS.orange,
        color_tr=COLORS.magenta,
        color_br=COLORS.azure,
        color_bl=COLORS.green,
    )

    # Screen blending so that working in linear space makes a significant
    # difference
    blend = ngl.GraphicConfig(
        ngl.Group(children=(dst, src)),
        blend=True,
        blend_src_factor="one",
        blend_dst_factor="one_minus_src_color",
        blend_src_factor_a="one",
        blend_dst_factor_a="zero",
    )

    # Intermediate RTT so that we can gamma correct the result
    tex = ngl.Texture2D(width=320, height=240)
    rtt = ngl.RenderToTexture(blend, color_textures=[tex])
    render = ngl.RenderTexture(tex)

    if linear:
        # The result of the combination is linear
        dst.add_filters(ngl.FilterSRGB2Linear())
        src.add_filters(ngl.FilterSRGB2Linear())
        # ...and we compress it back to sRGB
        render.add_filters(ngl.FilterLinear2sRGB())

    return ngl.Group(children=(rtt, render))
