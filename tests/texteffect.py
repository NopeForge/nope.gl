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

from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint

import pynopegl as ngl


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_color(cfg: ngl.SceneCfg):
    cfg.duration = 3

    text = "Rainbow"
    n = len(text)

    # Build one effect per char; the animation cannot be sahred between all
    # chars using a single effect because the animated values are actually
    # different
    effects = []
    for i in range(n):
        hue = i / (n - 1)
        animkf = [
            ngl.AnimKeyFrameColor(0, color=(hue, 0.6, 0.6)),
            ngl.AnimKeyFrameColor(0.5, color=(hue, 0.8, 1)),
            ngl.AnimKeyFrameColor(1, color=(hue, 0.6, 0.6)),
        ]
        effects.append(
            ngl.TextEffect(
                start_pos=i / n,
                end_pos=(i + 1) / n,
                overlap=0.7,
                target="char",
                color=ngl.AnimatedColor(animkf, space="hsl"),
            )
        )

    return ngl.Text(text=text, effects=effects, aspect_ratio=cfg.aspect_ratio)


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_opacity(cfg: ngl.SceneCfg):
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameFloat(0, 1),
        ngl.AnimKeyFrameFloat(0.5, 0.2),
        ngl.AnimKeyFrameFloat(1, 1),
    ]
    effects = [
        ngl.TextEffect(
            overlap=0.8,
            target="line",
            opacity=ngl.AnimatedFloat(animkf),
        )
    ]

    return ngl.Text("Ghost\nin the\nText", effects=effects, aspect_ratio=cfg.aspect_ratio)


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_transform(cfg: ngl.SceneCfg):
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameVec3(0, (0.0, 1.5, 0.0)),
        ngl.AnimKeyFrameVec3(1, (0.0, 0.0, 0.0), "bounce_out"),
    ]
    bounce_down = ngl.Translate(ngl.Identity(), vector=ngl.AnimatedVec3(animkf))
    effects = [
        ngl.TextEffect(
            start=0,
            end=cfg.duration - 1,
            target="word",
            overlap=0.9,
            transform=bounce_down,
            random=True,
            random_seed=0xFFA,
        ),
    ]

    return ngl.Text("Drop me down", effects=effects, aspect_ratio=cfg.aspect_ratio)


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_chars_space_nospace(cfg: ngl.SceneCfg):
    cfg.duration = 5

    animkf = [
        ngl.AnimKeyFrameColor(0, (1, 1, 1)),
        ngl.AnimKeyFrameColor(1 / 2, (1, 0, 0)),
        ngl.AnimKeyFrameColor(1, (1, 1, 1)),
    ]

    effects0 = [ngl.TextEffect(target="char", color=ngl.AnimatedColor(animkf))]
    effects1 = [ngl.TextEffect(target="char_nospace", color=ngl.AnimatedColor(animkf))]
    return ngl.Group(
        children=[
            ngl.Text(
                "AB CDE  F",
                effects=effects0,
                aspect_ratio=cfg.aspect_ratio,
                box_corner=(-1, 0, 0),
                box_height=(0, 1, 0),
            ),
            ngl.Text(
                "AB CDE  F",
                effects=effects1,
                aspect_ratio=cfg.aspect_ratio,
                box_corner=(-1, -1, 0),
                box_height=(0, 1, 0),
            ),
        ]
    )


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_blur(cfg: ngl.SceneCfg):
    cfg.duration = 5
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(1 / 2, 0.1),
        ngl.AnimKeyFrameFloat(1, 0),
    ]
    effects = [ngl.TextEffect(blur=ngl.AnimatedFloat(animkf))]
    return ngl.Text("B", effects=effects, aspect_ratio=cfg.aspect_ratio)


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_blur_outline(cfg: ngl.SceneCfg):
    cfg.duration = 5
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(1 / 2, 0.1),
        ngl.AnimKeyFrameFloat(1, 0),
    ]
    effects = [ngl.TextEffect(blur=ngl.AnimatedFloat(animkf), outline=0.01)]
    return ngl.Text("b", effects=effects, aspect_ratio=cfg.aspect_ratio)


@test_fingerprint(keyframes=10, tolerance=1)
@ngl.scene()
def texteffect_glow(cfg: ngl.SceneCfg):
    cfg.duration = 5
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(1 / 2, 0.2),
        ngl.AnimKeyFrameFloat(1, 0),
    ]
    effects = [ngl.TextEffect(glow=ngl.AnimatedFloat(animkf), glow_color=(1, 0, 0))]
    return ngl.Text("G", effects=effects, aspect_ratio=cfg.aspect_ratio)


@test_fingerprint(keyframes=10, tolerance=2)
@ngl.scene()
def texteffect_glow_outline(cfg: ngl.SceneCfg):
    cfg.duration = 5
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(1 / 2, 0.2),
        ngl.AnimKeyFrameFloat(1, 0),
    ]
    effects = [ngl.TextEffect(glow=ngl.AnimatedFloat(animkf), outline=0.01)]
    return ngl.Text("g", effects=effects, aspect_ratio=cfg.aspect_ratio)
