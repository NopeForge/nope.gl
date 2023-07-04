#
# Copyright 2019-2022 GoPro Inc.
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

import atexit
import csv
import locale
import math
import os
import pprint
import random
import tempfile
from collections import namedtuple
from pathlib import Path

from pynopegl_utils.misc import get_backend
from pynopegl_utils.toolbox.grid import autogrid_simple

import pynopegl as ngl

_backend_str = os.environ.get("BACKEND")
_backend = get_backend(_backend_str) if _backend_str else ngl.Backend.AUTO


def _get_scene(geometry=None):
    return ngl.Scene.from_params(ngl.RenderColor(geometry=ngl.Quad() if geometry is None else geometry))


def api_backend():
    ctx = ngl.Context()
    fake_backend_cls = namedtuple("FakeBackend", "value")
    fake_backend = fake_backend_cls(value=0x1234)
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=fake_backend))
    assert _ret_to_fourcc(ret) == "Earg"
    del ctx


def api_reconfigure():
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    assert ctx.draw(1) == 0
    del ctx


def api_reconfigure_clearcolor(width=16, height=16):
    import zlib

    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    ret = ctx.configure(
        ngl.Config(offscreen=True, width=width, height=height, backend=_backend, capture_buffer=capture_buffer)
    )
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0xB4BD32FA
    ret = ctx.configure(
        ngl.Config(
            offscreen=True,
            width=width,
            height=height,
            backend=_backend,
            capture_buffer=capture_buffer,
            clear_color=(0.4, 0.4, 0.4, 1.0),
        )
    )
    assert ret == 0
    assert ctx.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0x05C44869
    del capture_buffer
    del ctx


def api_reconfigure_fail():
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    ret = ctx.configure(ngl.Config(offscreen=False, backend=_backend))
    assert ret != 0
    assert ctx.draw(1) != 0
    del ctx


def api_resize_fail():
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    ret = ctx.resize(32, 32)
    assert ret != 0
    del ctx


def api_capture_buffer(width=16, height=16):
    import zlib

    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    for _ in range(2):
        capture_buffer = bytearray(width * height * 4)
        assert ctx.set_capture_buffer(capture_buffer) == 0
        assert ctx.draw(0) == 0
        assert ctx.set_capture_buffer(None) == 0
        assert ctx.draw(0) == 0
        assert zlib.crc32(capture_buffer) == 0xB4BD32FA
    del ctx


def api_ctx_ownership():
    ctx = ngl.Context()
    ctx2 = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    ret = ctx2.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
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
        ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
        assert ret == 0
        ret = ctx2.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
        assert ret == 0
        quad = ngl.Quad()
        render1 = _get_scene(quad)
        if not shared:
            quad = ngl.Quad()
        render2 = _get_scene(quad)
        root = ngl.Group([render1.root, render2.root])
        scene = ngl.Scene.from_params(root)
        assert ctx.set_scene(render2) == 0
        assert ctx.draw(0) == 0
        assert ctx2.set_scene(scene) != 0
        assert ctx2.draw(0) == 0
        del ctx
        del ctx2


def api_capture_buffer_lifetime(width=1024, height=1024):
    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    ret = ctx.configure(
        ngl.Config(offscreen=True, width=width, height=height, backend=_backend, capture_buffer=capture_buffer)
    )
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
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend, hud=True))
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    for i in range(60 * 3):
        assert ctx.draw(i / 60.0) == 0
    del ctx


def api_hud_csv(width=16, height=16):
    ctx = ngl.Context()

    # We can't use NamedTemporaryFile because we may not be able to open it
    # twice on some systems
    fd, csvpath = tempfile.mkstemp(suffix=".csv", prefix="ngl-test-hud-")
    os.close(fd)
    atexit.register(lambda: os.remove(csvpath))

    ret = ctx.configure(
        ngl.Config(offscreen=True, width=width, height=height, backend=_backend, hud=True, hud_export_filename=csvpath)
    )
    assert ret == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    for t in [0.0, 0.15, 0.30, 0.45, 1.0]:
        # Try to set a locale that messes up the representation of floats
        try:
            locale.setlocale(locale.LC_ALL, "fr_FR.UTF-8")
        except locale.Error:
            print("unable to set french locale")

        assert ctx.draw(t) == 0
    del ctx

    with open(csvpath) as csvfile:
        reader = csv.DictReader(csvfile)
        time_column = [row["time"] for row in reader]

    assert time_column == ["0.000000", "0.150000", "0.300000", "0.450000", "1.000000"], time_column


def _api_text_live_change(width=320, height=240, font_files=None):
    import zlib

    ctx = ngl.Context()
    capture_buffer = bytearray(width * height * 4)
    ret = ctx.configure(
        ngl.Config(offscreen=True, width=width, height=height, backend=_backend, capture_buffer=capture_buffer)
    )
    assert ret == 0

    # An empty string forces the text node to deal with a pipeline with nul
    # attributes, this is what we exercise here, along with a varying up and
    # down number of characters
    text_strings = ["foo", "", "foobar", "world", "hello\nworld", "\n\n", "last"]

    # Exercise the diamond-form/prepare mechanism
    text_node = ngl.Text(font_files=font_files)
    root = autogrid_simple([text_node] * 4)
    scene = ngl.Scene.from_params(root)
    assert ctx.set_scene(scene) == 0

    ctx.draw(0)
    last_crc = zlib.crc32(capture_buffer)
    for i, s in enumerate(text_strings, 1):
        text_node.set_text(s)
        ctx.draw(i)
        crc = zlib.crc32(capture_buffer)
        assert crc != last_crc
        last_crc = crc


def api_text_live_change():
    return _api_text_live_change()


def api_text_live_change_with_font():
    font_files = Path(__file__).resolve().parent / "assets" / "fonts" / "Quicksand-Medium.ttf"
    return _api_text_live_change(font_files=font_files.as_posix())


def _ret_to_fourcc(ret):
    if ret >= 0:
        return None
    x = -ret
    return chr(x >> 24) + chr(x >> 16 & 0xFF) + chr(x >> 8 & 0xFF) + chr(x & 0xFF)


def api_media_sharing_failure():
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    m = ngl.Media("/dev/null")
    root = ngl.Group(children=(ngl.Texture2D(data_src=m), ngl.Texture2D(data_src=m)))
    scene = ngl.Scene.from_params(root)
    assert _ret_to_fourcc(ctx.set_scene(scene)) == "Eusg"  # Usage error


def api_denied_node_live_change(width=320, height=240):
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0

    root = ngl.Translate(ngl.Group())
    scene = ngl.Scene.from_params(root)

    # Check that we can live change but not into a node
    assert ctx.set_scene(scene) == 0
    assert root.set_vector(1, 2, 3) == 0
    assert root.set_vector(ngl.UniformVec3(value=(3, 2, 1))) != 0

    # Check that we can do the change after a reset of the context
    assert ctx.set_scene(None) == 0
    assert root.set_vector(ngl.UniformVec3(value=(4, 5, 6))) == 0

    # Check that we can not live unplug a node from a live changeable parameter
    assert ctx.set_scene(scene) == 0
    assert root.set_vector(ngl.UniformVec3(value=(7, 8, 9))) != 0


def api_livectls():
    # Build a scene and extract its live controls
    rng = random.Random(0)
    root = ngl.Group(
        children=(
            ngl.UniformBool(live_id="b"),
            ngl.UniformFloat(live_id="f"),
            ngl.UniformIVec3(live_id="iv3"),
            ngl.UserSwitch(
                ngl.Group(
                    children=(
                        ngl.UniformMat4(live_id="m4"),
                        ngl.UniformColor(live_id="clr"),
                        ngl.UniformQuat(as_mat4=True, live_id="rot"),
                    )
                ),
                live_id="switch",
            ),
            ngl.Text(live_id="txt"),
        )
    )
    scene = ngl.Scene.from_params(root)
    livectls = ngl.get_livectls(scene)
    assert len(livectls) == 8

    # Attach scene and run a dummy draw to make sure it's valid
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0

    # Apply live changes on nodes previously tracked by get_livectls()
    values = dict(
        b=True,
        f=rng.uniform(-1, 1),
        iv3=[rng.randint(-100, 100) for _ in range(3)],
        switch=False,
        m4=[rng.uniform(-1, 1) for _ in range(16)],
        clr=(0.9, 0.3, 0.8),
        rot=(0.1, -0.2, 0.5, -0.3),
        txt="test string",
    )
    for live_id, value in values.items():
        node = livectls[live_id]["node"]
        node_type = livectls[live_id]["node_type"]
        assert node_type == node.__class__.__name__
        if node_type == "UserSwitch":
            node.set_enabled(value)
        elif node_type == "Text":
            node.set_text(value)
        elif hasattr(value, "__iter__"):
            node.set_value(*value)
        else:
            node.set_value(value)

    # Detach scene from context and grab all live controls again
    assert ctx.set_scene(None) == 0
    livectls = ngl.get_livectls(scene)

    # Inspect nodes to check if they were properly altered by the live changes
    for live_id, expected_value in values.items():
        value = livectls[live_id]["val"]
        node_type = livectls[live_id]["node_type"]
        if node_type == "Text":
            assert value == expected_value, (value, expected_value)
        elif hasattr(value, "__iter__"):
            assert all(math.isclose(v, e, rel_tol=1e-6) for v, e in zip(value, expected_value))
        else:
            assert math.isclose(value, expected_value, rel_tol=1e-6)


def api_reset_scene(width=320, height=240):
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0
    render = _get_scene()
    assert ctx.set_scene(render) == 0
    ctx.draw(0)
    assert ctx.set_scene(None) == 0
    ctx.draw(1)
    assert ctx.set_scene(render) == 0
    ctx.draw(2)
    assert ctx.set_scene(None) == 0
    ctx.draw(3)


def api_shader_init_fail(width=320, height=240):
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0

    render = ngl.Render(ngl.Quad(), ngl.Program(vertex="<bug>", fragment="<bug>"))
    scene = ngl.Scene.from_params(render)

    assert ctx.set_scene(scene) != 0
    assert ctx.set_scene(scene) != 0  # another try to make sure the state stays consistent
    assert ctx.draw(0) == 0


def _create_trf(scene, start, end, prefetch_time=None):
    trf = ngl.TimeRangeFilter(scene, start, end)
    if prefetch_time is not None:
        trf.set_prefetch_time(prefetch_time)
    return trf


def _create_trf_scene(start, end, keep_active=False):
    texture = ngl.Texture2D(width=64, height=64)
    # A subgraph using a RTT will produce a clear crash if its draw is called without a prefetch
    rtt = ngl.RenderToTexture(ngl.Identity(), clear_color=(1.0, 0.0, 0.0, 1.0), color_textures=(texture,))
    render = ngl.RenderTexture(texture=texture)
    group = ngl.Group(children=(rtt, render))
    trf = _create_trf(group, start, start + 1)

    trf_start = _create_trf(trf, start, start + 1)
    # This group could be any node as long as it has no prefetch/release callback
    group = ngl.Group(children=(trf,))
    trf_end = _create_trf(group, end - 1.0, end + 1.0)

    children = [trf_start, trf_end]

    if keep_active:
        # This TimeRangeFilter keeps the underyling graph active while preventing
        # the update/draw operations to descent into the graph for the [start, end]
        # time interval
        offset = 10.0
        prefetch_time = offset + end - start
        trf_keep_active = _create_trf(group, end + offset, end + offset + 1.0, prefetch_time)
        children += [trf_keep_active]

    root = ngl.Group(children=children)
    return ngl.Scene.from_params(root)


def api_trf_seek(width=320, height=240):
    """
    Run a special time sequence on a particularly crafted time filtered
    diamond-tree graph to detect potential release/prefetch issues.
    """
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0

    start = 0.0
    end = 10.0
    scene = _create_trf_scene(start, end)
    ret = ctx.set_scene(scene)
    assert ret == 0

    # The following time sequence is designed to create an inconsistent time state
    assert ctx.draw(end) == 0
    assert ctx.draw(start) == 0
    assert ctx.draw(end) == 0


def api_trf_seek_keep_alive(width=320, height=240):
    """
    Run a special time sequence on a particularly crafted time filtered
    diamond-tree graph (with nodes being kept active artificially) to detect
    potential release/prefetch issues.
    """
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0

    start = 0.0
    end = 10.0
    scene = _create_trf_scene(start, end, True)
    ret = ctx.set_scene(scene)
    assert ret == 0

    # The following time sequence is designed to create an inconsistent time state
    assert ctx.draw(end) == 0
    assert ctx.draw(start) == 0
    assert ctx.draw(end) == 0


def api_dot(width=320, height=240):
    """
    Exercise the ngl.dot() API.
    """
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.dot(0.0) is not None
    assert ctx.dot(1.0) is not None


def api_probing():
    """
    Exercise the probing APIs; the result is platform/hardware specific so
    tricky to test its output
    """
    backends = ngl.get_backends()
    probe = ngl.probe_backends()
    pprint.pprint(backends)
    pprint.pprint(probe)
