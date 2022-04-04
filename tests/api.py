#
# Copyright 2019 GoPro Inc.
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

import os
import math
import random
import pynodegl as ngl
from pynodegl_utils.misc import get_backend
from pynodegl_utils.toolbox.grid import autogrid_simple


_backend_str = os.environ.get('BACKEND')
_backend = get_backend(_backend_str) if _backend_str else ngl.BACKEND_AUTO


def _get_scene(geometry=None):
    return ngl.RenderColor(geometry=ngl.Quad() if geometry is None else geometry)


def api_backend():
    ctx = ngl.Context()
    ret = ctx.configure(backend=0x1234)
    assert ret < 0
    del ctx


def api_reconfigure():
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    assert ctx.draw(1) == 0
    del ctx


def api_reconfigure_clearcolor(width=16, height=16):
    import zlib
    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer)
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0xb4bd32fa
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer,
                        clear_color=(0.4, 0.4, 0.4, 1.0))
    assert ret == 0
    assert ctx.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0x05c44869
    del capture_buffer
    del ctx


def api_reconfigure_fail():
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    ret = ctx.configure(offscreen=0, backend=_backend)
    assert ret != 0
    assert ctx.draw(1) != 0
    del ctx


def api_capture_buffer(width=16, height=16):
    import zlib
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend)
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    for i in range(2):
        capture_buffer = bytearray(width * height * 4)
        assert ctx.set_capture_buffer(capture_buffer) == 0
        assert ctx.draw(0) == 0
        assert ctx.set_capture_buffer(None) == 0
        assert ctx.draw(0) == 0
        assert zlib.crc32(capture_buffer) == 0xb4bd32fa
    del ctx


def api_ctx_ownership():
    ctx = ngl.Context()
    ctx2 = ngl.Context()
    ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    ret = ctx2.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert ctx2.set_scene(scene) != 0
    assert ctx2.draw(0) == 0
    del ctx
    del ctx2


def api_ctx_ownership_subgraph():
    for shared in (True, False):
        ctx = ngl.Context()
        ctx2 = ngl.Context()
        ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
        assert ret == 0
        ret = ctx2.configure(offscreen=1, width=16, height=16, backend=_backend)
        assert ret == 0
        quad = ngl.Quad()
        render1 = _get_scene(quad)
        if not shared:
            quad = ngl.Quad()
        render2 = _get_scene(quad)
        scene = ngl.Group([render1, render2])
        assert ctx.set_scene(render2) == 0
        assert ctx.draw(0) == 0
        assert ctx2.set_scene(scene) != 0
        assert ctx2.draw(0) == 0
        del ctx
        del ctx2


def api_capture_buffer_lifetime(width=1024, height=1024):
    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer)
    assert ret == 0
    del capture_buffer
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    del ctx


# Exercise the HUD rasterization. We can't really check the output, so this is
# just for blind coverage and similar code instrumentalization.
def api_hud(width=234, height=123):
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend, hud=1)
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    for i in range(60 * 3):
        assert ctx.draw(i / 60.) == 0
    del ctx


def api_text_live_change(width=320, height=240):
    import zlib
    ctx = ngl.Context()
    capture_buffer = bytearray(width * height * 4)
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer)
    assert ret == 0

    # An empty string forces the text node to deal with a pipeline with nul
    # attributes, this is what we exercise here, along with a varying up and
    # down number of characters
    text_strings = ["foo", "", "foobar", "world", "hello\nworld", "\n\n", "last"]

    # Exercise the diamond-form/prepare mechanism
    text_node = ngl.Text()
    assert ctx.set_scene(autogrid_simple([text_node] * 4)) == 0

    ctx.draw(0)
    last_crc = zlib.crc32(capture_buffer)
    for i, s in enumerate(text_strings, 1):
        text_node.set_text(s)
        ctx.draw(i)
        crc = zlib.crc32(capture_buffer)
        assert crc != last_crc
        last_crc = crc


def _ret_to_fourcc(ret):
    if ret >= 0:
        return None
    x = -ret
    return chr(x>>24) + chr(x>>16 & 0xff) + chr(x>>8 & 0xff) + chr(x&0xff)


def api_media_sharing_failure():
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    m = ngl.Media('/dev/null')
    scene = ngl.Group(children=(ngl.Texture2D(data_src=m), ngl.Texture2D(data_src=m)))
    assert _ret_to_fourcc(ctx.set_scene(scene)) == 'Eusg'  # Usage error


def api_denied_node_live_change(width=320, height=240):
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=width, height=height, backend=_backend)
    assert ret == 0

    scene = ngl.Translate(ngl.Group())

    # Check that we can live change but not into a node
    assert ctx.set_scene(scene) == 0
    assert scene.set_vector(1, 2, 3) == 0
    assert scene.set_vector(ngl.UniformVec3(value=(3, 2, 1))) != 0

    # Check that we can do the change after a reset of the context
    assert ctx.set_scene(None) == 0
    assert scene.set_vector(ngl.UniformVec3(value=(4, 5, 6))) == 0

    # Check that we can not live unplug a node from a live changeable parameter
    assert ctx.set_scene(scene) == 0
    assert scene.set_vector(ngl.UniformVec3(value=(7, 8, 9))) != 0


def api_livectls():
    # Build a scene and extract its live controls
    rng = random.Random(0)
    scene = ngl.Group(children=(
        ngl.UniformBool(live_id='b'),
        ngl.UniformFloat(live_id='f'),
        ngl.UniformIVec3(live_id='iv3'),
        ngl.UserSwitch(
            ngl.Group(children=(
                ngl.UniformMat4(live_id='m4'),
                ngl.UniformColorA(live_id='clr'),
                ngl.UniformQuat(as_mat4=True, live_id='rot'),
            )),
            live_id='switch',
        ),
        ngl.Text(live_id='txt'),
    ))
    livectls = ngl.get_livectls(scene)
    assert len(livectls) == 8

    # Attach scene and run a dummy draw to make sure it's valid
    ctx = ngl.Context()
    ret = ctx.configure(offscreen=1, width=16, height=16, backend=_backend)
    assert ret == 0
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0

    # Apply live changes on nodes previously tracked by get_livectls()
    values = dict(
        b=True,
        f=rng.uniform(-1, 1),
        iv3=[rng.randint(-100, 100) for i in range(3)],
        switch=False,
        m4=[rng.uniform(-1, 1) for i in range(16)],
        clr=(.9, .3, .8, .9),
        rot=(.1, -.2, .5, -.3),
        txt='test string',
    )
    for live_id, value in values.items():
        node = livectls[live_id]['node']
        node_type = livectls[live_id]['node_type']
        assert node_type == node.__class__.__name__
        if node_type == 'UserSwitch':
            node.set_enabled(value)
        elif node_type == 'Text':
            node.set_text(value)
        elif hasattr(value, '__iter__'):
            node.set_value(*value)
        else:
            node.set_value(value)

    # Detach scene from context and grab all live controls again
    assert ctx.set_scene(None) == 0
    livectls = ngl.get_livectls(scene)

    # Inspect nodes to check if they were properly altered by the live changes
    for live_id, expected_value in values.items():
        value = livectls[live_id]['val']
        node_type = livectls[live_id]['node_type']
        if node_type == 'Text':
            assert value == expected_value, (value, expected_value)
        elif hasattr(value, '__iter__'):
            assert all(math.isclose(v, e, rel_tol=1e-6) for v, e in zip(value, expected_value))
        else:
            assert math.isclose(value, expected_value, rel_tol=1e-6)


def api_reset_scene(width=320, height=240):
    viewer = ngl.Context()
    ret = viewer.configure(offscreen=1, width=width, height=height, backend=_backend)
    assert ret == 0
    render = _get_scene()
    assert viewer.set_scene(render) == 0
    viewer.draw(0)
    assert viewer.set_scene(None) == 0
    viewer.draw(1)
    assert viewer.set_scene(render) == 0
    viewer.draw(2)
    assert viewer.set_scene(None) == 0
    viewer.draw(3)


def api_shader_init_fail(width=320, height=240):
    viewer = ngl.Context()
    ret = viewer.configure(offscreen=1, width=width, height=height, backend=_backend)
    assert ret == 0

    render = ngl.Render(ngl.Quad(), ngl.Program(vertex='<bug>', fragment='<bug>'))

    assert viewer.set_scene(render) != 0
    assert viewer.set_scene(render) != 0  # another try to make sure the state stays consistent
    assert viewer.draw(0) == 0
