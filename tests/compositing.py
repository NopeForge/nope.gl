#
# Copyright 2021 GoPro Inc.
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
from pynodegl_utils.toolbox.grid import autogrid_simple

_OPERATORS = (
    'src_over',
    'dst_over',
    'src_out',
    'dst_out',
    'src_in',
    'dst_in',
    'src_atop',
    'dst_atop',
    'xor',
)

_VERTEX = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    uv = (ngl_uvcoord - .5) * 2.;
}
'''

_FRAGMENT = '''
void main() {
    float sd = length(uv + off) - 0.5; // signed distance to a circle of radius 0.5
    ngl_out_color = color * step(sd, 0.0);
}
'''


def _get_compositing_scene(cfg, op, show_label=False):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 6

    # We can not use a circle geometry because the whole areas must be
    # rasterized for the compositing to work, so instead we build 2 overlapping
    # quad into which we draw colored circles, offsetted with an animation.
    # Alternatively, we could use a RTT.
    quad = ngl.Quad(corner=(-1, -1, 0), width=(2, 0, 0), height=(0, 2, 0))
    prog = ngl.Program(vertex=_VERTEX, fragment=_FRAGMENT)
    prog.update_vert_out_vars(uv=ngl.IOVec2())

    A_off_kf = (
        ngl.AnimKeyFrameVec2(0, (-1/3, 0)),
        ngl.AnimKeyFrameVec2(cfg.duration/2, (1/3, 0)),
        ngl.AnimKeyFrameVec2(cfg.duration, (-1/3, 0)),
    )
    B_off_kf = (
        ngl.AnimKeyFrameVec2(0, (1/3, 0)),
        ngl.AnimKeyFrameVec2(cfg.duration/2, (-1/3, 0)),
        ngl.AnimKeyFrameVec2(cfg.duration, (1/3, 0)),
    )
    A_off = ngl.AnimatedVec2(A_off_kf)
    B_off = ngl.AnimatedVec2(B_off_kf)

    A = ngl.Render(quad, prog, label='A')
    A.update_frag_resources(color=ngl.UniformVec4(value=COLORS.azure), off=A_off)

    B = ngl.Render(quad, prog, label='B', blending=op)
    B.update_frag_resources(color=ngl.UniformVec4(value=COLORS.orange), off=B_off)

    bg = ngl.RenderColor(blending='dst_over')

    # draw A in current FBO, then draw B with the current operator, and
    # then result goes over the white background
    ret = ngl.Group(children=(A, B, bg))

    if show_label:
        label_h = 1/4
        label_pad = .1
        label = ngl.Text(
            op,
            fg_color=COLORS.black,
            bg_color=(.8, .8, .8, 1),
            box_corner=(label_pad/2 - 1, 1 - label_h - label_pad/2, 0),
            box_width=(2 - label_pad, 0, 0),
            box_height=(0, label_h, 0),
        )
        ret.add_children(label)

    return ret


def _get_compositing_func(op):
    @test_fingerprint(nb_keyframes=10, tolerance=1)
    @scene()
    def scene_func(cfg):
        return _get_compositing_scene(cfg, op)
    return scene_func


@scene()
def compositing_all_operators(cfg):
    scenes = []
    for op in _OPERATORS:
        scene = _get_compositing_scene(cfg, op, show_label=True)
        scenes.append(scene)
    return autogrid_simple(scenes)


for operator in _OPERATORS:
    globals()[f'compositing_{operator}'] = _get_compositing_func(operator)
