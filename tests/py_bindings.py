#
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

import pynodegl as ngl


def py_bindings_allow_node():
    c = ngl.Camera(eye=(0, 1, 0))
    assert c.set_eye(ngl.EvalVec3()) == 0
    assert c.set_eye(1, 0, 0) == 0

    c = ngl.Camera(eye=ngl.NoiseVec3())
    assert c.set_eye(1, 0, 0) == 0
    assert c.set_eye(ngl.UniformVec3()) == 0

    r = ngl.Rotate(angle=30)
    assert r.set_angle(ngl.NoiseFloat()) == 0
    assert r.set_angle(-45) == 0

    r = ngl.Rotate(angle=ngl.EvalFloat())
    assert r.set_angle(90) == 0
    assert r.set_angle(ngl.UniformFloat()) == 0


def py_bindings_dict():
    foo = ngl.UniformVec3(value=(1, 2, 3), label="foo-node")
    assert foo.set_value(3, 2, 1) == 0
    render = ngl.Render(vert_resources=dict(foo=foo))

    assert render.set_label("r") == 0
    # Delete the previous entry and add another
    ret = render.update_vert_resources(
        foo=None,
        bar=ngl.UniformFloat(value=4, label="bar-node"),
    )
    assert ret == 0
    # Update by using a dict
    ret = render.update_vert_resources(
        dict(
            foo=ngl.UniformVec2(),
            bar=ngl.EvalFloat(),
        )
    )
    assert ret == 0


def py_bindings_f64s():
    kf = ngl.AnimKeyFrameFloat(easing_args=[1.2, 4.6])
    assert kf.add_easing_args(-0.4, 10.2, 7.3) == 0


def py_bindings_no_param():
    """Nodes with no param (but still inherit the common parameters)"""

    # Identity directly inherits from the common node
    ident = ngl.Identity(label="id")
    assert ident.set_label("ID") == 0

    # IOVars have an intermediate inheritance before the common node
    iovar = ngl.IOVec3(label="io")
    assert iovar.set_label("IO") == 0


def py_bindings_nodes():
    group = ngl.Group(children=(ngl.Identity(), ngl.Group()))
    assert group.add_children(ngl.Group(), ngl.GraphicConfig()) == 0


def py_bindings_rational():
    streamed_int = ngl.StreamedInt(timebase=(1, 90000))
    assert streamed_int.set_timebase((3, 4)) == 0


def py_bindings_wrong_param():
    try:
        ngl.Group(dont="exist")
    except Exception:
        pass
    else:
        assert False
