#
# Copyright 2023 Nope Forge
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

import textwrap

from pynopegl_utils.misc import load_media
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.cuepoints_utils import get_grid_points, get_points_nodes

import pynopegl as ngl


@test_fingerprint(width=256, height=256, keyframes=10, tolerance=1)
@ngl.scene()
def blur_gaussian(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 10

    noise = ngl.DrawNoise(type="blocky", octaves=3, scale=(9, 9))
    noise_texture = ngl.Texture2D(data_src=noise)
    blurred_texture = ngl.Texture2D()
    blur = ngl.GaussianBlur(
        source=noise_texture,
        destination=blurred_texture,
        bluriness=ngl.AnimatedFloat(
            [
                ngl.AnimKeyFrameFloat(0, 0),
                ngl.AnimKeyFrameFloat(cfg.duration, 1),
            ]
        ),
    )
    return ngl.Group(children=(blur, ngl.DrawTexture(blurred_texture)))


@test_fingerprint(width=800, height=800, keyframes=10, tolerance=5)
@ngl.scene()
def blur_fast_gaussian(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 10

    noise = ngl.DrawNoise(type="blocky", octaves=3, scale=(9, 9))
    noise_texture = ngl.Texture2D(data_src=noise)
    blurred_texture = ngl.Texture2D()
    blur = ngl.FastGaussianBlur(
        source=noise_texture,
        destination=blurred_texture,
        bluriness=ngl.AnimatedFloat(
            [
                ngl.AnimKeyFrameFloat(0, 0),
                ngl.AnimKeyFrameFloat(cfg.duration, 1),
            ]
        ),
    )
    return ngl.Group(children=(blur, ngl.DrawTexture(blurred_texture)))


_BLUR_HEXAGONAL_CUEPOINTS = get_grid_points(10, 10)


@test_cuepoints(width=540, height=808, points=_BLUR_HEXAGONAL_CUEPOINTS, keyframes=5, tolerance=5)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def blur_hexagonal(cfg: ngl.SceneCfg, show_dbg_points=False):
    mi = load_media("city")
    cfg.aspect_ratio = (mi.width, mi.height)
    cfg.duration = 5

    source_texture = ngl.Texture2D(data_src=ngl.Media(filename=mi.filename))
    blurred_texture = ngl.Texture2D()
    blur = ngl.HexagonalBlur(
        source=source_texture,
        destination=blurred_texture,
        amount=ngl.AnimatedFloat(
            [
                ngl.AnimKeyFrameFloat(0, 0),
                ngl.AnimKeyFrameFloat(cfg.duration, 1),
            ]
        ),
    )

    group = ngl.Group(children=(blur, ngl.DrawTexture(blurred_texture)))
    if show_dbg_points:
        group.add_children(get_points_nodes(cfg, _BLUR_HEXAGONAL_CUEPOINTS))

    return group


_BLUR_HEXAGONAL_MAP_VERTEX = textwrap.dedent(
    """
    void main()
    {
        vec4 position = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
        uv = position.xy;
        ngl_out_pos = position;
    }
    """
)

_BLUR_HEXAGONAL_MAP_FRAGMENT = textwrap.dedent(
    """
    #define ngli_sat(x) clamp(x, 0.0, 1.0)
    #define ngli_linear(a, b, x) (((x) - (a)) / ((b) - (a)))
    #define ngli_linearstep(a, b, x) ngli_sat(ngli_linear(a, b, x))

    float sd_rounded_box(vec2 position, vec2 size, float radius)
    {
       position = abs(position) - size + radius;
       return length(max(position, 0.0)) + min(max(position.x, position.y), 0.0) - radius;
    }

    void main()
    {
        float sd = sd_rounded_box(uv + vec2(-0.016, 0.11), vec2(0.19, 0.19), 0.13);
        float value = ngli_linearstep(0.0, 0.8, sd);
        ngl_out_color = vec4(value);
    }
    """
)


@test_cuepoints(width=540, height=808, points=_BLUR_HEXAGONAL_CUEPOINTS, keyframes=5, tolerance=5)
@ngl.scene(controls=dict(show_dbg_points=ngl.scene.Bool()))
def blur_hexagonal_with_map(cfg: ngl.SceneCfg, show_dbg_points=False):
    mi = load_media("city")
    cfg.aspect_ratio = (mi.width, mi.height)
    cfg.duration = 5

    quad = ngl.Quad(corner=(-1, -1, 0), width=(2, 0, 0), height=(0, 2, 0))
    program = ngl.Program(vertex=_BLUR_HEXAGONAL_MAP_VERTEX, fragment=_BLUR_HEXAGONAL_MAP_FRAGMENT)
    program.update_vert_out_vars(uv=ngl.IOVec2())
    render = ngl.Draw(quad, program)
    map_texture = ngl.Texture2D(width=mi.width // 2, height=mi.height // 2, format="r8_unorm", data_src=render)

    source_texture = ngl.Texture2D(data_src=ngl.Media(filename=mi.filename))
    blurred_texture = ngl.Texture2D()
    blur = ngl.HexagonalBlur(
        source=source_texture,
        destination=blurred_texture,
        amount=ngl.AnimatedFloat(
            [
                ngl.AnimKeyFrameFloat(0, 0),
                ngl.AnimKeyFrameFloat(cfg.duration, 1),
            ]
        ),
        map=map_texture,
    )

    group = ngl.Group(children=(blur, ngl.DrawTexture(blurred_texture)))
    if show_dbg_points:
        group.add_children(get_points_nodes(cfg, _BLUR_HEXAGONAL_CUEPOINTS))

    return group
