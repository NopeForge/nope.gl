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

from pynopegl_utils.misc import SceneCfg, scene
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.debug import get_debug_points
from pynopegl_utils.toolbox.colors import COLORS, get_random_color_buffer

import pynopegl as ngl

_RENDER_BUFFER_FRAG = """
void main()
{
    float color = ngl_tex2d(tex0, var_tex0_coord).r;
    ngl_out_color = vec4(color, 0.0, 0.0, 1.0);
}
"""


def _render_buffer(cfg: SceneCfg, w, h):
    n = w * h
    data = array.array("B", [i * 255 // n for i in range(n)])
    buf = ngl.BufferUByte(data=data)
    texture = ngl.Texture2D(width=w, height=h, data_src=buf)
    program = ngl.Program(vertex=cfg.get_vert("texture"), fragment=_RENDER_BUFFER_FRAG)
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(ngl.Quad(), program)
    render.update_frag_resources(tex0=texture)
    return render


@test_fingerprint()
@scene(w=scene.Range(range=[1, 128]), h=scene.Range(range=[1, 128]))
def texture_data(cfg: SceneCfg, w=4, h=5):
    cfg.aspect_ratio = (1, 1)
    return _render_buffer(cfg, w, h)


@test_fingerprint()
@scene(dim=scene.Range(range=[1, 100]))
def texture_data_animated(cfg: SceneCfg, dim=8):
    cfg.duration = 3.0
    nb_kf = int(cfg.duration)
    buffers = [get_random_color_buffer(cfg.rng, dim) for _ in range(nb_kf)]
    random_animkf = []
    time_scale = cfg.duration / float(nb_kf)
    for i, buf in enumerate(buffers + [buffers[0]]):
        random_animkf.append(ngl.AnimKeyFrameBuffer(i * time_scale, buf))
    random_buffer = ngl.AnimatedBufferVec4(keyframes=random_animkf)
    random_tex = ngl.Texture2D(data_src=random_buffer, width=dim, height=dim)
    return ngl.RenderTexture(random_tex)


@test_fingerprint()
@scene(h=scene.Range(range=[1, 32]))
def texture_data_unaligned_row(cfg: SceneCfg, h=32):
    """Tests upload of buffers with rows that are not 4-byte aligned"""
    cfg.aspect_ratio = (1, 1)
    return _render_buffer(cfg, 1, h)


@test_fingerprint(nb_keyframes=5)
@scene()
def texture_displacement(cfg: SceneCfg):
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    m = ngl.Media(m0.filename)
    source_tex = ngl.Texture2D(data_src=m)

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
    displace_render = ngl.Render(q, p)
    displace_render.update_frag_resources(t=ngl.Time())

    displace_tex = ngl.Texture2D(width=m0.width, height=m0.height)
    rtt = ngl.RenderToTexture(displace_render)
    rtt.add_color_textures(displace_tex)

    render = ngl.RenderDisplace(source_tex, displace_tex)
    return ngl.Group(children=[rtt, render])


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
    ngl_out_color = ngl_texcube(tex0, vec3(var_uvcoord.xy, 0.5));
}
"""


def _get_texture_cubemap_from_mrt_scene(samples=0):
    program = ngl.Program(vertex=_RENDER_TO_CUBEMAP_VERT, fragment=_RENDER_TO_CUBEMAP_FRAG, nb_frag_output=6)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    cube = ngl.TextureCube(size=64, min_filter="linear", mag_filter="linear")
    rtt = ngl.RenderToTexture(render, [cube], samples=samples)

    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=cube)

    return ngl.Group(children=(rtt, render))


def _get_texture_cubemap_from_mrt_scene_2_pass(samples=0):
    group = ngl.Group()
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    cube = ngl.TextureCube(size=64, min_filter="linear", mag_filter="linear")

    layer_base = 0
    for layer_count, fragment in ((2, _RENDER_TO_CUBEMAP_1_FRAG), (4, _RENDER_TO_CUBEMAP_2_FRAG)):
        program = ngl.Program(vertex=_RENDER_TO_CUBEMAP_VERT, fragment=fragment, nb_frag_output=layer_count)
        program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
        render = ngl.Render(quad, program)
        color_textures = [ngl.TextureView(cube, layer) for layer in range(layer_base, layer_base + layer_count)]
        rtt = ngl.RenderToTexture(render, color_textures, samples=samples)
        group.add_children(rtt)
        layer_base += layer_count

    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=cube)
    group.add_children(render)

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


@test_fingerprint()
@scene()
def texture_cubemap(_):
    cube = _get_texture_cubemap()
    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=cube)
    return render


_RENDER_CUBEMAP_LOD_FRAG = """
void main()
{
    ngl_out_color = ngl_texcubelod(tex0, vec3(var_uvcoord.xy, 0.5), 1.0);
}
"""


@test_fingerprint(tolerance=1)
@scene()
def texture_cubemap_mipmap(_):
    cube = _get_texture_cubemap(mipmap_filter="nearest")
    program = ngl.Program(vertex=_RENDER_CUBEMAP_VERT, fragment=_RENDER_CUBEMAP_LOD_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=cube)
    return render


@test_fingerprint()
@scene()
def texture_cubemap_from_mrt(_):
    return _get_texture_cubemap_from_mrt_scene()


@test_fingerprint()
@scene()
def texture_cubemap_from_mrt_msaa(_):
    return _get_texture_cubemap_from_mrt_scene(4)


@test_fingerprint()
@scene()
def texture_cubemap_from_mrt_2_pass(_):
    return _get_texture_cubemap_from_mrt_scene_2_pass()


@test_fingerprint()
@scene()
def texture_cubemap_from_mrt_2_pass_msaa(_):
    return _get_texture_cubemap_from_mrt_scene_2_pass(4)


@test_cuepoints(width=32, height=32, points={"bottom-left": (-1, -1), "top-right": (1, 1)}, tolerance=1)
@scene()
def texture_clear_and_scissor(_):
    render = ngl.RenderColor(COLORS.white)
    graphic_config = ngl.GraphicConfig(render, scissor_test=True, scissor=(0, 0, 0, 0), color_write_mask="")

    texture = ngl.Texture2D(width=64, height=64)
    rtt = ngl.RenderToTexture(ngl.Identity(), [texture], clear_color=COLORS.orange + (1,))
    render = ngl.RenderTexture(texture)

    return ngl.Group(children=(graphic_config, rtt, render))


@test_fingerprint(width=64, height=64)
@scene()
def texture_scissor(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)

    render = ngl.RenderColor(COLORS.orange)
    graphic_config = ngl.GraphicConfig(render, scissor_test=True, scissor=(32, 32, 32, 32))
    texture = ngl.Texture2D(width=64, height=64)
    rtt = ngl.RenderToTexture(graphic_config, [texture], clear_color=(0, 0, 0, 1))
    render = ngl.RenderTexture(texture)
    return ngl.Group(children=(rtt, render))


_TEXTURE2D_ARRAY_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""


_TEXTURE2D_ARRAY_FRAG = """
void main()
{
    ngl_out_color  = texture(tex0, vec3(var_uvcoord, 0.0));
    ngl_out_color += texture(tex0, vec3(var_uvcoord, 1.0));
    ngl_out_color += texture(tex0, vec3(var_uvcoord, 2.0));
}
"""


def _get_texture_2d_array(cfg: SceneCfg, mipmap_filter="none"):
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
        width=width, height=height, depth=depth, data_src=texture_buffer, mipmap_filter=mipmap_filter
    )
    return texture


@test_fingerprint()
@scene()
def texture_2d_array(cfg: SceneCfg):
    texture = _get_texture_2d_array(cfg)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_TEXTURE2D_ARRAY_VERT, fragment=_TEXTURE2D_ARRAY_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)
    return render


_TEXTURE2D_ARRAY_LOD_FRAG = """
void main()
{
    ngl_out_color  = textureLod(tex0, vec3(var_uvcoord, 0.0), 2.0);
    ngl_out_color += textureLod(tex0, vec3(var_uvcoord, 1.0), 2.0);
    ngl_out_color += textureLod(tex0, vec3(var_uvcoord, 2.0), 2.0);
}
"""


@test_fingerprint(tolerance=4)
@scene()
def texture_2d_array_mipmap(cfg: SceneCfg):
    texture = _get_texture_2d_array(cfg, mipmap_filter="nearest")
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_TEXTURE2D_ARRAY_VERT, fragment=_TEXTURE2D_ARRAY_LOD_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)
    return render


_RENDER_TO_TEXTURE2D_ARRAY_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
}
"""

_RENDER_TO_TEXTURE2D_ARRAY_FRAG = """
void main()
{
    float x = floor(gl_FragCoord.x / ngl_resolution.x * steps) / steps;
    ngl_out_color[0] = vec4(x, 0.0, 0.0, 1.0);
    ngl_out_color[1] = vec4(0.0, x, 0.0, 1.0);
    ngl_out_color[2] = vec4(0.0, 0.0, x, 1.0);
}
"""

_STEPS = 4


def _get_texture_2d_array_from_mrt_cuepoints():
    f = float(_STEPS)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return {f"{x}": (c(x), 0.0) for x in range(_STEPS)}


def _get_texture_2d_array_from_mrt_scene(cfg, show_dbg_points, samples=0):
    depth = 3
    program = ngl.Program(
        vertex=_RENDER_TO_TEXTURE2D_ARRAY_VERT, fragment=_RENDER_TO_TEXTURE2D_ARRAY_FRAG, nb_frag_output=depth
    )
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    render.update_frag_resources(steps=ngl.UniformFloat(value=_STEPS))
    texture = ngl.Texture2DArray(width=64, height=64, depth=depth, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(render, [texture], samples=samples)

    program = ngl.Program(vertex=_TEXTURE2D_ARRAY_VERT, fragment=_TEXTURE2D_ARRAY_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)

    group = ngl.Group(children=(rtt, render))
    if show_dbg_points:
        group.add_children(get_debug_points(cfg, _get_texture_3d_from_mrt_cuepoints()))

    return group


@test_cuepoints(points=_get_texture_2d_array_from_mrt_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def texture_2d_array_from_mrt(cfg: SceneCfg, show_dbg_points=False):
    return _get_texture_2d_array_from_mrt_scene(cfg, show_dbg_points)


@test_cuepoints(points=_get_texture_2d_array_from_mrt_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def texture_2d_array_from_mrt_msaa(cfg: SceneCfg, show_dbg_points=False):
    return _get_texture_2d_array_from_mrt_scene(cfg, show_dbg_points, 4)


_TEXTURE3D_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""


_TEXTURE3D_FRAG = """
void main()
{
    ngl_out_color  = ngl_tex3d(tex0, vec3(var_uvcoord, 0.0));
    ngl_out_color += ngl_tex3d(tex0, vec3(var_uvcoord, 0.5));
    ngl_out_color += ngl_tex3d(tex0, vec3(var_uvcoord, 1.0));
}
"""


@test_fingerprint()
@scene()
def texture_3d(cfg: SceneCfg):
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
    texture = ngl.Texture3D(width=width, height=height, depth=depth, data_src=texture_buffer)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_TEXTURE3D_VERT, fragment=_TEXTURE3D_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)
    return render


_RENDER_TO_TEXTURE3D_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
}
"""

_RENDER_TO_TEXTURE3D_FRAG = """
void main()
{
    float x = floor(gl_FragCoord.x / ngl_resolution.x * steps) / steps;
    ngl_out_color[0] = vec4(x, 0.0, 0.0, 1.0);
    ngl_out_color[1] = vec4(0.0, x, 0.0, 1.0);
    ngl_out_color[2] = vec4(0.0, 0.0, x, 1.0);
}
"""

_STEPS = 4


def _get_texture_3d_from_mrt_cuepoints():
    f = float(_STEPS)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return {f"{x}": (c(x), 0.0) for x in range(_STEPS)}


def _get_texture_3d_from_mrt_scene(cfg, show_dbg_points, samples=0):
    depth = 3
    program = ngl.Program(vertex=_RENDER_TO_TEXTURE3D_VERT, fragment=_RENDER_TO_TEXTURE3D_FRAG, nb_frag_output=depth)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    render.update_frag_resources(steps=ngl.UniformFloat(value=_STEPS))
    texture = ngl.Texture3D(width=64, height=64, depth=depth, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(render, [texture], samples=samples)

    program = ngl.Program(vertex=_TEXTURE3D_VERT, fragment=_TEXTURE3D_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)

    group = ngl.Group(children=(rtt, render))
    if show_dbg_points:
        group.add_children(get_debug_points(cfg, _get_texture_3d_from_mrt_cuepoints()))

    return group


@test_cuepoints(points=_get_texture_3d_from_mrt_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def texture_3d_from_mrt(cfg: SceneCfg, show_dbg_points=False):
    return _get_texture_3d_from_mrt_scene(cfg, show_dbg_points)


@test_cuepoints(points=_get_texture_3d_from_mrt_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def texture_3d_from_mrt_msaa(cfg: SceneCfg, show_dbg_points=False):
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
    ngl_out_color = ngl_tex2dlod(tex0, var_uvcoord, 0.5);
}
"""


_N = 8


def _get_texture_mipmap_cuepoints():
    f = float(_N)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return {f"{x}{y}": (c(x), c(y)) for y in range(_N) for x in range(_N)}


@test_cuepoints(points=_get_texture_mipmap_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def texture_mipmap(cfg: SceneCfg, show_dbg_points=False):
    cfg.aspect_ratio = (1, 1)
    cuepoints = _get_texture_mipmap_cuepoints()
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
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)

    group = ngl.Group(children=(render,))
    if show_dbg_points:
        group.add_children(get_debug_points(cfg, cuepoints))

    return group
