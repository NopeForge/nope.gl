import array
import colorsys
import math
import os.path as op

from pynopegl_utils.misc import SceneCfg, scene
from pynopegl_utils.toolbox.scenes import compare
from pynopegl_utils.toolbox.shapes import equilateral_triangle_coords

import pynopegl as ngl


@scene(xsplit=scene.Range(range=[0, 1], unit_base=100), trilinear=scene.Bool())
def lut3d(cfg: SceneCfg, xsplit=0.3, trilinear=True):
    """Lookup Table 3D using a Texture3D"""
    level = 6
    level2 = level**2

    # Generated with `ffmpeg -f lavfi -i haldclutsrc=6,curves=vintage,format=rgba
    # -f rawvideo -frames:v 1 lut3d.raw`
    lut3d_filename = op.join(op.dirname(__file__), "data", "lut3d.raw")
    cfg.files.append(lut3d_filename)
    lut3d_buf = ngl.BufferUBVec4(filename=lut3d_filename)
    lut3d_tex = ngl.Texture3D(data_src=lut3d_buf, width=level2, height=level2, depth=level2)
    if trilinear:
        lut3d_tex.set_min_filter("linear")
        lut3d_tex.set_mag_filter("linear")

    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    video = ngl.Media(m0.filename)
    video_tex = ngl.Texture2D(data_src=video)

    scene_tex = ngl.RenderTexture(video_tex)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    prog_lut = ngl.Program(fragment=cfg.get_frag("lut3d"), vertex=cfg.get_vert("lut3d"))
    prog_lut.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    scene_lut = ngl.Render(quad, prog_lut)
    scene_lut.update_frag_resources(tex0=video_tex, lut3d=lut3d_tex)

    return compare(cfg, scene_tex, scene_lut, xsplit)


@scene(bgcolor1=scene.Color(), bgcolor2=scene.Color(), bilinear_filtering=scene.Bool())
def buffer_dove(cfg: SceneCfg, bgcolor1=(0.6, 0, 0), bgcolor2=(0.8, 0.8, 0), bilinear_filtering=True):
    """Blending of a Render using a Buffer as data source"""
    cfg.duration = 3.0

    # Credits: https://icons8.com/icon/40514/dove
    # (Raw data is the premultiplied)
    icon_filename = op.join(op.dirname(__file__), "data", "icons8-dove.raw")
    cfg.files.append(icon_filename)
    w, h = (96, 96)
    cfg.aspect_ratio = (w, h)

    img_buf = ngl.BufferUBVec4(filename=icon_filename, label="icon raw buffer")

    img_tex = ngl.Texture2D(data_src=img_buf, width=w, height=h)
    if bilinear_filtering:
        img_tex.set_mag_filter("linear")
    quad = ngl.Quad((-0.5, -0.5, 0.1), (1, 0, 0), (0, 1, 0))
    render = ngl.RenderTexture(img_tex, geometry=quad, blending="src_over")

    shape_bg = ngl.Circle(radius=0.6, npoints=256)
    color_animkf = [
        ngl.AnimKeyFrameColor(0, bgcolor1),
        ngl.AnimKeyFrameColor(cfg.duration / 2.0, bgcolor2),
        ngl.AnimKeyFrameColor(cfg.duration, bgcolor1),
    ]
    ucolor = ngl.AnimatedColor(color_animkf)
    render_bg = ngl.RenderColor(ucolor, geometry=shape_bg, label="background")

    return ngl.Group(children=(render_bg, render))


@scene(size=scene.Range(range=[0, 2], unit_base=1000))
def triangle(cfg: SceneCfg, size=4 / 3):
    """Rotating triangle with edge coloring specified in a vertex attribute"""
    cfg.duration = 3.0
    cfg.aspect_ratio = (1, 1)

    colors_data = array.array("f", [0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0])
    colors_buffer = ngl.BufferVec3(data=colors_data)

    p0, p1, p2 = equilateral_triangle_coords(size)
    triangle = ngl.Triangle(p0, p1, p2)
    p = ngl.Program(fragment=cfg.get_frag("color"), vertex=cfg.get_vert("triangle"))
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


@scene(n=scene.Range(range=[2, 10]))
def fibo(cfg: SceneCfg, n=8):
    """Fibonacci with a recursive tree (nodes inherit transforms)"""
    cfg.duration = 5.0
    cfg.aspect_ratio = (1, 1)

    fib = [0, 1, 1]
    for i in range(2, n):
        fib.append(fib[i] + fib[i - 1])
    fib = fib[::-1]

    shift = 1 / 3.0  # XXX: what's the exact math here?
    shape_scale = 1.0 / ((2.0 - shift) * sum(fib))

    orig = (-shift, -shift, 0)
    g = None
    root = None
    for i, x in enumerate(fib[:-1]):
        w = x * shape_scale
        gray = 1.0 - i / float(n)
        color = (gray, gray, gray)
        q = ngl.Quad(orig, (w, 0, 0), (0, w, 0))
        render = ngl.RenderColor(color, geometry=q)

        new_g = ngl.Group()
        animkf = [
            ngl.AnimKeyFrameFloat(0, 90),
            ngl.AnimKeyFrameFloat(cfg.duration / 2, -90, "exp_in_out"),
            ngl.AnimKeyFrameFloat(cfg.duration, 90, "exp_in_out"),
        ]
        rot = ngl.Rotate(new_g, anchor=orig, angle=ngl.AnimatedFloat(animkf))
        if g:
            g.add_children(rot)
        else:
            root = rot
        g = new_g
        new_g.add_children(render)
        orig = (orig[0] + w, orig[1] + w, 0)

    assert root is not None
    return root


@scene(dim=scene.Range(range=[1, 50]))
def cropboard(cfg: SceneCfg, dim=15):
    """Divided media using instancing draw and UV coords offsetting from a buffer"""
    m0 = cfg.medias[0]
    cfg.duration = 10
    cfg.aspect_ratio = (m0.width, m0.height)

    kw = kh = 1.0 / dim
    qw = qh = 2.0 / dim

    p = ngl.Program(vertex=cfg.get_vert("cropboard"), fragment=cfg.get_frag("texture"))
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    m = ngl.Media(m0.filename)
    t = ngl.Texture2D(data_src=m)

    uv_offset_buffer = array.array("f")
    translate_a_buffer = array.array("f")
    translate_b_buffer = array.array("f")

    q = ngl.Quad(
        corner=(0, 0, 0), width=(qw, 0, 0), height=(0, qh, 0), uv_corner=(0, 0), uv_width=(kw, 0), uv_height=(0, kh)
    )

    for y in range(dim):
        for x in range(dim):
            uv_offset = [x * kw, (y + 1.0) * kh - 1.0]
            src = [cfg.rng.uniform(-2, 2), cfg.rng.uniform(-2, 2)]
            dst = [x * qw - 1.0, 1.0 - (y + 1.0) * qh]

            uv_offset_buffer.extend(uv_offset)
            translate_a_buffer.extend(src)
            translate_b_buffer.extend(dst)

    utime_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration * 2 / 3.0, 1, "exp_out")]
    utime = ngl.AnimatedFloat(utime_animkf)

    render = ngl.Render(q, p, nb_instances=dim**2)
    render.update_frag_resources(tex0=t)
    render.update_vert_resources(time=utime)
    render.update_instance_attributes(
        uv_offset=ngl.BufferVec2(data=uv_offset_buffer),
        translate_a=ngl.BufferVec2(data=translate_a_buffer),
        translate_b=ngl.BufferVec2(data=translate_b_buffer),
    )
    return render


@scene(freq_precision=scene.Range(range=[1, 10]), overlay=scene.Range(unit_base=100))
def audiotex(cfg: SceneCfg, freq_precision=7, overlay=0.6):
    """FFT/Waves audio texture of the audio stream blended on top of the video stream"""
    media = cfg.medias[0]
    cfg.duration = media.duration
    cfg.aspect_ratio = (media.width, media.height)

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    audio_m = ngl.Media(media.filename, audio_tex=True)
    audio_tex = ngl.Texture2D(data_src=audio_m)

    video_m = ngl.Media(media.filename)
    video_tex = ngl.Texture2D(data_src=video_m)

    p = ngl.Program(vertex=cfg.get_vert("dual-tex"), fragment=cfg.get_frag("audiotex"))
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_tex1_coord=ngl.IOVec2())
    render = ngl.Render(q, p)
    render.update_frag_resources(tex0=audio_tex, tex1=video_tex)
    render.update_frag_resources(overlay=ngl.UniformFloat(overlay))
    render.update_frag_resources(freq_precision=ngl.UniformInt(freq_precision))
    return render


@scene(particles=scene.Range(range=[1, 1023]))
def particles(cfg: SceneCfg, particles=32):
    """Particules demo using compute shaders and instancing"""

    compute_shader = cfg.get_comp("particles")
    vertex_shader = cfg.get_vert("particles")
    fragment_shader = cfg.get_frag("color")

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


@scene()
def blending_and_stencil(cfg: SceneCfg):
    """Scene using blending and stencil graphic features"""
    cfg.duration = 5
    vertex = cfg.get_vert("color")
    fragment = cfg.get_frag("color")

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


@scene(display_depth_buffer=scene.Bool())
def cube(cfg: SceneCfg, display_depth_buffer=False):
    """
    Cube with a common media Texture but a different color tainting on each side.
    Also includes a depth map visualization.
    """
    cube = ngl.Group(label="cube")

    vert_data = cfg.get_vert("texture")
    frag_data = cfg.get_frag("tex-tint")
    program = ngl.Program(vertex=vert_data, fragment=frag_data)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())

    texture = ngl.Texture2D(data_src=ngl.Media(cfg.medias[0].filename))
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

        depth_texture = ngl.Texture2D()
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


@scene()
def histogram(cfg: SceneCfg):
    """Histogram using compute shaders"""
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    g = ngl.Group()

    m = ngl.Media(cfg.medias[0].filename)
    t = ngl.Texture2D(data_src=m)

    h = ngl.Block(label="histogram_block", layout="std430")
    h.add_fields(
        ngl.BufferUInt(256, label="r"),
        ngl.BufferUInt(256, label="g"),
        ngl.BufferUInt(256, label="b"),
        ngl.UniformUInt(label="maximum"),
    )

    r = ngl.RenderTexture(t)
    proxy_size = 128
    proxy = ngl.Texture2D(width=proxy_size, height=proxy_size)
    rtt = ngl.RenderToTexture(r)
    rtt.add_color_textures(proxy)
    g.add_children(rtt)

    compute_program = ngl.ComputeProgram(cfg.get_comp("histogram-clear"), workgroup_size=(1, 1, 1))
    compute_program.update_properties(hist=ngl.ResourceProps(writable=True))
    compute = ngl.Compute(workgroup_count=(256, 1, 1), program=compute_program, label="histogram-clear")
    compute.update_resources(hist=h)
    g.add_children(compute)

    local_size = 8
    group_size = proxy_size // local_size
    compute_program = ngl.ComputeProgram(cfg.get_comp("histogram-exec"), workgroup_size=(local_size, local_size, 1))
    compute = ngl.Compute(workgroup_count=(group_size, group_size, 1), program=compute_program, label="histogram-exec")
    compute.update_resources(hist=h, source=proxy)
    compute_program.update_properties(hist=ngl.ResourceProps(writable=True))
    compute_program.update_properties(source=ngl.ResourceProps(as_image=True))
    g.add_children(compute)

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=cfg.get_vert("histogram-display"), fragment=cfg.get_frag("histogram-display"))
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    render = ngl.Render(q, p)
    render.update_frag_resources(tex0=t, hist=h)
    g.add_children(render)

    return g


@scene()
def quaternion(cfg: SceneCfg):
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

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(cfg.medias[0].filename)
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert("uniform-mat4"), fragment=cfg.get_frag("texture"))
    p.update_vert_out_vars(var_normal=ngl.IOVec3(), var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    render = ngl.Render(q, p)
    render.update_frag_resources(tex0=t)
    render.update_vert_resources(transformation_matrix=quat)

    camera = ngl.Camera(render)
    camera.set_eye(0.0, 0.0, 4.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)

    return camera


@scene(
    ndim=scene.Range(range=[1, 8]),
    nb_layers=scene.Range(range=[1, 10]),
    ref_color=scene.Color(),
    nb_mountains=scene.Range(range=[3, 15]),
)
def mountain(cfg: SceneCfg, ndim=3, nb_layers=7, ref_color=(0.5, 0.75, 0.75), nb_mountains=6):
    """Mountain generated with a stack of noise shaders using Textures as random source"""
    random_dim = 1 << ndim
    cfg.aspect_ratio = (16, 9)
    cfg.duration = nb_mountains**2

    def get_rand():
        return array.array("f", [cfg.rng.uniform(0, 1) for _ in range(random_dim)])

    black, white = (0, 0, 0), (1, 1, 1)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    prog = ngl.Program(vertex=cfg.get_vert("texture"), fragment=cfg.get_frag("mountain"))
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
        random_tex = ngl.Texture2D(data_src=random_buf, width=random_dim, height=1)

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


@scene(demo_str=scene.Text(), time_unit=scene.Range(range=[0.01, 0.3], unit_base=100))
def text(cfg: SceneCfg, demo_str="Hello World!\n\nThis is a multi-line\ntext demonstration.", time_unit=0.05):
    """Demonstrate the text node features (colors, scale, alignment, fitting, ...)"""

    group = ngl.Group()

    cfg.duration = time_unit * 2 * len(demo_str)

    nb_chars = len(demo_str)
    for i in range(nb_chars):
        ascii_text = ngl.Text(
            demo_str[: i + 1],
            aspect_ratio=cfg.aspect_ratio,
            bg_color=(0.15, 0.15, 0.15),
            bg_opacity=1,
            font_scale=1 / 2.0,
        )
        start = i * time_unit
        text_range = [ngl.TimeRangeModeNoop(0), ngl.TimeRangeModeCont(start)]
        if i != nb_chars - 1:
            end = (i + 1) * time_unit
            text_range.append(ngl.TimeRangeModeNoop(end))
        text_range_filter = ngl.TimeRangeFilter(ascii_text, ranges=text_range)
        group.add_children(text_range_filter)

    for valign in ("top", "center", "bottom"):
        for halign in ("left", "center", "right"):
            if (valign, halign) == ("center", "center"):
                continue
            fg_color = colorsys.hls_to_rgb(cfg.rng.uniform(0, 1), 0.5, 1.0)
            aligned_text = ngl.Text(
                f"{valign}-{halign}",
                valign=valign,
                halign=halign,
                aspect_ratio=cfg.aspect_ratio,
                fg_color=fg_color,
                bg_opacity=0,
                font_scale=1 / 5.0,
            )
            group.add_children(aligned_text)

    return group


@scene()
def smptebars_glitch(cfg: SceneCfg):
    """SMPTE bars glitching at irregular intervals"""

    cfg.duration = 15
    cfg.aspect_ratio = (4, 3)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=cfg.get_vert("smptebars"), fragment=cfg.get_frag("smptebars"))
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


@scene(
    mode=scene.List(("ramp", "radial")),
    c0=scene.Color(),
    c1=scene.Color(),
)
def gradient_eval(cfg: SceneCfg, mode="ramp", c0=(1, 0.5, 0.5), c1=(0.5, 1, 0.5)):
    """Animate a gradient and objects using CPU evaluation"""

    pos_res = dict(t=ngl.Time())
    pos0_x = ngl.EvalFloat("sin( 0.307*t - 0.190)", resources=pos_res)
    pos0_y = ngl.EvalFloat("sin( 0.703*t - 0.957)", resources=pos_res)
    pos1_x = ngl.EvalFloat("sin(-0.236*t + 0.218)", resources=pos_res)
    pos1_y = ngl.EvalFloat("sin(-0.851*t - 0.904)", resources=pos_res)

    trf0 = ngl.EvalVec3("x", "y", "0", resources=dict(x=pos0_x, y=pos0_y))
    trf1 = ngl.EvalVec3("x", "y", "0", resources=dict(x=pos1_x, y=pos1_y))

    geom = ngl.Circle(radius=0.02, npoints=64)
    p0 = ngl.RenderColor(color=(1 - c0[0], 1 - c0[1], 1 - c0[2]), geometry=geom)
    p1 = ngl.RenderColor(color=(1 - c1[0], 1 - c1[1], 1 - c1[2]), geometry=geom)
    p0 = ngl.Scale(p0, factors=(1 / cfg.aspect_ratio_float, 1, 1))
    p1 = ngl.Scale(p1, factors=(1 / cfg.aspect_ratio_float, 1, 1))
    p0 = ngl.Translate(p0, vector=trf0)
    p1 = ngl.Translate(p1, vector=trf1)

    pos0 = ngl.EvalVec2("x/2+.5", ".5-y/2", resources=dict(x=pos0_x, y=pos0_y))
    pos1 = ngl.EvalVec2("x/2+.5", ".5-y/2", resources=dict(x=pos1_x, y=pos1_y))
    grad = ngl.RenderGradient(pos0=pos0, pos1=pos1, mode=mode, color0=c0, color1=c1)

    return ngl.Group(children=(grad, p0, p1))
