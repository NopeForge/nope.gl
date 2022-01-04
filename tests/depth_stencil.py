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


def _render_quad(cfg, corner=(-1, -1, 0), width=(2, 0, 0), height=(0, 2, 0), color=(1, 1, 1, 1)):
    quad = ngl.Quad(corner, width, height)
    program = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(color=ngl.UniformVec4(value=color))
    return render


@test_fingerprint(width=16, height=16, nb_keyframes=2, tolerance=1)
@scene()
def depth_stencil_depth(cfg):
    group = ngl.Group()

    count = 4
    for i in range(count):
        depth = (i + 1) / count
        corner = (-1 + (count - 1 - i) * 2 / count, -1, depth)
        render = _render_quad(cfg, corner=corner, color=(depth, depth, depth, 1))
        graphicconfig = ngl.GraphicConfig(
            render,
            depth_test=True,
            depth_func='lequal',
        )
        group.add_children(graphicconfig)

    for i, depth in enumerate((0.4, 0.6)):
        corner = (-1, -0.5 + 0.25 * i, depth)
        height = (0, 1 - 0.25 * i * 2, 0)
        render = _render_quad(cfg, corner=corner, height=height, color=(0.5, 0, 0, 0.5))
        graphicconfig = ngl.GraphicConfig(
            render,
            blend=True,
            blend_src_factor='one',
            blend_src_factor_a='one',
            blend_dst_factor='one_minus_src_alpha',
            blend_dst_factor_a='one_minus_src_alpha',
            depth_test=True,
            depth_func='less',
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
        render = _render_quad(cfg, corner=(-1 + (i * 2) / count, -1, 0), color=(0, 0, 0, 1))
        graphicconfig = ngl.GraphicConfig(
            render,
            color_write_mask='',
            stencil_test=True,
            stencil_write_mask=0xff,
            stencil_func='always',
            stencil_ref=1,
            stencil_read_mask=0xff,
            stencil_fail='incr',
            stencil_depth_fail='incr',
            stencil_depth_pass='incr',
        )
        group.add_children(graphicconfig)

    render = _render_quad(cfg, color=COLORS.white)
    graphicconfig = ngl.GraphicConfig(
        render,
        stencil_test=True,
        stencil_write_mask=0x0,
        stencil_func='equal',
        stencil_ref=1,
        stencil_read_mask=0x1,
        stencil_fail='keep',
        stencil_depth_fail='keep',
        stencil_depth_pass='keep',
    )
    group.add_children(graphicconfig)

    return group
