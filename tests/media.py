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

import textwrap

from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.tests.cmp_resources import test_resources
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.toolbox.grid import AutoGrid

import pynodegl as ngl


def _get_time_scene(cfg):
    m0 = cfg.medias[0]

    media_seek = 10
    noop_duration = 2
    prefetch_duration = 2
    freeze_duration = 3
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start = range_start + freeze_duration
    play_stop = play_start + playback_duration
    range_stop = play_stop + freeze_duration
    duration = range_stop + noop_duration

    cfg.duration = duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop, media_seek + playback_duration),
    ]

    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m)
    r = ngl.RenderTexture(t)

    time_ranges = [
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(range_start),
        ngl.TimeRangeModeNoop(range_stop),
    ]
    rf = ngl.TimeRangeFilter(r, ranges=time_ranges, prefetch_time=prefetch_duration)

    return rf


@test_fingerprint(width=320, height=240, nb_keyframes=3, tolerance=1)
@scene()
def media_flat_remap(cfg):
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(cfg.duration / 2, 1.833),
    ]

    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m)
    return ngl.RenderTexture(t)


@test_cuepoints(points={"X": (0, -0.625)}, nb_keyframes=15, clear_color=list(COLORS.violet) + [1], tolerance=1)
@scene()
def media_phases_display(cfg):
    return _get_time_scene(cfg)


@test_resources(nb_keyframes=15)
@scene()
def media_phases_resources(cfg):
    return _get_time_scene(cfg)


# Note: the following test only makes sure the clamping code shader compiles,
# not check for an actual overflow
@test_cuepoints(points={"X": (0, -0.625)}, nb_keyframes=1, tolerance=1)
@scene()
def media_clamp(cfg):
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media, clamp_video=True)
    return ngl.RenderTexture(texture)


@test_cuepoints(points={f"P{i}": (i / 5 * 2 - 1, 0) for i in range(5)}, nb_keyframes=5, tolerance=1)
@scene()
def media_exposed_time(cfg):
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    vert = textwrap.dedent(
        """\
        void main()
        {
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
            uv = ngl_uvcoord;
        }
        """
    )

    frag = textwrap.dedent(
        """\
        void main()
        {
            ngl_out_color = vec4(vec3(step(0.0, tex0_ts/duration - uv.x)), 1.0);
        }
        """
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media)
    program = ngl.Program(vertex=vert, fragment=frag)
    program.update_vert_out_vars(uv=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture, duration=ngl.UniformFloat(cfg.duration))
    return render


@test_fingerprint(width=1024, height=1024, nb_keyframes=30, tolerance=1)
@scene(overlap_time=scene.Range(range=[0, 10], unit_base=10), dim=scene.Range(range=[1, 10]))
def media_queue(cfg, overlap_time=7.0, dim=3):
    cfg.duration = 10
    cfg.aspect_ratio = (1, 1)

    nb_medias = dim * dim

    medias = [m.filename for m in cfg.medias if m.filename.endswith(("mp4", "jpg"))]

    queued_medias = []
    ag = AutoGrid(range(nb_medias))
    for video_id, _, col, pos in ag:
        start = video_id * cfg.duration / nb_medias
        animkf = [
            ngl.AnimKeyFrameFloat(start, 0),
            ngl.AnimKeyFrameFloat(start + cfg.duration, cfg.duration),
        ]
        media = ngl.Media(medias[video_id % len(medias)], time_anim=ngl.AnimatedTime(animkf))

        texture = ngl.Texture2D(data_src=media, min_filter="linear", mag_filter="linear")
        render = ngl.RenderTexture(texture)
        render = ag.place_node(render, (col, pos))

        rf = ngl.TimeRangeFilter(render)
        if start:
            rf.add_ranges(ngl.TimeRangeModeNoop(0))
        rf.add_ranges(
            ngl.TimeRangeModeCont(start), ngl.TimeRangeModeNoop(start + cfg.duration / nb_medias + overlap_time)
        )

        queued_medias.append(rf)

    return ngl.Group(children=queued_medias)


@test_fingerprint(width=320, height=240, nb_keyframes=20, tolerance=1)
@scene()
def media_timeranges_rtt(cfg):
    m0 = cfg.medias[0]
    cfg.duration = d = 10
    cfg.aspect_ratio = (m0.width, m0.height)

    # Use a media/texture as leaf to exercise its prefetch/release mechanism
    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media)

    # Diamond tree on the same media texture
    render0 = ngl.RenderTexture(texture, label="leaf 0")
    render1 = ngl.RenderTexture(texture, label="leaf 1")

    # Create intermediate RTT "proxy" to exercise prefetch/release at this
    # level as well
    dst_tex0 = ngl.Texture2D(width=m0.width, height=m0.height)
    dst_tex1 = ngl.Texture2D(width=m0.width, height=m0.height)
    rtt0 = ngl.RenderToTexture(render0, [dst_tex0])
    rtt1 = ngl.RenderToTexture(render1, [dst_tex1])

    # Render the 2 RTTs vertically split (one half content each)
    quad0 = ngl.Quad((-1, -1, 0), (1, 0, 0), (0, 2, 0), uv_corner=(0, 0), uv_width=(0.5, 0))
    quad1 = ngl.Quad((0, -1, 0), (1, 0, 0), (0, 2, 0), uv_corner=(0.5, 0), uv_width=(0.5, 0))
    rtt_render0 = ngl.RenderTexture(dst_tex0, geometry=quad0, label="render RTT 0")
    rtt_render1 = ngl.RenderTexture(dst_tex1, geometry=quad1, label="render RTT 1")
    proxy0 = ngl.Group(children=(rtt0, rtt_render0), label="proxy 0")
    proxy1 = ngl.Group(children=(rtt1, rtt_render1), label="proxy 1")

    # We want to make sure the idle times are enough to exercise the
    # prefetch/release mechanism
    prefetch_time = 1
    assert prefetch_time < d / 5

    # Split the presentation in 5 segments such that there are inactive times,
    # prefetch times and both overlapping and non-overlapping times for the
    # RTTs
    ranges0 = (
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(1 / 5 * d),
        ngl.TimeRangeModeNoop(3 / 5 * d),
    )
    ranges1 = (
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(2 / 5 * d),
        ngl.TimeRangeModeNoop(4 / 5 * d),
    )
    trange0 = ngl.TimeRangeFilter(proxy0, ranges=ranges0, prefetch_time=prefetch_time, label="left")
    trange1 = ngl.TimeRangeFilter(proxy1, ranges=ranges1, prefetch_time=prefetch_time, label="right")

    return ngl.Group(children=(trange0, trange1))
