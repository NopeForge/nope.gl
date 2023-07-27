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

import array
import itertools
import textwrap

from pynopegl_utils.misc import SceneCfg, scene
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.toolbox.colors import COLORS, get_random_color_buffer
from pynopegl_utils.toolbox.grid import autogrid_simple
from pynopegl_utils.toolbox.shapes import equilateral_triangle_coords

import pynopegl as ngl


@test_cuepoints(points=dict(bl=(-1, -1), br=(1, -1), tr=(1, 1), tl=(-1, 1), c=(0, 0)), tolerance=5)
@scene()
def shape_precision_iovar(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)
    vert = textwrap.dedent(
        """\
        void main()
        {
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
            color = vec4((ngl_out_pos.xy + 1.0) * .5, 1.0 - (ngl_out_pos.x + 1.0) * .5 - (ngl_out_pos.y + 1.0) * .5, 1.0);
        }
        """
    )
    frag = textwrap.dedent(
        """\
        void main()
        {
            ngl_out_color = color;
        }
        """
    )
    program = ngl.Program(vertex=vert, fragment=frag)
    program.update_vert_out_vars(color=ngl.IOVec4(precision_out="high", precision_in="low"))
    geometry = ngl.Quad(corner=(-1, -1, 0), width=(2, 0, 0), height=(0, 2, 0))
    scene = ngl.Render(geometry, program)
    return scene


def _render_shape(geometry, color):
    return ngl.RenderColor(color, geometry=geometry)


@test_fingerprint()
@scene(sz=scene.Range(range=[0.1, 2], unit_base=100), color=scene.Color())
def shape_triangle(cfg: SceneCfg, sz=1, color=COLORS.orange):
    cfg.aspect_ratio = (1, 1)
    p0, p1, p2 = equilateral_triangle_coords(sz)
    geometry = ngl.Triangle(p0, p1, p2)
    return _render_shape(geometry, color)


@test_fingerprint(samples=4)
@scene(sz=scene.Range(range=[0.1, 2], unit_base=100), color=scene.Color())
def shape_triangle_msaa(cfg: SceneCfg, sz=1, color=COLORS.orange):
    cfg.aspect_ratio = (1, 1)
    p0, p1, p2 = equilateral_triangle_coords(sz)
    geometry = ngl.Triangle(p0, p1, p2)
    return _render_shape(geometry, color)


@test_fingerprint()
@scene(
    corner=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    width=scene.Vector(n=3, minv=(0, 0, 0), maxv=(2, 2, 2)),
    height=scene.Vector(n=3, minv=(0, 0, 0), maxv=(2, 2, 2)),
    color=scene.Color(),
)
def shape_quad(cfg: SceneCfg, corner=(-0.5, -0.8, 0), width=(0.9, 0.2, 0), height=(0.1, 1.3, 0), color=COLORS.sgreen):
    cfg.aspect_ratio = (1, 1)
    geometry = ngl.Quad(corner, width, height)
    return _render_shape(geometry, color)


@test_fingerprint()
@scene(radius=scene.Range(range=[0.1, 2], unit_base=100), color=scene.Color())
def shape_circle(cfg: SceneCfg, radius=0.5, color=COLORS.azure):
    cfg.aspect_ratio = (1, 1)
    geometry = ngl.Circle(radius, npoints=64)
    return _render_shape(geometry, color)


def _shape_geometry(cfg: SceneCfg, set_normals=False, set_indices=False):
    # Fake cube (3 faces only) obtained from:
    # echo 'cube();'>x.scad; openscad x.scad -o x.stl
    vertices = array.array(
        "f",
        [
            x - 0.5
            for x in [
                # fmt: off
                1,1,0, 0,1,1, 1,1,1,
                0,1,1, 1,1,0, 0,1,0,
                0,0,0, 0,1,1, 0,1,0,
                0,1,1, 0,0,0, 0,0,1,
                0,1,1, 1,0,1, 1,1,1,
                1,0,1, 0,1,1, 0,0,1,
                # fmt: on
            ]
        ],
    )

    normals = array.array(
        "f",
        [
            # fmt: off
            0,1,0,  0,1,0,  0,1,0,
            0,1,0,  0,1,0,  0,1,0,
           -1,0,0, -1,0,0, -1,0,0,
           -1,0,0, -1,0,0, -1,0,0,
            0,0,1,  0,0,1,  0,0,1,
            0,0,1,  0,0,1,  0,0,1,
            # fmt: on
        ],
    )

    vertices_buffer = ngl.BufferVec3(data=vertices)
    normals_buffer = ngl.BufferVec3(data=normals)

    cfg.aspect_ratio = (1, 1)
    geometry = ngl.Geometry(vertices=vertices_buffer)

    if set_normals:
        geometry.set_normals(normals_buffer)
        prog = ngl.Program(vertex=cfg.get_vert("colored-normals"), fragment=cfg.get_frag("colored-normals"))
        prog.update_vert_out_vars(var_normal=ngl.IOVec3())
        render = ngl.Render(geometry, prog)
    else:
        prog = ngl.Program(vertex=cfg.get_vert("color"), fragment=cfg.get_frag("color"))
        render = ngl.Render(geometry, prog)
        render.update_frag_resources(color=ngl.UniformVec3(value=COLORS.magenta), opacity=ngl.UniformFloat(1))

    if set_indices:
        indices = array.array("H", list(range(3 * 6)))
        indices_buffer = ngl.BufferUShort(data=indices)
        geometry.set_indices(indices_buffer)

    return ngl.Rotate(render, 45, axis=(1, 1, 1))


@test_fingerprint()
@scene()
def shape_geometry(cfg: SceneCfg):
    return _shape_geometry(cfg, set_normals=False, set_indices=False)


@test_fingerprint()
@scene()
def shape_geometry_normals(cfg: SceneCfg):
    return _shape_geometry(cfg, set_normals=True, set_indices=False)


@test_fingerprint()
@scene()
def shape_geometry_indices(cfg: SceneCfg):
    return _shape_geometry(cfg, set_normals=False, set_indices=True)


@test_fingerprint()
@scene()
def shape_geometry_normals_indices(cfg: SceneCfg):
    return _shape_geometry(cfg, set_normals=True, set_indices=True)


@test_fingerprint()
@scene()
def shape_geometry_with_renderother(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)

    vertices_data = array.array("f")
    uvcoords_data = array.array("f")
    boundaries = (
        (-1, 0, 0, 1),
        (0, 1, 0, 1),
        (-1, 0, -1, 0),
        (0, 1, -1, 0),
    )
    for xmin, xmax, ymin, ymax in boundaries:
        x0, y0 = cfg.rng.uniform(xmin, xmax), cfg.rng.uniform(ymin, ymax)
        x1, y1 = cfg.rng.uniform(xmin, xmax), cfg.rng.uniform(ymin, ymax)
        vertices_data.extend((0, 0, 0, x0, y0, 0, x1, y1, 0))
        uvcoords_data.extend((0, 0, x0, y0, x1, y1))

    geometry = ngl.Geometry(
        vertices=ngl.BufferVec3(data=vertices_data),
        uvcoords=ngl.BufferVec2(data=uvcoords_data),
        topology="triangle_list",
    )

    return ngl.RenderColor(geometry=geometry)


@test_fingerprint()
@scene()
def shape_diamond_colormask(_):
    color_write_masks = ("r+g+b+a", "r+g+a", "g+b+a", "r+b+a")
    geometry = ngl.Circle(npoints=5)
    render = ngl.RenderColor(COLORS.white, geometry=geometry)
    scenes = [ngl.GraphicConfig(render, color_write_mask=cwm) for cwm in color_write_masks]
    return autogrid_simple(scenes)


def _get_morphing_coordinates(rng, n, x_off, y_off):
    coords = [(rng.uniform(0, 1) + x_off, rng.uniform(0, 1) + y_off, 0) for _ in range(n - 1)]
    coords.append(coords[0])  # smooth loop
    return coords


@test_fingerprint(keyframes=8, tolerance=1)
@scene(n=scene.Range(range=[2, 50]))
def shape_morphing(cfg: SceneCfg, n=6):
    cfg.duration = 5.0
    vertices_tl = _get_morphing_coordinates(cfg.rng, n, -1, 0)
    vertices_tr = _get_morphing_coordinates(cfg.rng, n, 0, 0)
    vertices_bl = _get_morphing_coordinates(cfg.rng, n, -1, -1)
    vertices_br = _get_morphing_coordinates(cfg.rng, n, 0, -1)

    vertices_animkf = []
    for i, coords in enumerate(zip(vertices_tl, vertices_tr, vertices_bl, vertices_br)):
        flat_coords = list(itertools.chain(*coords))
        coords_array = array.array("f", flat_coords)
        vertices_animkf.append(ngl.AnimKeyFrameBuffer(i * cfg.duration / (n - 1), coords_array))
    vertices = ngl.AnimatedBufferVec3(vertices_animkf)

    geom = ngl.Geometry(vertices)
    geom.set_topology("triangle_strip")
    p = ngl.Program(vertex=cfg.get_vert("color"), fragment=cfg.get_frag("color"))
    render = ngl.Render(geom, p)
    render.update_frag_resources(color=ngl.UniformVec3(COLORS.cyan), opacity=ngl.UniformFloat(1))
    return render


def _get_cropboard_function(set_indices=False):
    @test_fingerprint(keyframes=10, tolerance=1)
    @scene(dim_clr=scene.Range(range=[1, 50]), dim_cut=scene.Range(range=[1, 50]))
    def cropboard(cfg: SceneCfg, dim_clr=3, dim_cut=9):
        cfg.duration = 5.0 + 1.0

        nb_kf = 2
        buffers = [get_random_color_buffer(cfg.rng, dim_clr) for _ in range(nb_kf)]
        random_animkf = []
        time_scale = cfg.duration / float(nb_kf)
        for i, buf in enumerate(buffers + [buffers[0]]):
            random_animkf.append(ngl.AnimKeyFrameBuffer(i * time_scale, buf))
        random_buffer = ngl.AnimatedBufferVec4(keyframes=random_animkf)
        random_tex = ngl.Texture2D(data_src=random_buffer, width=dim_clr, height=dim_clr)

        kw = kh = 1.0 / dim_cut
        qw = qh = 2.0 / dim_cut

        p = ngl.Program(vertex=cfg.get_vert("cropboard"), fragment=cfg.get_frag("texture"))
        p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())

        uv_offset_buffer = array.array("f")
        translate_a_buffer = array.array("f")
        translate_b_buffer = array.array("f")

        if set_indices:
            indices = array.array("H", [0, 2, 1, 1, 3, 0])
            indices_buffer = ngl.BufferUShort(data=indices)

            vertices = array.array(
                "f",
                [
                    # fmt: off
                     0, 0,  0,
                    qw, qh, 0,
                    qw, 0,  0,
                     0, qh, 0,
                    # fmt: on
                ],
            )

            uvcoords = array.array(
                "f",
                [
                    # fmt: off
                     0, 1.0,
                    kw, 1.0 - kh,
                    kw, 1.0,
                     0, 1.0 - kh,
                    # fmt: on
                ],
            )

            vertices_buffer = ngl.BufferVec3(data=vertices)
            uvcoords_buffer = ngl.BufferVec2(data=uvcoords)

            q = ngl.Geometry(vertices=vertices_buffer, uvcoords=uvcoords_buffer, indices=indices_buffer)
        else:
            q = ngl.Quad(
                corner=(0, 0, 0),
                width=(qw, 0, 0),
                height=(0, qh, 0),
                uv_corner=(0, 0),
                uv_width=(kw, 0),
                uv_height=(0, kh),
            )

        for y in range(dim_cut):
            for x in range(dim_cut):
                uv_offset = [x * kw, (y + 1.0) * kh - 1.0]
                src = [cfg.rng.uniform(-2, 2), cfg.rng.uniform(-2, 2)]
                dst = [x * qw - 1.0, 1.0 - (y + 1.0) * qh]

                uv_offset_buffer.extend(uv_offset)
                translate_a_buffer.extend(src)
                translate_b_buffer.extend(dst)

        utime_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration - 1.0, 1)]
        utime = ngl.AnimatedFloat(utime_animkf)

        render = ngl.Render(q, p, nb_instances=dim_cut**2)
        render.update_frag_resources(tex0=random_tex)
        render.update_vert_resources(time=utime)
        render.update_instance_attributes(
            uv_offset=ngl.BufferVec2(data=uv_offset_buffer),
            translate_a=ngl.BufferVec2(data=translate_a_buffer),
            translate_b=ngl.BufferVec2(data=translate_b_buffer),
        )

        return render

    return cropboard


shape_cropboard = _get_cropboard_function(set_indices=False)
shape_cropboard_indices = _get_cropboard_function(set_indices=True)


TRIANGLES_MAT4_ATTRIBUTE_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * matrix * vec4(ngl_position, 1.0);
}
"""


@test_fingerprint()
@scene()
def shape_triangles_mat4_attribute(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)
    p0, p1, p2 = equilateral_triangle_coords(1)
    geometry = ngl.Triangle(p0, p1, p2)
    matrices = ngl.BufferMat4(
        data=array.array(
            "f",
            [
                # fmt: off
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
               -0.5, 0.0, 0.0, 1.0,

                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.5, 0.0, 0.0, 1.0,
                # fmt: on
            ],
        )
    )

    program = ngl.Program(
        vertex=TRIANGLES_MAT4_ATTRIBUTE_VERT,
        fragment=cfg.get_frag("color"),
    )
    render = ngl.Render(geometry, program, nb_instances=2)
    render.update_instance_attributes(matrix=matrices)
    render.update_frag_resources(color=ngl.UniformVec3(value=COLORS.orange), opacity=ngl.UniformFloat(1))
    return render


def _get_shape_scene(cfg: SceneCfg, shape, cull_mode):
    cfg.aspect_ratio = (1, 1)

    geometry_cls = dict(
        triangle=ngl.Triangle,
        quad=ngl.Quad,
        circle=ngl.Circle,
    )
    geometry = geometry_cls[shape]()

    node = _render_shape(geometry, COLORS.sgreen)
    return ngl.GraphicConfig(node, cull_mode=cull_mode)


def _get_shape_function(shape, cull_mode):
    @test_fingerprint()
    @scene()
    def shape_function(cfg: SceneCfg):
        return _get_shape_scene(cfg, shape, cull_mode)

    return shape_function


for shape in ("triangle", "quad", "circle"):
    for cull_mode in ("front", "back"):
        globals()["shape_" + shape + "_cull_" + cull_mode] = _get_shape_function(shape, cull_mode)
