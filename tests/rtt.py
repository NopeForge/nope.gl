#
# Copyright 2023 Nope Forge
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

import array

import pynopegl as ngl
from pynopegl_utils.misc import get_shader
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.cuepoints_utils import get_grid_points, get_points_nodes
from pynopegl_utils.toolbox.colors import COLORS


def _get_cube():
    # fmt: off

    # front
    p0 = (-1, -1, 1)
    p1 = ( 1, -1, 1)
    p2 = ( 1,  1, 1)
    p3 = (-1,  1, 1)

    # back
    p4 = (-1, -1, -1)
    p5 = ( 1, -1, -1)
    p6 = ( 1,  1, -1)
    p7 = (-1,  1, -1)

    cube_vertices_data = array.array(
        "f",
        p0 + p1 + p2 + p2 + p3 + p0 +  # front
        p1 + p5 + p6 + p6 + p2 + p1 +  # right
        p7 + p6 + p5 + p5 + p4 + p7 +  # back
        p4 + p0 + p3 + p3 + p7 + p4 +  # left
        p4 + p5 + p1 + p1 + p0 + p4 +  # bottom
        p3 + p2 + p6 + p6 + p7 + p3    # top
    )
    # fmt: on

    uvs = (0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1)
    cube_uvs_data = array.array("f", uvs * 6)

    up = (0, 1, 0)
    down = (0, -1, 0)
    front = (0, 0, 1)
    back = (0, 0, -1)
    left = (-1, 0, 0)
    right = (1, 0, 0)

    cube_normals_data = array.array("f", front * 6 + right * 6 + back * 6 + left * 6 + down * 6 + up * 6)

    return ngl.Geometry(
        vertices=ngl.BufferVec3(data=cube_vertices_data),
        uvcoords=ngl.BufferVec2(data=cube_uvs_data),
        normals=ngl.BufferVec3(data=cube_normals_data),
    )


_RENDER_CUBE_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_normal = ngl_normal_matrix * ngl_normal;
}
"""


_RENDER_CUBE_FRAG = """
void main()
{
    ngl_out_color = vec4((var_normal + 1.0) / 2.0, 1.0);
}
"""


def _get_cube_scene(cfg: ngl.SceneCfg, depth_test=True, stencil_test=False):
    cube = _get_cube()
    program = ngl.Program(vertex=_RENDER_CUBE_VERT, fragment=_RENDER_CUBE_FRAG)
    program.update_vert_out_vars(var_normal=ngl.IOVec3())
    draw = ngl.Draw(cube, program)
    draw = ngl.Scale(draw, (0.5, 0.5, 0.5))

    for i in range(3):
        rot_animkf = ngl.AnimatedFloat(
            [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))]
        )
        axis = tuple(int(i == x) for x in range(3))
        assert len(axis) == 3
        draw = ngl.Rotate(draw, axis=axis, angle=rot_animkf)

    config = ngl.GraphicConfig(draw, depth_test=depth_test, stencil_test=stencil_test)

    return ngl.Camera(
        config,
        eye=(0.0, 0.0, 3.0),
        center=(0.0, 0.0, 0.0),
        up=(0.0, 1.0, 0.0),
        perspective=(45.0, cfg.aspect_ratio_float),
        clipping=(1.0, 10.0),
    )


_RENDER_DEPTH = """
void main()
{
    float depth = ngl_texvideo(tex0, var_tex0_coord).r;
    ngl_out_color = vec4(depth, depth, depth, 1.0);
}
"""


def _get_rtt_scene(
    cfg: ngl.SceneCfg,
    depth_test=True,
    stencil_test=False,
    texture_ds_format=None,
    samples=0,
    implicit=False,
    resizable=False,
    mipmap_filter="none",
    sample_depth=False,
):
    cfg.duration = 10
    cfg.aspect_ratio = (1, 1)

    scene = _get_cube_scene(cfg, depth_test, stencil_test)

    size = 0 if resizable else 1024
    texture = ngl.Texture2D(
        width=size,
        height=size,
        min_filter="linear",
        mag_filter="nearest",
        mipmap_filter=mipmap_filter,
    )

    if implicit:
        assert texture_ds_format == None
        assert samples == 0
        assert sample_depth == False
        texture.set_data_src(scene)
        texture.set_clear_color(0, 0, 0, 1)
        return ngl.DrawTexture(texture)

    texture_depth = None
    if texture_ds_format:
        texture_depth = ngl.Texture2D(
            width=size,
            height=size,
            format=texture_ds_format,
            min_filter="nearest",
            mag_filter="nearest",
        )

    rtt = ngl.RenderToTexture(
        scene,
        [texture],
        depth_texture=texture_depth,
        samples=samples,
        clear_color=(0, 0, 0, 1),
    )

    if sample_depth:
        quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
        program = ngl.Program(vertex=get_shader("texture.vert"), fragment=_RENDER_DEPTH)
        program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
        draw = ngl.Draw(quad, program)
        draw.update_frag_resources(tex0=texture_depth)
    else:
        draw = ngl.DrawTexture(texture)
    return ngl.Group(children=[rtt, draw])


def _get_rtt_function(**kwargs):
    tolerance = 0
    if kwargs.get("sample_depth", False):
        tolerance = 2

    @test_fingerprint(width=512, height=512, keyframes=10, tolerance=tolerance)
    @ngl.scene()
    def rtt_function(cfg: ngl.SceneCfg):
        return _get_rtt_scene(cfg, **kwargs)

    return rtt_function


_rtt_tests = dict(
    depth=dict(depth_test=True),
    depth_stencil=dict(depth_test=True, stencil_test=True),
    depth_stencil_resizable=dict(depth_test=True, stencil_test=True, resizable=True),
    depth_msaa=dict(depth_test=True, samples=4),
    depth_stencil_msaa=dict(depth_test=True, stencil_test=True, samples=4),
    depth_stencil_msaa_resizable=dict(depth_test=True, stencil_test=True, samples=4, resizable=True),
    depth_implicit=dict(depth_test=True, implicit=True),
    depth_stencil_implicit=dict(depth_test=True, stencil_test=True, implicit=True),
    depth_stencil_implicit_resizable=dict(depth_test=True, stencil_test=True, implicit=True, resizable=True),
    depth_texture=dict(texture_ds_format="auto_depth"),
    depth_stencil_texture=dict(texture_ds_format="auto_depth_stencil"),
    depth_stencil_texture_resizable=dict(texture_ds_format="auto_depth_stencil", resizable=True),
    depth_texture_msaa=dict(texture_ds_format="auto_depth", samples=4),
    depth_stencil_texture_msaa=dict(texture_ds_format="auto_depth_stencil", samples=4),
    depth_stencil_texture_msaa_resizable=dict(texture_ds_format="auto_depth_stencil", samples=4, resizable=True),
    mipmap=dict(depth_test=True, mipmap_filter="linear"),
    mipmap_resizable=dict(depth_test=True, resizable=True, mipmap_filter="linear"),
    sample_depth=dict(texture_ds_format="auto_depth", sample_depth=True),
    sample_depth_msaa=dict(texture_ds_format="auto_depth", sample_depth=True, samples=4),
)


for name, params in _rtt_tests.items():
    globals()["rtt_" + name] = _get_rtt_function(**params)


def _rtt_load_attachment(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    background = ngl.DrawColor(COLORS.white)
    draw = ngl.DrawColor(COLORS.orange)

    texture = ngl.Texture2D(width=16, height=16, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(draw, [texture])

    texture_noop = ngl.Texture2D(width=16, height=16, min_filter="nearest", mag_filter="nearest")
    rtt_noop = ngl.RenderToTexture(draw, [texture_noop])

    quad = ngl.Quad((0, 0, 0), (1, 0, 0), (0, 1, 0))
    foreground = ngl.DrawTexture(texture, geometry=quad)

    return ngl.Group(children=[background, rtt, rtt_noop, foreground])


@test_cuepoints(width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1)
@ngl.scene()
def rtt_load_attachment(cfg: ngl.SceneCfg):
    return _rtt_load_attachment(cfg)


@test_cuepoints(
    samples=4, width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1
)
@ngl.scene()
def rtt_load_attachment_msaa(cfg: ngl.SceneCfg):
    return _rtt_load_attachment(cfg)


def _rtt_load_attachment_nested(cfg: ngl.SceneCfg, samples=0):
    scene = _rtt_load_attachment(cfg)

    texture = ngl.Texture2D(width=16, height=16, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(scene, [texture], samples=samples)

    foreground = ngl.DrawTexture(texture)

    return ngl.Group(children=[rtt, foreground])


@test_cuepoints(width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1)
@ngl.scene()
def rtt_load_attachment_nested(cfg: ngl.SceneCfg):
    return _rtt_load_attachment_nested(cfg)


@test_cuepoints(width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1)
@ngl.scene()
def rtt_load_attachment_nested_msaa(cfg: ngl.SceneCfg):
    return _rtt_load_attachment_nested(cfg, 4)


def _rtt_load_attachment_implicit(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    background = ngl.DrawColor(COLORS.white)

    foreground = ngl.DrawTexture(
        ngl.Texture2D(
            data_src=ngl.DrawColor(COLORS.orange),
            min_filter="nearest",
            mag_filter="nearest",
        ),
        geometry=ngl.Quad((0, 0, 0), (1, 0, 0), (0, 1, 0)),
    )

    return ngl.Group(children=[background, foreground])


@test_cuepoints(width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1)
@ngl.scene()
def rtt_load_attachment_implicit(cfg: ngl.SceneCfg):
    return _rtt_load_attachment_implicit(cfg)


@test_cuepoints(
    samples=4, width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1
)
@ngl.scene()
def rtt_load_attachment_msaa_implicit(cfg: ngl.SceneCfg):
    return _rtt_load_attachment_implicit(cfg)


def _rtt_load_attachment_nested_implicit(cfg: ngl.SceneCfg, samples=0):
    scene = _rtt_load_attachment_implicit(cfg)

    texture = ngl.Texture2D(width=16, height=16, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(scene, [texture], samples=samples)

    foreground = ngl.DrawTexture(texture)

    return ngl.Group(children=[rtt, foreground])


@test_cuepoints(width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1)
@ngl.scene()
def rtt_load_attachment_nested_implicit(cfg: ngl.SceneCfg):
    return _rtt_load_attachment_nested_implicit(cfg)


@test_cuepoints(width=32, height=32, points={"bottom-left": (-0.5, -0.5), "top-right": (0.5, 0.5)}, tolerance=1)
@ngl.scene()
def rtt_load_attachment_nested_msaa_implicit(cfg: ngl.SceneCfg):
    return _rtt_load_attachment_nested_implicit(cfg, 4)


@test_fingerprint(width=512, height=512, keyframes=10, tolerance=3)
@ngl.scene()
def rtt_clear_attachment_with_timeranges(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    # Time-disabled full screen white quad
    draw = ngl.DrawColor(COLORS.white)

    time_range_filter = ngl.TimeRangeFilter(draw, end=0)

    # Intermediate no-op RTT to force the use of a different draw pass internally
    texture = ngl.Texture2D(width=32, height=32, min_filter="nearest", mag_filter="nearest")
    rtt_noop = ngl.RenderToTexture(ngl.Identity(), [texture])

    # Centered rotating quad
    quad = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    draw = ngl.DrawColor(COLORS.orange, geometry=quad)

    animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, -360)]
    draw = ngl.Rotate(draw, angle=ngl.AnimatedFloat(animkf))

    group = ngl.Group(children=[time_range_filter, rtt_noop, draw])

    # Root RTT
    texture = ngl.Texture2D(width=512, height=512, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(group, [texture])

    # Full screen draw of the root RTT result
    draw = ngl.DrawTexture(texture)

    return ngl.Group(children=[rtt, draw])


@test_fingerprint(width=512, height=512, keyframes=10, tolerance=3)
@ngl.scene()
def rtt_shared_subgraph_implicit(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    scene = _get_cube_scene(cfg)

    texture = ngl.Texture2D(format="r8g8_unorm", data_src=scene)
    draw = ngl.DrawTexture(texture)

    # The order here is important as we want to test that the render target
    # (texture) internally setup a dedicated rnode and that the rnode used by
    # the draw node is not overridden by the subgraph
    group = ngl.Group(children=[scene, draw])

    return group


@test_fingerprint(width=512, height=512, keyframes=10, tolerance=3)
@ngl.scene()
def rtt_resizable_with_timeranges(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 10

    scene = _get_cube_scene(cfg)

    texture = ngl.Texture2D()
    rtt = ngl.RenderToTexture(scene, color_textures=[texture])
    draw = ngl.DrawTexture(texture)
    group = ngl.Group(children=[rtt, draw])

    time_range_filter1 = ngl.TimeRangeFilter(group, start=0, end=1)
    time_range_filter2 = ngl.TimeRangeFilter(group, start=5)

    group = ngl.Group(children=[time_range_filter1, time_range_filter2])

    return group


@test_fingerprint(width=512, height=512, keyframes=10, tolerance=3)
@ngl.scene()
def rtt_resizable_with_timeranges_implicit(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 10

    scene = _get_cube_scene(cfg)

    texture = ngl.Texture2D(data_src=scene)
    draw = ngl.DrawTexture(texture)

    time_range_filter1 = ngl.TimeRangeFilter(draw, start=0, end=1)
    time_range_filter2 = ngl.TimeRangeFilter(draw, start=5)

    group = ngl.Group(children=[time_range_filter1, time_range_filter2])

    return group


@test_cuepoints(width=32, height=32, points=get_grid_points(8, 1), tolerance=1)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def rtt_srgb(cfg: ngl.SceneCfg, show_dbg_points=False):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 1

    gradient = ngl.DrawGradient(
        color0=(0, 0, 0),
        color1=(1, 1, 1),
    )
    texture = ngl.Texture2D(
        format="r8g8b8a8_srgb",
        width=32,
        height=32,
        data_src=gradient,
    )
    group = ngl.Group(children=[ngl.DrawTexture(texture)])

    if show_dbg_points:
        cuepoints = get_grid_points(8, 1)
        group.add_children(get_points_nodes(cfg, cuepoints))

    return group
