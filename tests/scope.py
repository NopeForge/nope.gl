#
# Copyright 2022-2023 Nope Forge
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

import textwrap

from pynopegl_utils.misc import load_media
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint

import pynopegl as ngl


def _get_colorstats(media):
    return ngl.ColorStats(
        texture=ngl.Texture2D(
            data_src=ngl.Media(media.filename),
            min_filter="nearest",
            mag_filter="nearest",
        )
    )


@test_fingerprint(keyframes=5, tolerance=3)
@ngl.scene()
def scope_colorstats(cfg):
    vert = textwrap.dedent(
        """
        void main()
        {
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
            uv = vec2(ngl_uvcoord.x, 1.0 - ngl_uvcoord.y);
        }
        """
    )
    frag = textwrap.dedent(
        """
        #define NB_SLICES 8
        void main()
        {
            float n = float(NB_SLICES);
            int s0 = int(floor(uv.x * n) / n * float(stats.depth));
            int s1 = s0 + stats.depth / NB_SLICES;

            uvec3 sum = uvec3(0);
            for (int i = s0; i < s1; i++) {
                int bin = clamp(i, 0, stats.depth - 1);
                sum += stats.summary[bin].rgb;
            }
            sum /= uint(NB_SLICES);

            vec3 amp = vec3(sum) / (float(stats.max_rgb.y) / zoom);
            ngl_out_color = vec4(step(uv.y, amp), 1.0);
        }
        """
    )

    media = load_media(cfg, "cat")

    # The selected test media has a lot of black so we zoom into the histogram
    # to have a relevant output
    uzoom = ngl.UniformFloat(50)

    cfg.duration = media.duration
    cfg.aspect_ratio = (media.width, media.height)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=vert, fragment=frag)
    program.update_vert_out_vars(uv=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(stats=_get_colorstats(media), zoom=uzoom)
    return render


def _get_histogram_func(mode):
    @test_fingerprint(keyframes=5, tolerance=5)
    @ngl.scene()
    def scene_func(cfg):
        media = load_media(cfg, "mire")
        cfg.duration = media.duration
        cfg.aspect_ratio = (media.width, media.height)
        return ngl.RenderHistogram(stats=_get_colorstats(media), mode=mode)

    return scene_func


scope_render_histogram_luma_only = _get_histogram_func("luma_only")
scope_render_histogram_mixed = _get_histogram_func("mixed")
scope_render_histogram_parade = _get_histogram_func("parade")


def _get_waveform_func(mode):
    @test_fingerprint(keyframes=5, tolerance=3)
    @ngl.scene()
    def scene_func(cfg):
        media = load_media(cfg, "mire")
        cfg.duration = media.duration
        cfg.aspect_ratio = (media.width, media.height)
        return ngl.RenderWaveform(stats=_get_colorstats(media), mode=mode)

    return scene_func


scope_render_waveform_luma_only = _get_waveform_func("luma_only")
scope_render_waveform_mixed = _get_waveform_func("mixed")
scope_render_waveform_parade = _get_waveform_func("parade")


@test_fingerprint(keyframes=5, tolerance=3)
@ngl.scene()
def scope_rtt(cfg):
    """This test makes sure the texture is analyzed after the RTT has written into it"""

    media = load_media(cfg, "mire")
    cfg.duration = media.duration
    cfg.aspect_ratio = (media.width, media.height)

    # Proxy scene
    src_texture = ngl.Texture2D(data_src=ngl.Media(media.filename), min_filter="nearest", mag_filter="nearest")
    scene = ngl.RenderTexture(src_texture)
    dst_texture = ngl.Texture2D(width=media.width, height=media.height, min_filter="nearest", mag_filter="nearest")
    rtt = ngl.RenderToTexture(scene, color_textures=[dst_texture])

    scope = ngl.RenderWaveform(stats=ngl.ColorStats(dst_texture))
    return ngl.Group(children=(rtt, scope))
