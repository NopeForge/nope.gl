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

from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.toolbox.colors import COLORS

import pynodegl as ngl


def _render_quad(corner=(-1, -1, 0), width=(2, 0, 0), height=(0, 2, 0), color=(1, 1, 1), opacity=1.0):
    quad = ngl.Quad(corner, width, height)
    return ngl.RenderColor(color, opacity=opacity, geometry=quad, blending="src_over")


@test_fingerprint(width=16, height=16, nb_keyframes=2, tolerance=1)
@scene()
def depth_stencil_depth(cfg):
    group = ngl.Group()

    count = 4
    for i in range(count):
        depth = (i + 1) / count
        corner = (-1 + (count - 1 - i) * 2 / count, -1, depth)
        render = _render_quad(corner=corner, color=(depth, depth, depth))
        graphicconfig = ngl.GraphicConfig(
            render,
            depth_test=True,
            depth_func="lequal",
        )
        group.add_children(graphicconfig)

    for i, depth in enumerate((0.4, 0.6)):
        corner = (-1, -0.5 + 0.25 * i, depth)
        height = (0, 1 - 0.25 * i * 2, 0)
        render = _render_quad(corner=corner, height=height, color=COLORS.red, opacity=0.5)
        graphicconfig = ngl.GraphicConfig(
            render,
            depth_test=True,
            depth_func="less",
            depth_write_mask=0,
        )
        group.add_children(graphicconfig)

    return group


@test_fingerprint(width=16, height=16, nb_keyframes=2, tolerance=1)
@scene()
def depth_stencil_stencil(cfg):
    group = ngl.Group()

    count = 4
    for i in range(count):
        render = _render_quad(corner=(-1 + (i * 2) / count, -1, 0), color=COLORS.black)
        graphicconfig = ngl.GraphicConfig(
            render,
            color_write_mask="",
            stencil_test=True,
            stencil_write_mask=0xFF,
            stencil_func="always",
            stencil_ref=1,
            stencil_read_mask=0xFF,
            stencil_fail="incr",
            stencil_depth_fail="incr",
            stencil_depth_pass="incr",
        )
        group.add_children(graphicconfig)

    render = _render_quad(color=COLORS.white)
    graphicconfig = ngl.GraphicConfig(
        render,
        stencil_test=True,
        stencil_write_mask=0x0,
        stencil_func="equal",
        stencil_ref=1,
        stencil_read_mask=0x1,
        stencil_fail="keep",
        stencil_depth_fail="keep",
        stencil_depth_pass="keep",
    )
    group.add_children(graphicconfig)

    return group
