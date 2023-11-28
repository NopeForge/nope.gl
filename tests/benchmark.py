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

import colorsys
import textwrap

from pynopegl_utils.misc import MEDIA_FILES_DB, get_shader, load_media
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.cmp_resources import test_resources

import pynopegl as ngl


def _get_random_easing(rng):
    easings = (
        "linear",
        "exp_in",
        "exp_out",
        "bounce_in",
        "bounce_out",
        "elastic_in",
        "elastic_out",
        "back_in",
        "back_out",
    )
    return rng.choice(easings)


_mix = lambda a, b, x: a * (1 - x) + b * x


def _get_random_animated(rng, t0, t1, value_gen, cls_kf, cls):
    nb_kfs = rng.randint(2, 5)
    animkf = [cls_kf(_mix(t0, t1, i / nb_kfs), value_gen(rng), _get_random_easing(rng)) for i in range(0, nb_kfs + 1)]
    return cls(animkf)


def _get_random_animated_float(rng, t0, t1, value_gen):
    return _get_random_animated(rng, t0, t1, value_gen, ngl.AnimKeyFrameFloat, ngl.AnimatedFloat)


def _get_random_animated_vec3(rng, t0, t1, value_gen):
    return _get_random_animated(rng, t0, t1, value_gen, ngl.AnimKeyFrameVec3, ngl.AnimatedVec3)


def _get_random_animated_color(rng, t0, t1):
    cls = lambda animkf: ngl.AnimatedColor(animkf, space="hsl")
    return _get_random_animated(rng, t0, t1, _get_random_color_hsl, ngl.AnimKeyFrameColor, cls)


def _get_random_color_hsl(rng):
    return (rng.random(), rng.uniform(0.7, 1.0), rng.uniform(0.5, 0.8))


def _get_random_color(rng):
    h, s, l = _get_random_color_hsl(rng)
    return colorsys.hls_to_rgb(h, l, s)


def _get_random_opacity(rng):
    return rng.uniform(0.1, 0.9)


def _get_random_angle(rng):
    return rng.uniform(-360, 360)


def _get_random_factors(rng):
    return (rng.uniform(0.8, 1.2), rng.uniform(0.8, 1.2), rng.uniform(0.8, 1.2))


def _get_random_animated_opacity(rng, t0, t1):
    return _get_random_animated_float(rng, t0, t1, _get_random_opacity)


def _get_random_position(rng):
    return (rng.uniform(-1, 1), rng.uniform(-1, 1), 0)


def _get_random_anchor(rng):
    return (rng.uniform(-0.2, 0.2), rng.uniform(-0.2, 0.2), 0)


def _get_random_geometry(rng):
    circle = lambda rng: ngl.Circle(
        radius=rng.uniform(1 / 4, 3 / 4),
        npoints=rng.randint(5, 25),
    )
    quad = lambda rng: ngl.Quad(
        corner=_get_random_position(rng),
        width=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0),
        height=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0),
    )
    triangle = lambda rng: ngl.Triangle(
        edge0=_get_random_position(rng),
        edge1=_get_random_position(rng),
        edge2=_get_random_position(rng),
    )
    shape_func = rng.choice((circle, quad, triangle))
    return shape_func(rng)


_MEDIA_UIDS = list(MEDIA_FILES_DB.keys())


def _get_random_texture(cfg: ngl.SceneCfg, rng):
    media_uid = rng.choice(_MEDIA_UIDS)
    texture = cfg.texture_cache.get(
        media_uid,
        ngl.Texture2D(
            data_src=ngl.Media(load_media(cfg, media_uid).filename),
            min_filter="nearest",
            mag_filter="nearest",
        ),
    )
    cfg.texture_cache[media_uid] = texture
    return texture


def _get_random_rendertexture(cfg: ngl.SceneCfg, rng):
    return ngl.RenderTexture(
        texture=_get_random_texture(cfg, rng),
        geometry=_get_random_geometry(rng),
        blending="src_over",
    )


def _get_random_text(rng):
    words = """
        Broccoli Cabbage Mushroom Cucumber Kombu Potato Celery Lettuce Garlic
        Pumpkin Leek Bulbasaur Monkey Goose Axolotl Capybara Wombat Kakapo
        Yip-yip! Nyaa~ Oulala Hehehe Puni-puni Tutuluu~ Gaoo~ Unchi! Maho?
    """
    return ngl.Text(
        box_width=(1, 0, 0),
        box_height=(0, 1, 0),
        box_corner=(-0.5, -0.5, 0),
        text=rng.choice(words.split()),
        fg_color=_get_random_color(rng),
        fg_opacity=_get_random_opacity(rng),
        bg_opacity=0,
    )


def _get_random_compute(cfg: ngl.SceneCfg, rng, t0, t1):
    count = 10

    vertex_shader = textwrap.dedent(
        """
        void main()
        {
            vec4 position = vec4(ngl_position, 1.0) + vec4(pos.data[ngl_instance_index], 0.0, 0.0);
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * position;
            var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
        }
        """
    )

    funcs = ("snake", "circle", "spread")

    compute_shader = textwrap.dedent(
        """
        #define TAU (2.0 * 3.14159265358979323846)

        vec2 snake(float i)
        {
            return vec2(i * 2.0 - 1.0, sin((time + i) * TAU));
        }

        vec2 circle(float i)
        {
            float angle = (time + i) * TAU;
            return vec2(sin(angle), cos(angle));
        }

        vec2 spread(float i)
        {
            float angle = i * TAU;
            return vec2(sin(angle), cos(angle)) * (time + i);
        }

        void main()
        {
            uint wg = gl_WorkGroupID.x;
            float i = float(wg) / float(gl_NumWorkGroups.x - 1U);
            pos.data[wg] = FUNC(i);
        }
        """.replace(
            "FUNC", rng.choice(funcs)
        )
    )

    pos = ngl.Block(fields=[ngl.BufferVec2(count=count, label="data")])

    time_animkf = [ngl.AnimKeyFrameFloat(t0, 0), ngl.AnimKeyFrameFloat(t1, 1)]

    compute_prog = ngl.ComputeProgram(compute_shader, workgroup_size=(1, 1, 1))
    compute_prog.update_properties(pos=ngl.ResourceProps(writable=True))

    compute = ngl.Compute(workgroup_count=(count, 1, 1), program=compute_prog)
    compute.update_resources(time=ngl.AnimatedFloat(time_animkf), pos=pos)

    geometry = _get_random_geometry(rng)
    program = ngl.Program(vertex=vertex_shader, fragment=get_shader("texture.frag"))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2())
    render = ngl.Render(geometry, program, nb_instances=count, blending="src_over")
    render.update_frag_resources(tex0=_get_random_texture(cfg, rng))
    render.update_vert_resources(pos=pos)
    render = ngl.Scale(render, factors=(0.5, 0.5, 0))

    return ngl.Group(children=(compute, render))


def _get_random_render(cfg: ngl.SceneCfg, rng, t0, t1, enable_computes):
    color = lambda rng: ngl.RenderColor(
        color=_get_random_animated_color(rng, t0, t1),
        opacity=_get_random_animated_opacity(rng, t0, t1),
        geometry=_get_random_geometry(rng),
        blending="src_over",
    )
    gradient = lambda rng: ngl.RenderGradient(
        color0=_get_random_animated_color(rng, t0, t1),
        color1=_get_random_animated_color(rng, t0, t1),
        opacity0=_get_random_animated_opacity(rng, t0, t1),
        opacity1=_get_random_animated_opacity(rng, t0, t1),
        pos0=(rng.random(), rng.random()),
        pos1=(rng.random(), rng.random()),
        mode=rng.choice(("ramp", "radial")),
        geometry=_get_random_geometry(rng),
        blending="src_over",
    )
    texture = lambda rng: _get_random_rendertexture(cfg, rng)
    text = lambda rng: _get_random_text(rng)
    compute = lambda rng: _get_random_compute(cfg, rng, t0, t1)
    render_funcs = [
        color,
        gradient,
        text,
        texture,
    ]
    if enable_computes:
        render_funcs.append(compute)
    return rng.choice(render_funcs)(rng)


def _get_random_transform(rng, t0, t1, child):
    translate = lambda rng, child: ngl.Translate(
        child,
        vector=_get_random_animated_vec3(rng, t0, t1, _get_random_position),
    )
    scale = lambda rng, child: ngl.Scale(
        child,
        factors=_get_random_animated_vec3(rng, t0, t1, _get_random_factors),
        anchor=_get_random_anchor(rng),
    )
    rotate = lambda rng, child: ngl.Rotate(
        child,
        angle=_get_random_animated_float(rng, t0, t1, _get_random_angle),
        anchor=_get_random_anchor(rng),
    )

    for _ in range(rng.randint(1, 2)):
        trf_func = rng.choice((translate, scale, rotate))
        child = trf_func(rng, child)
    return child


def _get_random_time_range(rng, t0, t1):
    t_amp = rng.uniform(4, 12)
    t_start = max(rng.uniform(t0, t1) - t_amp / 2, 0)
    t_end = t_start + t_amp / 2
    return t_start, t_end


def _get_random_layer(cfg: ngl.SceneCfg, rng, t0, t1, enable_computes, layer=4):
    nb_elems = rng.randint(2, 5)
    children = []
    sub_layers = rng.sample(range(nb_elems), 2)
    for i in range(nb_elems):
        if i in sub_layers and layer != 0:
            # Recursively create another layer
            child = _get_random_layer(cfg, rng, t0, t1, enable_computes, layer=layer - 1)
            child = _get_random_transform(rng, t0, t1, child)
            child.set_label(f"layer={layer}")

            # Create a (small) RTT of the children
            rtt_tex = ngl.Texture2D(
                width=rng.randint(50, 90),
                height=rng.randint(15, 70),
                min_filter="nearest",
                mag_filter="nearest",
            )
            rtt = ngl.RenderToTexture(
                child,
                clear_color=_get_random_color(rng) + (1,),
                forward_transforms=True,
            )
            rtt.add_color_textures(rtt_tex)
            rtt_render = ngl.RenderTexture(
                rtt_tex,
                geometry=_get_random_geometry(rng),
                blending="src_over",
            )
            rtt_render = _get_random_transform(rng, t0, t1, rtt_render)
            t_start, t_end = _get_random_time_range(rng, t0, t1)
            rtt_group = ngl.Group(children=(rtt, rtt_render))
            t_filter = ngl.TimeRangeFilter(rtt_group, t_start, t_end)

            # Draw both the children and the rendered texture
            child = ngl.Group(children=(t_filter, child))

        else:
            # We are at a leaf (last layer) so we create a random render
            t_start, t_end = _get_random_time_range(rng, t0, t1)
            child = _get_random_render(cfg, rng, t_start, t_end, enable_computes)
            if rng.random() < 1 / 3:
                child = _get_random_transform(rng, t_start, t_end, child)
            child = ngl.TimeRangeFilter(child, t_start, t_end)
        children.append(child)
    return ngl.Group(children=children)


def _get_scene(cfg: ngl.SceneCfg, seed=0, enable_computes=True):
    cfg.duration = 30
    cfg.aspect_ratio = (16, 9)
    rng = cfg.rng
    rng.seed(seed)
    cfg.texture_cache = {}

    t0, t1 = 0, cfg.duration

    # Some discrete abstract background
    bg = ngl.RenderGradient4(
        color_tl=_get_random_animated_color(rng, t0, t1),
        color_tr=_get_random_animated_color(rng, t0, t1),
        color_br=_get_random_animated_color(rng, t0, t1),
        color_bl=_get_random_animated_color(rng, t0, t1),
        filters=[ngl.FilterExposure(-1.5)],
    )

    # Always update all the textures to avoid time jumps
    textures = list(cfg.texture_cache.values())

    # The main overlay
    root = _get_random_layer(cfg, rng, t0, t1, enable_computes)

    group = ngl.Group(children=textures + [root])

    camera = ngl.Camera(group)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)
    return ngl.Group(children=(bg, camera))


@ngl.scene(controls=dict(seed=ngl.scene.Range(range=[0, 1000]), enable_computes=ngl.scene.Bool()))
def benchmark_test(cfg: ngl.SceneCfg, seed=82, enable_computes=True):
    """Function to be used for manual testing"""
    return _get_scene(cfg, seed, enable_computes)


@test_fingerprint(width=1920, height=1080, keyframes=120, tolerance=4)
@ngl.scene()
def benchmark_fingerprint_with_compute(cfg: ngl.SceneCfg):
    return _get_scene(cfg, seed=0, enable_computes=True)


@test_fingerprint(width=1920, height=1080, keyframes=120, tolerance=4)
@ngl.scene()
def benchmark_fingerprint_without_compute(cfg: ngl.SceneCfg):
    return _get_scene(cfg, seed=1, enable_computes=False)


@test_resources(keyframes=60)
@ngl.scene()
def benchmark_resources_with_compute(cfg: ngl.SceneCfg):
    return _get_scene(cfg, seed=2, enable_computes=True)


@test_resources(keyframes=60)
@ngl.scene()
def benchmark_resources_without_compute(cfg: ngl.SceneCfg):
    return _get_scene(cfg, seed=3, enable_computes=False)
