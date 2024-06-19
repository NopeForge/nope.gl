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
import hashlib
import locale
import math
import os
import pprint
import random
import tempfile
from collections import namedtuple
from pathlib import Path

from pynopegl_utils.misc import get_backend, load_media
from pynopegl_utils.toolbox.grid import autogrid_simple

import pynopegl as ngl

_backend_str = os.environ.get("BACKEND")
_backend = get_backend(_backend_str) if _backend_str else ngl.Backend.AUTO


def _get_scene():
    return ngl.Scene.from_params(ngl.DrawColor(geometry=ngl.Quad()))


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


def api_scene_context_transfer():
    """Test transfering a scene from one context to another"""
    scene = _get_scene()
    ctx = ngl.Context()
    ctx2 = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    ret = ctx2.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert ctx.set_scene(None) == 0
    assert ctx2.set_scene(scene) == 0
    assert ctx2.draw(0) == 0
    assert ctx2.set_scene(None) == 0


def api_scene_lifetime():
    """Test if the context is still working properly when we release the user scene ownership"""
    scene = _get_scene()
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    assert ctx.set_scene(scene) == 0
    del scene
    assert ctx.draw(0) == 0
    del ctx


def api_scene_mutate():
    """Test if the scene association is prevent graph structure changes"""
    hook = ngl.Group()
    eval = ngl.EvalVec3()
    draw = ngl.DrawColor(color=eval)
    root = ngl.Group(children=[hook])
    assert hook.add_children(draw) == 0
    scene = ngl.Scene.from_params(root)
    assert hook.add_children(ngl.DrawColor()) != 0
    assert draw.set_opacity(0.5) == 0  # we can change a direct value...
    assert draw.set_opacity(ngl.UniformFloat()) != 0  # ...but we can't change the structure
    assert eval.update_resources(t=ngl.Time()) != 0
    del scene


def api_scene_ownership():
    """Test if part of a graph is shared between 2 different scenes"""
    shared_geometry = ngl.Quad()
    scene0 = ngl.Scene.from_params(ngl.DrawColor(geometry=shared_geometry))
    try:
        scene1 = ngl.Scene.from_params(ngl.DrawColor(geometry=shared_geometry))
    except Exception:
        pass
    else:
        del scene1
        assert False
    del scene0


def api_scene_resilience():
    """Similar to API the scene ownership test but make sure the API is error resilient"""
    shared_geometry = ngl.Quad()
    scene0 = ngl.Scene.from_params(ngl.DrawColor(geometry=shared_geometry))

    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    assert ctx.set_scene(scene0) == 0
    assert ctx.draw(0) == 0

    try:
        scene1 = ngl.Scene.from_params(ngl.DrawColor(geometry=shared_geometry))
    except Exception:
        pass
    else:
        del scene1
        assert False

    assert ctx.draw(0) == 0
    del scene0
    del ctx


def api_scene_files():
    # Store abstract file references in the graph
    media = ngl.Media(filename="nope")
    root = ngl.Group(
        children=[
            ngl.Texture2D(data_src=media),
            ngl.Texture2D(data_src=ngl.BufferByte(filename="hamster")),
        ]
    )
    scene = ngl.Scene.from_params(root)

    # Update the ref after the node has been associated with its scene
    media.set_filename("cat")

    assert scene.files == ["cat", "hamster"]

    # Replace the simple filename strings with their corresponding actual file paths
    for i, filepath in enumerate(scene.files):
        new_filepath = load_media(filepath).filename
        scene.update_filepath(i, new_filepath)

    # Query again the files and check if they've been updated
    assert all(Path(filepath).exists() for filepath in scene.files)

    # Associate with the context
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=16, height=16, backend=_backend))
    assert ret == 0
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0

    # Change one path using live controls
    new_ref = "cat was here"
    media.set_filename(new_ref)

    # Check if the change is effective back in the scene
    assert any(filepath == new_ref for filepath in scene.files)

    # Detach the context and check if the change is still persistent like other live controls
    ctx.set_scene(None)
    del ctx
    assert any(filepath == new_ref for filepath in scene.files)


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


def _api_text_live_change(width=320, height=240, font_faces=None):
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
    text_node = ngl.Text(font_faces=font_faces)
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
    font_faces = Path(__file__).resolve().parent / "assets" / "fonts" / "Quicksand-Medium.ttf"
    return _api_text_live_change(font_faces=[ngl.FontFace(font_faces.as_posix())])


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

    # Check that we can not live unplug a node from a live changeable parameter
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
    draw = _get_scene()
    assert ctx.set_scene(draw) == 0
    ctx.draw(0)
    assert ctx.set_scene(None) == 0
    ctx.draw(1)
    assert ctx.set_scene(draw) == 0
    ctx.draw(2)
    assert ctx.set_scene(None) == 0
    ctx.draw(3)


def api_shader_init_fail(width=320, height=240):
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=width, height=height, backend=_backend))
    assert ret == 0

    draw = ngl.Draw(ngl.Quad(), ngl.Program(vertex="<bug>", fragment="<bug>"))
    scene = ngl.Scene.from_params(draw)

    assert ctx.set_scene(scene) != 0
    assert ctx.set_scene(scene) != 0  # another try to make sure the state stays consistent
    assert ctx.draw(0) == 0


def _create_trf(scene, start, end, prefetch_time=None):
    trf = ngl.TimeRangeFilter(scene, start, end)
    if prefetch_time is not None:
        trf.set_prefetch_time(prefetch_time)
    return trf


def _create_trf_scene(start, end, keep_active=False):
    texture = ngl.Texture2D(width=64, height=64, min_filter="nearest", mag_filter="nearest")
    # A subgraph using a RTT will produce a clear crash if its draw is called without a prefetch
    rtt = ngl.RenderToTexture(ngl.Identity(), clear_color=(1.0, 0.0, 0.0, 1.0), color_textures=(texture,))
    draw = ngl.DrawTexture(texture=texture)
    group = ngl.Group(children=(rtt, draw))
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


def api_caps():
    # Manually build a scene config with the default backend and explicit capabilities
    scene_cfg = ngl.SceneCfg(backend=next(k for k, v in ngl.get_backends().items() if v["is_default"]))
    backends = ngl.probe_backends()
    scene_cfg.caps = backends[scene_cfg.backend]["caps"]

    # Building this scene is expected to succeed because caps are consistent
    scene_func = ngl.scene()(lambda _: ngl.Group())
    scene_func(scene_cfg)

    # This one must fail because it lacks a capability
    scene_cfg.caps.pop(ngl.Cap.TEXT_LIBRARIES)
    try:
        scene_func(scene_cfg)
    except Exception:
        pass
    else:
        assert False


def api_get_backend():
    """
    Exercise the ngl.Context.get_backend() API; the result is platform/hardware
    specific so tricky to test its output
    """
    ctx = ngl.Context()
    cfg = ngl.Config(offscreen=True, width=32, height=32, backend=ngl.Backend.AUTO)
    try:
        backend = ctx.get_backend()
    except Exception:
        pass
    else:
        assert False
    ret = ctx.configure(cfg)
    assert ret == 0
    backend = ctx.get_backend()
    assert isinstance(backend["id"], ngl.Backend)
    assert backend["id"] != ngl.Backend.AUTO
    assert backend["is_default"] == True
    assert backend["caps"]
    pprint.pprint(backend)


def api_viewport():
    ctx = ngl.Context()
    ret = ctx.configure(ngl.Config(offscreen=True, width=640, height=480, backend=_backend))
    assert ret == 0
    assert ctx.viewport == (0, 0, 640, 480)

    scene = ngl.Scene.from_params(ngl.Group(), aspect_ratio=(8, 9))
    ctx.set_scene(scene)
    assert ctx.viewport == (107, 0, 426, 480)

    ctx.set_scene(None)
    assert ctx.viewport == (0, 0, 640, 480)


def api_transform_chain_check():
    invalid_chain = ngl.Translate(ngl.Rotate(ngl.Skew()))
    root = ngl.Camera(eye_transform=invalid_chain)
    try:
        ngl.Scene.from_params(root)
    except Exception:
        pass
    else:
        assert False


def _create_trf_scene_with_media(start, end):
    mi = load_media("city")
    media = ngl.Media(mi.filename)
    texture = ngl.Texture2D(data_src=media)
    draw = ngl.DrawTexture(texture=texture)
    group = ngl.Group(children=(draw,))
    trf = _create_trf(group, start, end)

    return ngl.Scene.from_params(trf)


def api_update_with_timeranges(width=320, height=240):
    """
    Exercise the `ngl_update()` API
    """
    ngl.log_set_min_level(ngl.Log.VERBOSE)
    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    ret = ctx.configure(
        ngl.Config(
            offscreen=True,
            width=width,
            height=height,
            backend=_backend,
            capture_buffer=capture_buffer,
            clear_color=(0.0, 0.0, 0.0, 0.0),
        )
    )
    assert ret == 0

    start = 5.0
    end = 10.0
    scene = _create_trf_scene_with_media(start, end)
    assert ctx.set_scene(scene) == 0

    initial_hash = hashlib.md5(capture_buffer).hexdigest()
    assert ctx.update(start - 0.5) == 0
    assert initial_hash == hashlib.md5(capture_buffer).hexdigest()

    assert ctx.draw(start) == 0
    draw_hash = hashlib.md5(capture_buffer).hexdigest()
    assert initial_hash != draw_hash

    assert ctx.draw(end - 1.0) == 0
    assert draw_hash == hashlib.md5(capture_buffer).hexdigest()

    assert ctx.update(end + 1.5) == 0
    assert draw_hash == hashlib.md5(capture_buffer).hexdigest()

    assert ctx.draw(end + 1.5) == 0
    assert initial_hash == hashlib.md5(capture_buffer).hexdigest()

    assert ctx.update(end - 1.0) == 0
    assert initial_hash == hashlib.md5(capture_buffer).hexdigest()

    assert ctx.draw(end - 1.0) == 0
    assert draw_hash == hashlib.md5(capture_buffer).hexdigest()

    assert ctx.draw(0.0) == 0
    assert initial_hash == hashlib.md5(capture_buffer).hexdigest()
