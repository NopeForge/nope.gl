import array
import math
import os.path as op

from pynopegl_utils.misc import get_shader, load_media
from pynopegl_utils.toolbox.scenes import compare
from pynopegl_utils.toolbox.shapes import equilateral_triangle_coords

import pynopegl as ngl


@ngl.scene(controls=dict(xsplit=ngl.scene.Range(range=[0, 1], unit_base=100), trilinear=ngl.scene.Bool()))
def lut3d(cfg: ngl.SceneCfg, xsplit=0.3, trilinear=True):
    """Lookup Table 3D using a Texture3D"""
    level = 6
    level2 = level**2

    # Generated with `ffmpeg -f lavfi -i haldclutsrc=6,curves=vintage,format=rgba
    # -f rawvideo -frames:v 1 lut3d.raw`
    lut3d_filename = op.join(op.dirname(__file__), "data", "lut3d.raw")
    lut3d_buf = ngl.BufferUBVec4(filename=lut3d_filename)
    lut3d_tex = ngl.Texture3D(data_src=lut3d_buf, width=level2, height=level2, depth=level2)
    lut3d_tex.set_min_filter("linear" if trilinear else "nearest")
    lut3d_tex.set_mag_filter("linear" if trilinear else "nearest")

    m0 = load_media("mire")
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    video = ngl.Media(m0.filename)
    video_tex = ngl.Texture2D(data_src=video)

    scene_tex = ngl.RenderTexture(video_tex)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    prog_lut = ngl.Program(fragment=get_shader("lut3d.frag"), vertex=get_shader("lut3d.vert"))
    prog_lut.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    scene_lut = ngl.Render(quad, prog_lut)
    scene_lut.update_frag_resources(tex0=video_tex, lut3d=lut3d_tex)

    return compare(cfg, scene_tex, scene_lut, xsplit)


@ngl.scene(controls=dict(size=ngl.scene.Range(range=[0, 2], unit_base=1000)))
def triangle(cfg: ngl.SceneCfg, size=4 / 3):
    """Rotating triangle with edge coloring specified in a vertex attribute"""
    cfg.duration = 3.0
    cfg.aspect_ratio = (1, 1)

    colors_data = array.array("f", [0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0])
    colors_buffer = ngl.BufferVec3(data=colors_data)

    p0, p1, p2 = equilateral_triangle_coords(size)
    triangle = ngl.Triangle(p0, p1, p2)
    p = ngl.Program(fragment=get_shader("color.frag"), vertex=get_shader("triangle.vert"))
    p.update_vert_out_vars(color=ngl.IOVec3())
    node = ngl.Render(triangle, p)
    node.update_attributes(edge_color=colors_buffer)
    node.update_frag_resources(opacity=ngl.UniformFloat(1))
    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration / 3.0, -360 / 3.0, "exp_in_out"),
        ngl.AnimKeyFrameFloat(2 * cfg.duration / 3.0, -2 * 360 / 3.0, "exp_in_out"),
        ngl.AnimKeyFrameFloat(cfg.duration, -360, "exp_in_out"),
    ]
    node = ngl.Rotate(node, angle=ngl.AnimatedFloat(animkf))
    return node


@ngl.scene(controls=dict(particles=ngl.scene.Range(range=[1, 1023])))
def particles(cfg: ngl.SceneCfg, particles=32):
    """Particules demo using compute shaders and instancing"""

    compute_shader = get_shader("particles.comp")
    vertex_shader = get_shader("particles.vert")
    fragment_shader = get_shader("color.frag")

    cfg.duration = 6

    x = 64
    p = x * particles

    positions = array.array("f")
    velocities = array.array("f")

    for _ in range(p):
        positions.extend(
            [
                cfg.rng.uniform(-1.0, 1.0),
                cfg.rng.uniform(0.0, 1.0),
                0.0,
            ]
        )

        velocities.extend(
            [
                cfg.rng.uniform(-0.01, 0.01),
                cfg.rng.uniform(-0.05, 0.05),
            ]
        )

    ipositions = ngl.Block(fields=[ngl.BufferVec3(data=positions, label="data")], layout="std430")
    ivelocities = ngl.Block(fields=[ngl.BufferVec2(data=velocities, label="data")], layout="std430")
    opositions = ngl.Block(fields=[ngl.BufferVec3(count=p, label="data")], layout="std430")

    animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 1)]
    utime = ngl.AnimatedFloat(animkf)
    uduration = ngl.UniformFloat(cfg.duration)

    cp = ngl.ComputeProgram(compute_shader, workgroup_size=(1, 1, 1))
    cp.update_properties(opositions=ngl.ResourceProps(writable=True))

    c = ngl.Compute(workgroup_count=(x, particles, 1), program=cp)
    c.update_resources(
        time=utime,
        duration=uduration,
        ipositions=ipositions,
        ivelocities=ivelocities,
        opositions=opositions,
    )

    quad_width = 0.01
    quad = ngl.Quad(corner=(-quad_width / 2, -quad_width / 2, 0), width=(quad_width, 0, 0), height=(0, quad_width, 0))
    p = ngl.Program(
        vertex=vertex_shader,
        fragment=fragment_shader,
    )
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    r = ngl.Render(quad, p, nb_instances=particles, blending="src_over")
    r.update_frag_resources(color=ngl.UniformVec3(value=(0, 0.6, 0.8)), opacity=ngl.UniformFloat(0.9))
    r.update_vert_resources(positions=opositions)

    g = ngl.Group()
    g.add_children(c, r)

    return ngl.Camera(g)


@ngl.scene()
def blending_and_stencil(cfg: ngl.SceneCfg):
    """Scene using blending and stencil graphic features"""
    cfg.duration = 5
    vertex = get_shader("color.vert")
    fragment = get_shader("color.frag")

    program = ngl.Program(vertex=vertex, fragment=fragment)
    circle = ngl.Circle(npoints=256)
    cloud_color = ngl.UniformVec3(value=(1, 1, 1))
    cloud_opacity = ngl.UniformFloat(0.4)

    main_group = ngl.Group()

    render = ngl.RenderColor(color=(0.2, 0.6, 1), label="sky")
    config = ngl.GraphicConfig(
        render,
        stencil_test=True,
        stencil_write_mask=0xFF,
        stencil_func="always",
        stencil_ref=1,
        stencil_read_mask=0xFF,
        stencil_fail="replace",
        stencil_depth_fail="replace",
        stencil_depth_pass="replace",
    )
    main_group.add_children(config)

    render = ngl.RenderColor(color=(1, 0.8, 0), geometry=circle, label="sun")

    scale = ngl.Scale(render, (0.15, 0.15, 0.0))
    translate = ngl.Translate(scale, (0.4, 0.3, 0))
    main_group.add_children(translate)

    cloud_group = ngl.Group(label="clouds")

    centers = [
        (-1.0, 0.85, 0.4),
        (-0.5, 2.0, 1.0),
        (0, 0.85, 0.4),
        (1.0, 1.55, 0.8),
        (0.6, 0.65, 0.075),
        (0.5, 1.80, 1.25),
    ]

    for center in centers:
        render = ngl.Render(circle, program, blending="src_over")
        render.update_frag_resources(color=cloud_color, opacity=cloud_opacity)

        factor = cfg.rng.random() * 0.4 + center[2]
        keyframe = cfg.duration * (cfg.rng.random() * 0.4 + 0.2)
        animkf = (
            ngl.AnimKeyFrameVec3(0, (factor, factor, 0)),
            ngl.AnimKeyFrameVec3(keyframe, (factor + 0.1, factor + 0.1, 0)),
            ngl.AnimKeyFrameVec3(cfg.duration, (factor, factor, 0)),
        )
        scale = ngl.Scale(render, factors=ngl.AnimatedVec3(animkf))

        translate = ngl.Translate(scale, vector=(center[0], center[1], 0))
        cloud_group.add_children(translate)

    config = ngl.GraphicConfig(
        cloud_group,
        stencil_test=True,
        stencil_write_mask=0x0,
        stencil_func="equal",
        stencil_ref=1,
        stencil_read_mask=0xFF,
        stencil_fail="keep",
        stencil_depth_fail="keep",
        stencil_depth_pass="keep",
    )
    main_group.add_children(config)

    camera = ngl.Camera(main_group)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_orthographic(-cfg.aspect_ratio_float, cfg.aspect_ratio_float, -1.0, 1.0)
    camera.set_clipping(1.0, 10.0)

    return camera


def _get_cube_quads():
    # fmt: off
    #         corner             width        height      color
    return (((-0.5, -0.5,  0.5), ( 1, 0,  0), (0, 1,  0), (1, 1, 0)),  # front
            (( 0.5, -0.5, -0.5), (-1, 0,  0), (0, 1,  0), (0, 0, 1)),  # back
            ((-0.5, -0.5, -0.5), ( 0, 0,  1), (0, 1,  0), (0, 1, 0)),  # left
            (( 0.5, -0.5,  0.5), ( 0, 0, -1), (0, 1,  0), (0, 1, 1)),  # right
            ((-0.5, -0.5, -0.5), ( 1, 0,  0), (0, 0,  1), (1, 0, 0)),  # bottom
            ((-0.5,  0.5,  0.5), ( 1, 0,  0), (0, 0, -1), (1, 0, 1)))  # top
    # fmt: on


def _get_cube_side(texture, program, corner, width, height, color):
    render = ngl.Render(ngl.Quad(corner, width, height), program)
    render.update_frag_resources(tex0=texture)
    render.update_frag_resources(blend_color=ngl.UniformVec3(value=color))
    render.update_frag_resources(mix_factor=ngl.UniformFloat(value=0.2))
    return render


@ngl.scene(controls=dict(display_depth_buffer=ngl.scene.Bool()))
def cube(cfg: ngl.SceneCfg, display_depth_buffer=False):
    """
    Cube with a common media Texture but a different color tainting on each side.
    Also includes a depth map visualization.
    """
    cube = ngl.Group(label="cube")

    vert_data = get_shader("texture.vert")
    frag_data = get_shader("tex-tint.frag")
    program = ngl.Program(vertex=vert_data, fragment=frag_data)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())

    media = load_media("mire")
    texture = ngl.Texture2D(data_src=ngl.Media(media.filename))
    children = [_get_cube_side(texture, program, qi[0], qi[1], qi[2], qi[3]) for qi in _get_cube_quads()]
    cube.add_children(*children)

    for i in range(3):
        rot_animkf = ngl.AnimatedFloat(
            [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))]
        )
        axis = tuple(int(i == x) for x in range(3))
        cube = ngl.Rotate(cube, axis=axis, angle=rot_animkf)

    config = ngl.GraphicConfig(cube, depth_test=True)

    camera = ngl.Camera(config)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)

    if not display_depth_buffer:
        return camera
    else:
        group = ngl.Group()

        depth_texture = ngl.Texture2D(min_filter="nearest", mag_filter="nearest")
        depth_texture.set_format("auto_depth")
        depth_texture.set_width(640)
        depth_texture.set_height(480)

        texture = ngl.Texture2D()
        texture.set_width(640)
        texture.set_height(480)
        rtt = ngl.RenderToTexture(camera)
        rtt.add_color_textures(texture)
        rtt.set_depth_texture(depth_texture)

        quad = ngl.Quad((-1.0, -1.0, 0), (1, 0, 0), (0, 1, 0))
        render = ngl.RenderTexture(texture, geometry=quad)
        group.add_children(rtt, render)

        quad = ngl.Quad((0.0, 0.0, 0), (1, 0, 0), (0, 1, 0))
        render = ngl.RenderTexture(depth_texture, geometry=quad)
        group.add_children(rtt, render)

        return group


@ngl.scene()
def quaternion(cfg: ngl.SceneCfg):
    """Animated quaternion used to rotate a plane"""
    cfg.duration = 10.0
    step = cfg.duration / 5.0
    x = math.sqrt(0.5)
    quat_animkf = [
        ngl.AnimKeyFrameQuat(0 * step, (0, 0, 0, 1)),
        ngl.AnimKeyFrameQuat(1 * step, (0, 0, -x, x)),
        ngl.AnimKeyFrameQuat(2 * step, (0, 1, 0, 0)),
        ngl.AnimKeyFrameQuat(3 * step, (1, 0, 0, 0)),
        ngl.AnimKeyFrameQuat(4 * step, (x, 0, 0, x)),
        ngl.AnimKeyFrameQuat(5 * step, (0, 0, 0, 1)),
    ]

    quat = ngl.AnimatedQuat(quat_animkf, as_mat4=True)
    render = ngl.RenderColor(color=(1.0, 1.0, 1.0))
    transform = ngl.Transform(render, matrix=quat)

    camera = ngl.Camera(transform)
    camera.set_eye(0.0, 0.0, 4.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)

    return camera


@ngl.scene(
    controls=dict(
        ndim=ngl.scene.Range(range=[1, 8]),
        nb_layers=ngl.scene.Range(range=[1, 10]),
        ref_color=ngl.scene.Color(),
        nb_mountains=ngl.scene.Range(range=[3, 15]),
    )
)
def mountain(cfg: ngl.SceneCfg, ndim=3, nb_layers=7, ref_color=(0.5, 0.75, 0.75), nb_mountains=6):
    """Mountain generated with a stack of noise shaders using Textures as random source"""
    random_dim = 1 << ndim
    cfg.aspect_ratio = (16, 9)
    cfg.duration = nb_mountains**2

    def get_rand():
        return array.array("f", [cfg.rng.uniform(0, 1) for _ in range(random_dim)])

    black, white = (0, 0, 0), (1, 1, 1)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    prog = ngl.Program(vertex=get_shader("texture.vert"), fragment=get_shader("mountain.frag"))
    prog.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    hscale = 1 / 2.0
    mountains = []
    for i in range(nb_mountains):
        yoffset = (nb_mountains - i - 1) / float(nb_mountains - 1) * (1.0 - hscale)

        if i < nb_mountains / 2:
            c0, c1 = ref_color, white
            x = (i + 1) / float(nb_mountains / 2 + 1)
        else:
            c0, c1 = black, ref_color
            x = (i - nb_mountains / 2) / float((nb_mountains - 1) / 2)
        mcolor = tuple(x * a + (1.0 - x) * b for a, b in zip(c0, c1))

        random_buf = ngl.BufferFloat(data=get_rand())
        random_tex = ngl.Texture2D(
            data_src=random_buf, width=random_dim, height=1, min_filter="nearest", mag_filter="nearest"
        )

        utime_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, i + 1)]
        utime = ngl.AnimatedFloat(utime_animkf)

        uyoffset_animkf = [
            ngl.AnimKeyFrameFloat(0, yoffset / 2.0),
            ngl.AnimKeyFrameFloat(cfg.duration / 2.0, yoffset),
            ngl.AnimKeyFrameFloat(cfg.duration, yoffset / 2.0),
        ]
        uyoffset = ngl.AnimatedFloat(uyoffset_animkf)

        render = ngl.Render(quad, prog, blending="src_over")
        render.update_frag_resources(tex0=random_tex)
        render.update_frag_resources(dim=ngl.UniformInt(random_dim))
        render.update_frag_resources(nb_layers=ngl.UniformInt(nb_layers))
        render.update_frag_resources(time=utime)
        render.update_frag_resources(lacunarity=ngl.UniformFloat(2.0))
        render.update_frag_resources(gain=ngl.UniformFloat(0.5))
        render.update_frag_resources(mcolor=ngl.UniformVec3(mcolor))
        render.update_frag_resources(yoffset=uyoffset)
        render.update_frag_resources(hscale=ngl.UniformFloat(hscale))

        mountains.append(render)

    sky = ngl.RenderColor(white[:3])

    group = ngl.Group(children=[sky] + mountains)
    return group


@ngl.scene()
def smptebars_glitch(cfg: ngl.SceneCfg):
    """SMPTE bars glitching at irregular intervals"""

    cfg.duration = 15
    cfg.aspect_ratio = (4, 3)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=get_shader("smptebars.vert"), fragment=get_shader("smptebars.frag"))
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    freq = cfg.framerate[0] / cfg.framerate[1] + 1
    render.update_frag_resources(
        active_noise=ngl.NoiseFloat(octaves=1),
        active_probability=ngl.UniformFloat(0.4),  # glitch 40% of the time
        uv_noise_0=ngl.NoiseVec2(amplitude=0.05, frequency=freq, seed=1000 + 0),
        uv_noise_1=ngl.NoiseVec2(amplitude=0.05, frequency=freq, seed=1000 + 0x7FFF),
        uv_noise_2=ngl.NoiseVec2(amplitude=0.05, frequency=freq, seed=1000 + 0xFFFF),
    )
    return render
