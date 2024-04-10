#
# Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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
from textwrap import dedent

from pynopegl_utils.misc import get_shader, load_media
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.cuepoints_utils import get_grid_points, get_points_nodes
from pynopegl_utils.toolbox.colors import COLORS, get_random_color_buffer

import pynopegl as ngl

_RENDER_BUFFER_FRAG = """
void main()
{
    float color = texture(tex0, var_tex0_coord).r;
    ngl_out_color = vec4(color, 0.0, 0.0, 1.0);
}
"""


def _draw_buffer(cfg: ngl.SceneCfg, w, h):
    n = w * h
    data = array.array("B", [i * 255 // n for i in range(n)])
    buf = ngl.BufferUByte(data=data)
    texture = ngl.Texture2D(width=w, height=h, data_src=buf, min_filter="nearest", mag_filter="nearest")
    program = ngl.Program(vertex=get_shader("texture.vert"), fragment=_RENDER_BUFFER_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    draw = ngl.Draw(ngl.Quad(), program)
    draw.update_frag_resources(tex0=texture)
    return draw


@test_fingerprint(width=128, height=128)
@ngl.scene(controls=dict(w=ngl.scene.Range(range=[1, 128]), h=ngl.scene.Range(range=[1, 128])))
def texture_data(cfg: ngl.SceneCfg, w=4, h=5):
    cfg.aspect_ratio = (1, 1)
    return _draw_buffer(cfg, w, h)


@test_fingerprint(width=128, height=128)
@ngl.scene(controls=dict(dim=ngl.scene.Range(range=[1, 100])))
def texture_data_animated(cfg: ngl.SceneCfg, dim=8):
    cfg.duration = 3.0
    cfg.aspect_ratio = (1, 1)
    nb_kf = int(cfg.duration)
    buffers = [get_random_color_buffer(cfg.rng, dim) for _ in range(nb_kf)]
    random_animkf = []
    time_scale = cfg.duration / float(nb_kf)
    for i, buf in enumerate(buffers + [buffers[0]]):
        random_animkf.append(ngl.AnimKeyFrameBuffer(i * time_scale, buf))
    random_buffer = ngl.AnimatedBufferVec4(keyframes=random_animkf)
    random_tex = ngl.Texture2D(
        data_src=random_buffer,
        width=dim,
        height=dim,
        min_filter="nearest",
        mag_filter="nearest",
    )
    return ngl.DrawTexture(random_tex)


@test_fingerprint(width=128, height=128)
@ngl.scene(controls=dict(h=ngl.scene.Range(range=[1, 32])))
def texture_data_unaligned_row(cfg: ngl.SceneCfg, h=32):
    """Tests upload of buffers with rows that are not 4-byte aligned"""
    cfg.aspect_ratio = (1, 1)
    return _draw_buffer(cfg, 1, h)


@test_fingerprint(width=128, height=128, keyframes=(0, 1, 0))
@ngl.scene(controls=dict(w=ngl.scene.Range(range=[1, 128]), h=ngl.scene.Range(range=[1, 128])))
def texture_data_seek_timeranges(cfg: ngl.SceneCfg, w=4, h=5):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 1
    return ngl.TimeRangeFilter(_draw_buffer(cfg, w, h), end=1)


@test_fingerprint(width=1280, height=960, keyframes=5)
@ngl.scene()
def texture_displacement(cfg: ngl.SceneCfg):
    m0 = load_media("mire")
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    m = ngl.Media(m0.filename)
    source_tex = ngl.Texture2D(data_src=m, min_filter="nearest", mag_filter="nearest")

    # Generate a displacement texture
    vert = dedent(
        """
        void main()
        {
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
            uv = ngl_uvcoord;
        }
        """
    )
    frag = dedent(
        """
        void main()
        {
            float d = (sin(uv.y * 10.0 + t) + 1.0) / 2.0 * 0.1;
            ngl_out_color = vec4(0.0, d, 0.0, 0.0);
        }
        """
    )
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=vert, fragment=frag)
    p.update_vert_out_vars(uv=ngl.IOVec2())
    displace_draw = ngl.Draw(q, p)
    displace_draw.update_frag_resources(t=ngl.Time())

    displace_tex = ngl.Texture2D(width=m0.width, height=m0.height, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(displace_draw)
    rtt.add_color_textures(displace_tex)

    draw = ngl.DrawDisplace(source_tex, displace_tex)
    return ngl.Group(children=[rtt, draw])


_RENDER_TO_CUBEMAP_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
}
"""


_RENDER_TO_CUBEMAP_FRAG = """
void main()
{
    ngl_out_color[0] = vec4(1.0, 0.0, 0.0, 1.0); // right
    ngl_out_color[1] = vec4(0.0, 1.0, 0.0, 1.0); // left
    ngl_out_color[2] = vec4(0.0, 0.0, 1.0, 1.0); // top
    ngl_out_color[3] = vec4(1.0, 1.0, 0.0, 1.0); // bottom
    ngl_out_color[4] = vec4(0.0, 1.0, 1.0, 1.0); // back
    ngl_out_color[5] = vec4(1.0, 0.0, 1.0, 1.0); // front
}
"""


_RENDER_TO_CUBEMAP_1_FRAG = """
void main()
{
    ngl_out_color[0] = vec4(1.0, 0.0, 0.0, 1.0); // right
    ngl_out_color[1] = vec4(0.0, 1.0, 0.0, 1.0); // left
}
"""


_RENDER_TO_CUBEMAP_2_FRAG = """
void main()
{
    ngl_out_color[0] = vec4(0.0, 0.0, 1.0, 1.0); // top
    ngl_out_color[1] = vec4(1.0, 1.0, 0.0, 1.0); // bottom
    ngl_out_color[2] = vec4(0.0, 1.0, 1.0, 1.0); // back
    ngl_out_color[3] = vec4(1.0, 0.0, 1.0, 1.0); // front
}
"""


_RENDER_CUBEMAP_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_position;
}
"""


_RENDER_CUBEMAP_FRAG = """
void main()
{
    ngl_out_color = texture(tex0, vec3(var_uvcoord.xy, 0.5));
}
"""


def _get_texture_cubemap_from_mrt_scene(cfg: ngl.SceneCfg, samples=0):
    cfg.aspect_ratio = (1, 1)
    program = ngl.Program(vertex=_RENDER_TO_CUBEMAP_VERT, fragment=_RENDER_TO_CUBEMAP_FRAG, nb_frag_output=6)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    draw = ngl.Draw(quad, program)
    cube = ngl.TextureCube(size=64, min_filter="linear", mag_filter="linear")
    rtt = ngl.RenderToTexture(draw, [cube], samples=samples)

    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=cube)

    return ngl.Group(children=(rtt, draw))


def _get_texture_cubemap_from_mrt_scene_2_pass(cfg: ngl.SceneCfg, samples=0):
    cfg.aspect_ratio = (1, 1)
    group = ngl.Group()
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    cube = ngl.TextureCube(size=64, min_filter="linear", mag_filter="linear")

    layer_base = 0
    for layer_count, fragment in ((2, _RENDER_TO_CUBEMAP_1_FRAG), (4, _RENDER_TO_CUBEMAP_2_FRAG)):
        program = ngl.Program(vertex=_RENDER_TO_CUBEMAP_VERT, fragment=fragment, nb_frag_output=layer_count)
        program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
        draw = ngl.Draw(quad, program)
        color_textures = [ngl.TextureView(cube, layer) for layer in range(layer_base, layer_base + layer_count)]
        rtt = ngl.RenderToTexture(draw, color_textures, samples=samples)
        group.add_children(rtt)
        layer_base += layer_count

    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=cube)
    group.add_children(draw)

    return group


def _get_texture_cubemap(mipmap_filter="none"):
    n = 64
    p = n * n
    cb_data = array.array(
        "B",
        (255, 0, 0, 255) * p
        + (0, 255, 0, 255) * p
        + (0, 0, 255, 255) * p
        + (255, 255, 0, 255) * p
        + (0, 255, 255, 255) * p
        + (255, 0, 255, 255) * p,
    )
    cb_buffer = ngl.BufferUBVec4(data=cb_data)
    cube = ngl.TextureCube(
        size=n, min_filter="linear", mag_filter="linear", data_src=cb_buffer, mipmap_filter=mipmap_filter
    )
    return cube


@test_fingerprint(width=800, height=800)
@ngl.scene()
def texture_cubemap(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cube = _get_texture_cubemap()
    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=cube)
    return draw


_RENDER_CUBEMAP_LOD_FRAG = """
void main()
{
    ngl_out_color = textureLod(tex0, vec3(var_uvcoord.xy, 0.5), 1.0);
}
"""


@test_fingerprint(width=800, height=800, tolerance=1)
@ngl.scene()
def texture_cubemap_mipmap(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cube = _get_texture_cubemap(mipmap_filter="nearest")
    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_LOD_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=cube)
    return draw


@test_fingerprint(width=800, height=800)
@ngl.scene()
def texture_cubemap_from_mrt(cfg: ngl.SceneCfg):
    return _get_texture_cubemap_from_mrt_scene(cfg)


@test_fingerprint(width=800, height=800)
@ngl.scene()
def texture_cubemap_from_mrt_msaa(cfg: ngl.SceneCfg):
    return _get_texture_cubemap_from_mrt_scene(cfg, 4)


@test_fingerprint(width=800, height=800)
@ngl.scene()
def texture_cubemap_from_mrt_2_pass(cfg: ngl.SceneCfg):
    return _get_texture_cubemap_from_mrt_scene_2_pass(cfg)


@test_fingerprint(width=800, height=800)
@ngl.scene()
def texture_cubemap_from_mrt_2_pass_msaa(cfg: ngl.SceneCfg):
    return _get_texture_cubemap_from_mrt_scene_2_pass(cfg, 4)


@test_cuepoints(width=32, height=32, points={"bottom-left": (-1, -1), "top-right": (1, 1)}, tolerance=1)
@ngl.scene()
def texture_clear_and_scissor(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    draw = ngl.DrawColor(COLORS.white)
    graphic_config = ngl.GraphicConfig(draw, scissor_test=True, scissor=(0, 0, 0, 0), color_write_mask="")

    texture = ngl.Texture2D(width=64, height=64, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(ngl.Identity(), [texture], clear_color=COLORS.orange + (1,))
    draw = ngl.DrawTexture(texture)

    return ngl.Group(children=(graphic_config, rtt, draw))


@test_fingerprint(width=64, height=64)
@ngl.scene()
def texture_scissor(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    draw = ngl.DrawColor(COLORS.orange)
    graphic_config = ngl.GraphicConfig(draw, scissor_test=True, scissor=(32, 32, 32, 32))
    texture = ngl.Texture2D(width=64, height=64, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(graphic_config, [texture], clear_color=(0, 0, 0, 1))
    draw = ngl.DrawTexture(texture)
    return ngl.Group(children=(rtt, draw))


_TEXTURE2D_ARRAY_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
}
"""


_TEXTURE2D_ARRAY_FRAG = """
void main()
{
    ngl_out_color  = texture(tex0, vec3(var_tex0_coord, 0.0));
    ngl_out_color += texture(tex0, vec3(var_tex0_coord, 1.0));
    ngl_out_color += texture(tex0, vec3(var_tex0_coord, 2.0));
}
"""


def _get_texture_2d_array(cfg: ngl.SceneCfg, mipmap_filter="none"):
    width, height, depth = 9, 9, 3
    n = width * height
    data = array.array("B")
    for i in cfg.rng.sample(range(n), n):
        data.extend([i * 255 // n, 0, 0, 255])
    for i in cfg.rng.sample(range(n), n):
        data.extend([0, i * 255 // n, 0, 255])
    for i in cfg.rng.sample(range(n), n):
        data.extend([0, 0, i * 255 // n, 255])
    texture_buffer = ngl.BufferUBVec4(data=data)
    texture = ngl.Texture2DArray(
        width=width,
        height=height,
        depth=depth,
        data_src=texture_buffer,
        mipmap_filter=mipmap_filter,
        min_filter="nearest",
        mag_filter="nearest",
    )
    return texture


@test_fingerprint(width=320, height=320)
@ngl.scene()
def texture_2d_array(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    texture = _get_texture_2d_array(cfg)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_TEXTURE2D_ARRAY_VERT, fragment=_TEXTURE2D_ARRAY_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture)
    return draw


_TEXTURE2D_ARRAY_LOD_FRAG = """
void main()
{
    ngl_out_color  = textureLod(tex0, vec3(var_tex0_coord, 0.0), 2.0);
    ngl_out_color += textureLod(tex0, vec3(var_tex0_coord, 1.0), 2.0);
    ngl_out_color += textureLod(tex0, vec3(var_tex0_coord, 2.0), 2.0);
}
"""


@test_fingerprint(width=1280, height=720, tolerance=4)
@ngl.scene()
def texture_2d_array_mipmap(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (16, 9)
    texture = _get_texture_2d_array(cfg, mipmap_filter="nearest")
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_TEXTURE2D_ARRAY_VERT, fragment=_TEXTURE2D_ARRAY_LOD_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture)
    return draw


_RENDER_TO_TEXTURE2D_ARRAY_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""

_RENDER_TO_TEXTURE2D_ARRAY_FRAG = """
void main()
{
    float x = floor(var_uvcoord.x * steps) / steps;
    x = var_uvcoord.y < 0.5 ? x : 1.0 - x;
    ngl_out_color[0] = vec4(x, 0.0, 0.0, 1.0);
    ngl_out_color[1] = vec4(0.0, x, 0.0, 1.0);
    ngl_out_color[2] = vec4(0.0, 0.0, x, 1.0);
}
"""

_STEPS = 4
_CUEPOINTS = get_grid_points(_STEPS, 2)


def _get_texture_2d_array_from_mrt_scene(cfg: ngl.SceneCfg, show_dbg_points, samples=0):
    cfg.aspect_ratio = (1, 1)

    depth = 3
    program = ngl.Program(
        vertex=_RENDER_TO_TEXTURE2D_ARRAY_VERT, fragment=_RENDER_TO_TEXTURE2D_ARRAY_FRAG, nb_frag_output=depth
    )
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(steps=ngl.UniformFloat(value=_STEPS))
    texture = ngl.Texture2DArray(width=64, height=64, depth=depth, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(draw, [texture], samples=samples)

    program = ngl.Program(vertex=_TEXTURE2D_ARRAY_VERT, fragment=_TEXTURE2D_ARRAY_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture)

    group = ngl.Group(children=(rtt, draw))
    if show_dbg_points:
        group.add_children(get_points_nodes(cfg, _CUEPOINTS))

    return group


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, tolerance=1)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def texture_2d_array_from_mrt(cfg: ngl.SceneCfg, show_dbg_points=False):
    return _get_texture_2d_array_from_mrt_scene(cfg, show_dbg_points)


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, tolerance=1)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def texture_2d_array_from_mrt_msaa(cfg: ngl.SceneCfg, show_dbg_points=False):
    return _get_texture_2d_array_from_mrt_scene(cfg, show_dbg_points, 4)


_TEXTURE3D_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
}
"""


_TEXTURE3D_FRAG = """
void main()
{
    ngl_out_color  = texture(tex0, vec3(var_tex0_coord, 0.0));
    ngl_out_color += texture(tex0, vec3(var_tex0_coord, 0.5));
    ngl_out_color += texture(tex0, vec3(var_tex0_coord, 1.0));
}
"""


@test_fingerprint(width=400, height=400)
@ngl.scene()
def texture_3d(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    width, height, depth = 9, 9, 3
    n = width * height
    data = array.array("B")
    for i in cfg.rng.sample(range(n), n):
        data.extend([i * 255 // n, 0, 0, 255])
    for i in cfg.rng.sample(range(n), n):
        data.extend([0, i * 255 // n, 0, 255])
    for i in cfg.rng.sample(range(n), n):
        data.extend([0, 0, i * 255 // n, 255])
    texture_buffer = ngl.BufferUBVec4(data=data)
    texture = ngl.Texture3D(
        width=width,
        height=height,
        depth=depth,
        data_src=texture_buffer,
        min_filter="nearest",
        mag_filter="nearest",
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_TEXTURE3D_VERT, fragment=_TEXTURE3D_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture)
    return draw


_RENDER_TO_TEXTURE3D_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""

_RENDER_TO_TEXTURE3D_FRAG = """
void main()
{
    float x = floor(var_uvcoord.x * steps) / steps;
    x = var_uvcoord.y < 0.5 ? x : 1.0 - x;
    ngl_out_color[0] = vec4(x, 0.0, 0.0, 1.0);
    ngl_out_color[1] = vec4(0.0, x, 0.0, 1.0);
    ngl_out_color[2] = vec4(0.0, 0.0, x, 1.0);
}
"""


def _get_texture_3d_from_mrt_scene(cfg: ngl.SceneCfg, show_dbg_points, samples=0):
    cfg.aspect_ratio = (1, 1)
    depth = 3
    program = ngl.Program(vertex=_RENDER_TO_TEXTURE3D_VERT, fragment=_RENDER_TO_TEXTURE3D_FRAG, nb_frag_output=depth)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(steps=ngl.UniformFloat(value=_STEPS))
    texture = ngl.Texture3D(width=64, height=64, depth=depth, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(draw, [texture], samples=samples)

    program = ngl.Program(vertex=_TEXTURE3D_VERT, fragment=_TEXTURE3D_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture)

    group = ngl.Group(children=(rtt, draw))
    if show_dbg_points:
        group.add_children(get_points_nodes(cfg, _CUEPOINTS))

    return group


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, tolerance=1)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def texture_3d_from_mrt(cfg: ngl.SceneCfg, show_dbg_points=False):
    return _get_texture_3d_from_mrt_scene(cfg, show_dbg_points)


@test_cuepoints(width=128, height=128, points=_CUEPOINTS, tolerance=1)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def texture_3d_from_mrt_msaa(cfg: ngl.SceneCfg, show_dbg_points=False):
    return _get_texture_3d_from_mrt_scene(cfg, show_dbg_points, 4)


_RENDER_TEXTURE_LOD_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""


_RENDER_TEXTURE_LOD_FRAG = """
void main()
{
    ngl_out_color = textureLod(tex0, var_uvcoord, 0.5);
}
"""


_N = 8
_MIPMAP_CUEPOINTS = get_grid_points(_N, _N)


@test_cuepoints(width=128, height=128, points=_MIPMAP_CUEPOINTS, tolerance=1)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def texture_mipmap(cfg: ngl.SceneCfg, show_dbg_points=False):
    cfg.aspect_ratio = (1, 1)
    black = (0, 0, 0, 255)
    white = (255, 255, 255, 255)
    p = _N // 2
    cb_data = array.array(
        "B",
        ((black + white) * p + (white + black) * p) * p,
    )
    cb_buffer = ngl.BufferUBVec4(data=cb_data)

    texture = ngl.Texture2D(
        width=_N,
        height=_N,
        min_filter="nearest",
        mipmap_filter="linear",
        data_src=cb_buffer,
    )

    program = ngl.Program(vertex=_RENDER_TEXTURE_LOD_VERT, fragment=_RENDER_TEXTURE_LOD_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture)

    group = ngl.Group(children=(draw,))
    if show_dbg_points:
        group.add_children(get_points_nodes(cfg, _MIPMAP_CUEPOINTS))

    return group


@test_fingerprint(width=320, height=320, keyframes=5, tolerance=1)
@ngl.scene()
def texture_reframing(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = d = 3

    media = load_media("hamster")
    anim_pos_kf = [
        ngl.AnimKeyFrameVec3(0, (-1, -1, 0)),
        ngl.AnimKeyFrameVec3(d / 2, (1, 1, 0)),
        ngl.AnimKeyFrameVec3(d, (-1, -1, 0)),
    ]
    anim_angle_kf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(d, 360)]
    tex = ngl.Texture2D(data_src=ngl.Media(filename=media.filename))
    tex = ngl.Scale(tex, factors=(1.2, 1.2, 1))
    tex = ngl.Rotate(tex, angle=ngl.AnimatedFloat(anim_angle_kf))
    tex = ngl.Translate(tex, vector=ngl.AnimatedVec3(anim_pos_kf))
    geometry = ngl.Quad(corner=(-0.8, -0.8, 0), width=(1.6, 0, 0), height=(0, 1.6, 0))
    return ngl.DrawTexture(texture=tex, geometry=geometry)


@test_fingerprint(width=640, height=360, keyframes=5, tolerance=1)
@ngl.scene()
def texture_masking(cfg: ngl.SceneCfg):
    media = load_media("cat")

    cfg.aspect_ratio = (media.width, media.height)
    cfg.duration = d = media.duration

    animkf = [
        ngl.AnimKeyFrameVec3(0, (0.5, 0.5, 1)),
        ngl.AnimKeyFrameVec3(d, (20, 20, 1), "exp_in"),
    ]

    return ngl.Group(
        children=[
            ngl.DrawGradient4(),
            ngl.DrawMask(
                content=ngl.Texture2D(
                    data_src=ngl.Media(filename=media.filename),
                ),
                mask=ngl.Texture2D(
                    data_src=ngl.Scale(
                        child=ngl.Text("CAT"),
                        factors=ngl.AnimatedVec3(animkf),
                        anchor=(0, -0.09, 0),
                    ),
                ),
                blending="src_over",
            ),
        ]
    )
