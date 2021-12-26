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

import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.toolbox.colors import COLORS


def _get_userlive_switch_func():
    scene0 = ngl.RenderColor(COLORS.white[:3], geometry=ngl.Circle())
    scene1 = ngl.RenderColor(COLORS.red[:3], geometry=ngl.Quad())
    scene2 = ngl.RenderColor(COLORS.azure[:3], geometry=ngl.Triangle())

    switch0 = ngl.UserSwitch(ngl.Scale(scene0, factors=(1/3, 1/3, 1/3), anchor=(-1, 0, 0)))
    switch1 = ngl.UserSwitch(ngl.Scale(scene1, factors=(1/2, 1/2, 1/2)))
    switch2 = ngl.UserSwitch(ngl.Scale(scene2, factors=(1/3, 1/3, 1/3), anchor=(1, 0, 0)))

    def keyframes_callback(t_id):
        # Build a "random" composition of switches
        switch0.set_enabled(t_id % 2 == 0)
        switch1.set_enabled(t_id % 3 == 0)
        switch2.set_enabled(t_id % 4 == 0)

    @test_fingerprint(nb_keyframes=10,
                      keyframes_callback=keyframes_callback,
                      tolerance=1,
                      exercise_serialization=False)
    @scene(s0=scene.Bool(), s1=scene.Bool(), s2=scene.Bool())
    def scene_func(cfg, s0_enabled=True, s1_enabled=True, s2_enabled=True):
        cfg.aspect_ratio = (1, 1)
        switch0.set_enabled(s0_enabled)
        switch1.set_enabled(s1_enabled)
        switch2.set_enabled(s2_enabled)
        return ngl.Group(children=(switch0, switch1, switch2))

    return scene_func


userlive_switch = _get_userlive_switch_func()
