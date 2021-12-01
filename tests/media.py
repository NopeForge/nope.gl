#!/usr/bin/env python
#
# Copyright 2020 GoPro Inc.
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
from pynodegl_utils.misc import scene, Media
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_resources import test_resources
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint


def _get_time_scene(cfg):
    m0 = cfg.medias[0]

    media_seek        = 10
    noop_duration     = 2
    prefetch_duration = 2
    freeze_duration   = 3
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start  = range_start   + freeze_duration
    play_stop   = play_start    + playback_duration
    range_stop  = play_stop     + freeze_duration
    duration    = range_stop    + noop_duration

    cfg.duration = duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop,  media_seek + playback_duration),
    ]

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    r = ngl.Render(q, p)
    r.update_frag_resources(tex0=t)

    time_ranges = [
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(range_start),
        ngl.TimeRangeModeNoop(range_stop),
    ]
    rf = ngl.TimeRangeFilter(r, ranges=time_ranges, prefetch_time=prefetch_duration)

    return rf


@scene()
def flat_remap(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]

    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(-0.4, 1.833),
        ngl.AnimKeyFrameFloat(cfg.duration, 1.833),
    ]

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m)
    render = ngl.Render(q)
    render.update_frag_resources(tex0=t)
    return render


@test_cuepoints(points={'X': (0, -0.625)}, nb_keyframes=15, clear_color=COLORS.violet, tolerance=1)
@scene()
def media_phases_display(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]
    return _get_time_scene(cfg)


@test_resources(nb_keyframes=15)
@scene()
def media_phases_resources(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]
    return _get_time_scene(cfg)


# Note: the following test only makes sure the clamping code shader compiles,
# not check for an actual overflow
@test_cuepoints(points={'X': (0, -0.625)}, nb_keyframes=1, tolerance=1)
@scene()
def media_clamp(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]

    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media, clamp_video=True)
    program = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)
    return render


@test_fingerprint(width=320, height=240, nb_keyframes=20, tolerance=1)
@scene()
def media_timeranges_rtt(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]

    m0 = cfg.medias[0]
    cfg.duration = d = 10
    cfg.aspect_ratio = (m0.width, m0.height)

    program = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())

    # Use a media/texture as leaf to exercise its prefetch/release mechanism
    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media)

    # Diamond tree on the same media texture
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render0 = ngl.Render(quad, program, frag_resources=dict(tex0=texture), label='leaf 0')
    render1 = ngl.Render(quad, program, frag_resources=dict(tex0=texture), label='leaf 1')

    # Create intermediate RTT "proxy" to exercise prefetch/release at this
    # level as well
    dst_tex0 = ngl.Texture2D(width=m0.width, height=m0.height)
    dst_tex1 = ngl.Texture2D(width=m0.width, height=m0.height)
    rtt0 = ngl.RenderToTexture(render0, [dst_tex0])
    rtt1 = ngl.RenderToTexture(render1, [dst_tex1])

    # Render the 2 RTTs vertically split (one half content each)
    quad0 = ngl.Quad((-1, -1, 0), (1, 0, 0), (0, 2, 0), uv_corner=(0, 0), uv_width=(.5, 0))
    quad1 = ngl.Quad((0, -1, 0), (1, 0, 0), (0, 2, 0), uv_corner=(.5, 0), uv_width=(.5, 0))
    rtt_render0 = ngl.Render(quad0, program, frag_resources=dict(tex0=dst_tex0), label='render RTT 0')
    rtt_render1 = ngl.Render(quad1, program, frag_resources=dict(tex0=dst_tex1), label='render RTT 1')
    proxy0 = ngl.Group(children=(rtt0, rtt_render0), label='proxy 0')
    proxy1 = ngl.Group(children=(rtt1, rtt_render1), label='proxy 1')

    # We want to make sure the idle times are enough to exercise the
    # prefetch/release mechanism
    prefetch_time = 1
    assert prefetch_time < d / 5

    # Split the presentation in 5 segments such that there are inactive times,
    # prefetch times and both overlapping and non-overlapping times for the
    # RTTs
    ranges0 = (
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(1/5 * d),
        ngl.TimeRangeModeNoop(3/5 * d),
    )
    ranges1 = (
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(2/5 * d),
        ngl.TimeRangeModeNoop(4/5 * d),
    )
    trange0 = ngl.TimeRangeFilter(proxy0, ranges=ranges0, prefetch_time=prefetch_time, label='left')
    trange1 = ngl.TimeRangeFilter(proxy1, ranges=ranges1, prefetch_time=prefetch_time, label='right')

    return ngl.Group(children=(trange0, trange1))
