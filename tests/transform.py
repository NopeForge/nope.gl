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

import pynopegl as ngl
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.toolbox.colors import COLORS
from pynopegl_utils.toolbox.shapes import equilateral_triangle_coords


def _transform_shape(w=0.75, h=0.45):
    geometry = ngl.Quad(corner=(-w / 2.0, -h / 2.0, 0), width=(w, 0, 0), height=(0, h, 0))
    return ngl.DrawColor(COLORS.rose, geometry=geometry)


@test_fingerprint(width=320, height=320)
@ngl.scene()
def transform_matrix(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    mat = (
        # fmt: off
        0.5, 0.5, 0.0, 0.0,
       -1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
       -0.2, 0.4, 0.0, 1.0,
        # fmt: on
    )
    return ngl.Transform(shape, matrix=mat)


@test_fingerprint(width=320, height=180, keyframes=8, tolerance=1)
@ngl.scene()
def transform_animated_camera(cfg: ngl.SceneCfg):
    cfg.duration = 5.0
    cfg.aspect_ratio = (16, 9)
    g = ngl.Group()

    elems = (
        # fmt: off
        (COLORS.red,     None),
        (COLORS.yellow,  (-0.6,  0.8, -1)),
        (COLORS.green,   ( 0.6,  0.8, -1)),
        (COLORS.cyan,    (-0.6, -0.5, -1)),
        (COLORS.magenta, ( 0.6, -0.5, -1)),
        # fmt: on
    )

    quad = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    for color, vector in elems:
        node = ngl.DrawColor(color, geometry=quad)
        if vector:
            node = ngl.Translate(node, vector=vector)
        g.add_children(node)

    g = ngl.GraphicConfig(g, depth_test=True)
    camera = ngl.Camera(g)
    camera.set_eye(0, 0, 2)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_clipping(0.1, 10.0)

    tr_animkf = [ngl.AnimKeyFrameVec3(0, (0.0, 0.0, 0.0)), ngl.AnimKeyFrameVec3(cfg.duration, (0.0, 0.0, 3.0))]
    eye_transform = ngl.Translate(ngl.Identity(), vector=ngl.AnimatedVec3(tr_animkf))

    rot_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360)]
    eye_transform = ngl.Rotate(eye_transform, axis=(0, 1, 0), angle=ngl.AnimatedFloat(rot_animkf))

    camera.set_eye_transform(eye_transform)

    perspective_animkf = [
        ngl.AnimKeyFrameVec2(0.5, (60.0, cfg.aspect_ratio_float)),
        ngl.AnimKeyFrameVec2(cfg.duration, (45.0, cfg.aspect_ratio_float)),
    ]
    camera.set_perspective(ngl.AnimatedVec2(perspective_animkf))

    return camera


@test_fingerprint(width=320, height=320, keyframes=12, tolerance=1)
@ngl.scene()
def transform_eye_camera(cfg: ngl.SceneCfg):
    cfg.duration = 3.0
    cfg.aspect_ratio = (1, 1)

    node = ngl.DrawGradient4(geometry=ngl.Circle(radius=0.7, npoints=128))
    animkf = [
        ngl.AnimKeyFrameVec3(0, (0, -0.5, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2, (0, 1, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (0, -0.5, 0)),
    ]
    return ngl.Camera(node, eye=ngl.AnimatedVec3(animkf))


@test_fingerprint(width=320, height=320)
@ngl.scene(controls=dict(vector=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1))))
def transform_translate(cfg: ngl.SceneCfg, vector=(0.2, 0.7, -0.4)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.Translate(shape, vector)


@test_fingerprint(width=320, height=320)
@ngl.scene()
def transform_translate_animated(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3.0
    p0, p1, p2 = equilateral_triangle_coords()
    anim = [
        ngl.AnimKeyFrameVec3(0, p0),
        ngl.AnimKeyFrameVec3(1 * cfg.duration / 3.0, p1),
        ngl.AnimKeyFrameVec3(2 * cfg.duration / 3.0, p2),
        ngl.AnimKeyFrameVec3(cfg.duration, p0),
    ]
    shape = _transform_shape()
    return ngl.Translate(shape, vector=ngl.AnimatedVec3(anim))


@test_fingerprint(width=320, height=320)
@ngl.scene(controls=dict(factors=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1))))
def transform_scale(cfg: ngl.SceneCfg, factors=(0.7, 1.4, 0)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.Scale(shape, factors)


@test_fingerprint(width=320, height=320)
@ngl.scene(
    controls=dict(
        factors=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
        anchor=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    )
)
def transform_scale_anchor(cfg: ngl.SceneCfg, factors=(0.7, 1.4, 0), anchor=(-0.4, 0.5, 0.7)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.Scale(shape, factors, anchor=anchor)


@test_fingerprint(width=320, height=320)
@ngl.scene(controls=dict(factors=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1))))
def transform_scale_animated(cfg: ngl.SceneCfg, factors=(0.7, 1.4, 0)):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 2.0
    shape = _transform_shape()
    anim = [
        ngl.AnimKeyFrameVec3(0, (0, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2.0, factors),
        ngl.AnimKeyFrameVec3(cfg.duration, (0, 0, 0)),
    ]
    return ngl.Scale(shape, factors=ngl.AnimatedVec3(anim))


@test_fingerprint(width=320, height=320)
@ngl.scene(
    controls=dict(
        factors=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
        anchor=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    )
)
def transform_scale_anchor_animated(cfg: ngl.SceneCfg, factors=(0.7, 1.4, 0), anchor=(-0.4, 0.5, 0.7)):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 2.0
    shape = _transform_shape()
    anim = [
        ngl.AnimKeyFrameVec3(0, (0, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2.0, factors),
        ngl.AnimKeyFrameVec3(cfg.duration, (0, 0, 0)),
    ]
    return ngl.Scale(shape, factors=ngl.AnimatedVec3(anim), anchor=anchor)


@test_fingerprint(width=320, height=320)
@ngl.scene(
    controls=dict(
        angles=ngl.scene.Vector(n=3, minv=(-360, -360, -360), maxv=(360, 360, 360)),
        axis=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    )
)
def transform_skew(cfg: ngl.SceneCfg, angles=(0.0, -70, 14), axis=(1, 0, 0)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.Skew(shape, angles=angles, axis=axis)


@test_fingerprint(width=320, height=320, keyframes=8)
@ngl.scene(
    controls=dict(
        angles=ngl.scene.Vector(n=3, minv=(-360, -360, -360), maxv=(360, 360, 360)),
        axis=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    )
)
def transform_skew_animated(cfg: ngl.SceneCfg, angles=(0, -60, 14), axis=(1, 0, 0), anchor=(0, 0.05, -0.5)):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 2.0
    shape = _transform_shape()
    anim = [
        ngl.AnimKeyFrameVec3(0, (0, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2.0, angles),
        ngl.AnimKeyFrameVec3(cfg.duration, (0, 0, 0)),
    ]
    return ngl.Skew(shape, angles=ngl.AnimatedVec3(anim), axis=axis, anchor=anchor)


@test_fingerprint(width=320, height=320)
@ngl.scene(controls=dict(angle=ngl.scene.Range(range=[0, 360], unit_base=10)))
def transform_rotate(cfg: ngl.SceneCfg, angle=123.4):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.Rotate(shape, angle)


@test_fingerprint(width=320, height=320)
@ngl.scene(
    controls=dict(
        angle=ngl.scene.Range(range=[0, 360], unit_base=10),
        anchor=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    )
)
def transform_rotate_anchor(cfg: ngl.SceneCfg, angle=123.4, anchor=(0.15, 0.35, 0.7)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.Rotate(shape, angle, anchor=anchor)


@test_fingerprint(width=320, height=320)
@ngl.scene(controls=dict(quat=ngl.scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1))))
def transform_rotate_quat(cfg: ngl.SceneCfg, quat=(0, 0, -0.474, 0.880)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.RotateQuat(shape, quat)


@test_fingerprint(width=320, height=320)
@ngl.scene(
    controls=dict(
        quat=ngl.scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)),
        anchor=ngl.scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    )
)
def transform_rotate_quat_anchor(cfg: ngl.SceneCfg, quat=(0, 0, -0.474, 0.880), anchor=(0.15, 0.35, 0.7)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    return ngl.RotateQuat(shape, quat, anchor=anchor)


@test_fingerprint(width=320, height=320, keyframes=8)
@ngl.scene(
    controls=dict(
        quat0=ngl.scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)),
        quat1=ngl.scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)),
    )
)
def transform_rotate_quat_animated(cfg: ngl.SceneCfg, quat0=(0, 0, -0.474, 0.880), quat1=(0, 0, 0, 0)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape()
    anim = [
        ngl.AnimKeyFrameQuat(0, quat0),
        ngl.AnimKeyFrameQuat(cfg.duration / 2.0, quat1),
        ngl.AnimKeyFrameQuat(cfg.duration, quat0),
    ]
    return ngl.RotateQuat(shape, quat=ngl.AnimatedQuat(anim))


@test_fingerprint(width=320, height=320, keyframes=15)
@ngl.scene()
def transform_path(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 7
    shape = _transform_shape(w=0.2, h=0.5)

    # fmt: off
    points = (
        ( 0.7,  0.0, -0.3),
        (-0.8, -0.1,  0.1),
    )
    controls = (
        ( 0.2,  0.3, -0.2),
        (-0.2, -0.8, -0.4),
    )
    # fmt: on

    keyframes = [
        ngl.PathKeyMove(to=points[0]),
        ngl.PathKeyLine(to=points[1]),
        ngl.PathKeyBezier2(control=controls[0], to=points[0]),
        ngl.PathKeyBezier3(control1=controls[0], control2=controls[1], to=points[1]),
    ]
    path = ngl.Path(keyframes)

    # We use back_in_out easing to force an overflow on both sides
    anim_kf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration - 1, 1, "back_in_out"),
    ]

    return ngl.Translate(shape, vector=ngl.AnimatedPath(anim_kf, path))


@test_fingerprint(width=320, height=320, keyframes=15, tolerance=2)
@ngl.scene()
def transform_smoothpath(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3
    shape = _transform_shape(w=0.3, h=0.3)

    # fmt: off
    points = (
        (-0.62, -0.30, 0.0),
        (-0.36,  0.40, 0.0),
        ( 0.04, -0.27, 0.0),
        ( 0.36,  0.28, 0.0),
        ( 0.65, -0.04, 0.0),
    )
    controls = (
        (-0.84, 0.07, 0.0),
        ( 0.84, 0.04, 0.0),
    )
    # fmt: on

    flat_points = (elt for point in points for elt in point)
    points_array = array.array("f", flat_points)

    path = ngl.SmoothPath(
        ngl.BufferVec3(data=points_array),
        control1=controls[0],
        control2=controls[1],
        tension=0.4,
    )

    anim_kf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1, "exp_in_out"),
    ]

    return ngl.Translate(shape, vector=ngl.AnimatedPath(anim_kf, path))


@test_fingerprint(width=320, height=320, keyframes=15, tolerance=1)
@ngl.scene()
def transform_shared_anim(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 6

    # Duplicate the same shape at different positions
    shape = ngl.DrawColor(geometry=ngl.Circle(radius=1 / 3, npoints=5))
    shape0 = ngl.Translate(shape, vector=(-0.5, 0.5, 0))
    shape1 = ngl.Translate(shape, vector=(-0.5, -0.5, 0))

    # Same animation, reused at different times
    anim_d = cfg.duration * 2 / 3
    anim_kf = [
        ngl.AnimKeyFrameVec3(0, (0, 0, 0)),
        ngl.AnimKeyFrameVec3(anim_d / 2, (1, 0, 0), "exp_out"),
        ngl.AnimKeyFrameVec3(anim_d, (0, 0, 0), "back_in"),
    ]
    anim0 = ngl.Translate(shape0, vector=ngl.AnimatedVec3(anim_kf))
    anim1 = ngl.Translate(shape1, vector=ngl.AnimatedVec3(anim_kf, time_offset=cfg.duration * 1 / 3))

    return ngl.Group(children=[anim0, anim1])
