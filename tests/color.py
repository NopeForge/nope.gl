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


from pynodegl_utils.misc import SceneCfg, scene
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints

import pynodegl as ngl


def _get_anim_color_scene_func(c0, c1, space):
    @test_cuepoints(points={"c": (0, 0)}, nb_keyframes=10, tolerance=1)
    @scene()
    def scene_func(cfg: SceneCfg):
        cfg.duration = 5
        color_animkf = [
            # Start at t=1 and end 1s earlier so that it tests the underflow
            # and overflow of the animation
            ngl.AnimKeyFrameColor(1, c0),
            ngl.AnimKeyFrameColor(cfg.duration - 1, c1),
        ]
        ucolor = ngl.AnimatedColor(color_animkf, space=space)
        return ngl.RenderColor(ucolor)

    return scene_func


def _get_static_color_scene_func(c, space):
    @test_cuepoints(points={"c": (0, 0)}, nb_keyframes=1, tolerance=1)
    @scene()
    def scene_func(_):
        return ngl.RenderColor(color=ngl.UniformColor(c, space=space))

    return scene_func


color_anim_srgb = _get_anim_color_scene_func((1.0, 0.5, 0.0), (0.0, 0.6, 1.0), "srgb")
color_anim_hsl = _get_anim_color_scene_func((0.6, 0.9, 0.4), (0.1, 0.5, 0.6), "hsl")
color_anim_hsv = _get_anim_color_scene_func((0.3, 0.7, 0.6), (1.0, 1.0, 0.7), "hsv")

color_static_srgb = _get_static_color_scene_func((1.0, 0.5, 0.0), "srgb")
color_static_hsl = _get_static_color_scene_func((0.6, 0.9, 0.4), "hsl")
color_static_hsv = _get_static_color_scene_func((0.3, 0.7, 0.6), "hsv")


@test_cuepoints(points={"c": (0, 0)}, nb_keyframes=10, tolerance=0)
@scene()
def color_negative_values_srgb(cfg: SceneCfg):
    cfg.duration = 5
    kfs = (
        # The elastic_in easing has the special property to undershoot under 0
        ngl.AnimKeyFrameVec3(-5, value=(0.0, 0.0, 0.0)),
        ngl.AnimKeyFrameVec3(5, value=(0.0, 0.0, 1.0), easing="elastic_in"),
    )
    color0 = ngl.AnimatedVec3(keyframes=kfs)
    color1 = ngl.UniformVec3(value=(-1.0, -1.0, 1.0))
    return ngl.RenderGradient(color0=color0, color1=color1, linear=True)
