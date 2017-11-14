import os
import array
import math
import random

from OpenGL import GL

from pynodegl import (
        AnimKeyFrameFloat,
        AnimKeyFrameVec3,
        AnimKeyFrameVec4,
        AnimatedFloat,
        AnimatedVec3,
        AnimatedVec4,
        BufferFloat,
        BufferUBVec4,
        BufferVec2,
        BufferVec3,
        Camera,
        Circle,
        Compute,
        ComputeProgram,
        ConfigBlend,
        GraphicConfig,
        Geometry,
        Group,
        Media,
        Program,
        Quad,
        Render,
        Rotate,
        Texture2D,
        Translate,
        Triangle,
        UniformInt,
        UniformFloat,
        UniformVec4,
)

from pynodegl_utils.misc import scene, get_shader


@scene(bgcolor1={'type': 'color'},
       bgcolor2={'type': 'color'},
       bilinear_filtering={'type': 'bool'})
def buffer_dove(cfg,
                bgcolor1=(.6,0,0,1), bgcolor2=(.8,.8,0,1),
                bilinear_filtering=True):
    cfg.duration = 3.

    # Credits: https://icons8.com/icon/40514/dove
    icon = open(os.path.join(os.path.dirname(__file__), 'data', 'icons8-dove.raw'))
    w, h = (96, 96)
    icon_data = icon.read()
    assert len(icon_data) == w * h * 4

    array_data = array.array('B', icon_data)
    img_buf = BufferUBVec4(data=array_data)
    img_tex = Texture2D(data_src=img_buf, width=w, height=h)
    if bilinear_filtering:
        img_tex.set_mag_filter(GL.GL_LINEAR)
    quad = Quad((-.5, -.5, 0.1), (1, 0, 0), (0, 1, 0))
    prog = Program()
    render = Render(quad, prog, name='dove')
    render.update_textures(tex0=img_tex)
    render = GraphicConfig(render, blend=ConfigBlend(GL.GL_TRUE,
                                                     GL.GL_SRC_ALPHA,
                                                     GL.GL_ONE_MINUS_SRC_ALPHA,
                                                     GL.GL_ONE,
                                                     GL.GL_ZERO))

    prog_bg = Program(fragment=get_shader('color'))
    shape_bg = Circle(radius=.6, npoints=256)
    render_bg = Render(shape_bg, prog_bg, name='background')
    color_animkf = [AnimKeyFrameVec4(0,                bgcolor1),
                    AnimKeyFrameVec4(cfg.duration/2.0, bgcolor2),
                    AnimKeyFrameVec4(cfg.duration,     bgcolor1)]
    ucolor = UniformVec4(anim=AnimatedVec4(color_animkf))
    render_bg.update_uniforms(color=ucolor)

    rot_animkf = [AnimKeyFrameFloat(0, 0),
                  AnimKeyFrameFloat(cfg.duration, 360)]

    return Group(children=(render_bg, render))

@scene(size={'type': 'range', 'range': [0,1.5], 'unit_base': 1000})
def triangle(cfg, size=0.5):
    b = size * math.sqrt(3) / 2.0
    c = size * 1/2.

    triangle = Triangle((-b, -c, 0), (b, -c, 0), (0, size, 0))
    p = Program(fragment=get_shader('triangle'))
    node = Render(triangle, p)
    animkf = [AnimKeyFrameFloat(0, 0),
              AnimKeyFrameFloat(cfg.duration, -360*2)]
    node = Rotate(node, anim=AnimatedFloat(animkf))
    return node

@scene(n={'type': 'range', 'range': [2,10]})
def fibo(cfg, n=8):
    cfg.duration = 5.0

    p = Program(fragment=get_shader('color'))

    fib = [0, 1, 1]
    for i in range(2, n):
        fib.append(fib[i] + fib[i-1])
    fib = fib[::-1]

    shift = 1/3. # XXX: what's the exact math here?
    shape_scale = 1. / ((2.-shift) * sum(fib))

    orig = (-shift, -shift, 0)
    g = None
    root = None
    for i, x in enumerate(fib[:-1]):
        w = x * shape_scale
        gray = 1. - i/float(n)
        color = [gray, gray, gray, 1]
        q = Quad(orig, (w, 0, 0), (0, w, 0))
        render = Render(q, p)
        render.update_uniforms(color=UniformVec4(value=color))

        new_g = Group()
        animkf = [AnimKeyFrameFloat(0,               90),
                  AnimKeyFrameFloat(cfg.duration/2, -90, "exp_in_out"),
                  AnimKeyFrameFloat(cfg.duration,    90, "exp_in_out")]
        rot = Rotate(new_g, anchor=orig, anim=AnimatedFloat(animkf))
        if g:
            g.add_children(rot)
        else:
            root = rot
        g = new_g
        new_g.add_children(render)
        orig = (orig[0] + w, orig[1] + w, 0)

    root = Camera(root)

    root.set_eye(0.0, 0.0, 2.0)
    root.set_up(0.0, 1.0, 0.0)
    root.set_perspective(45.0, cfg.aspect_ratio, 1.0, 10.0)
    return root

@scene(dim={'type': 'range', 'range': [1,50]})
def cropboard(cfg, dim=15):
    cfg.duration = 10

    kw = kh = 1. / dim
    qw = qh = 2. / dim
    tqs = []

    p = Program()
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)

    for y in range(dim):
        for x in range(dim):
            corner = (-1. + x*qw, 1. - (y+1.)*qh, 0)
            q = Quad(corner, (qw, 0, 0), (0, qh, 0))

            q.set_uv_corner(x*kw, 1. - (y+1.)*kh)
            q.set_uv_width(kw, 0)
            q.set_uv_height(0, kh)

            render = Render(q, p)
            render.update_textures(tex0=t)

            startx = random.uniform(-2, 2)
            starty = random.uniform(-2, 2)
            trn_animkf = [AnimKeyFrameVec3(0, (startx, starty, 0)),
                          AnimKeyFrameVec3(cfg.duration*2/3., (0, 0, 0), "exp_out")]
            trn = Translate(render, anim=AnimatedVec3(trn_animkf))
            tqs.append(trn)

    return Group(children=tqs)

@scene(freq_precision={'type': 'range', 'range': [1,10]},
       overlay={'type': 'range', 'unit_base': 100})
def audiotex(cfg, freq_precision=7, overlay=0.6):
    media = cfg.medias[0]
    cfg.duration = media.duration

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    audio_m = Media(media.filename, audio_tex=1)
    audio_tex = Texture2D(data_src=audio_m)

    video_m = Media(media.filename)
    video_tex = Texture2D(data_src=video_m)

    p = Program(vertex=get_shader('audiotex-vert'),
                fragment=get_shader('audiotex'))
    render = Render(q, p)
    render.update_textures(tex0=audio_tex, tex1=video_tex)
    render.update_uniforms(overlay=UniformFloat(overlay))
    render.update_uniforms(freq_precision=UniformInt(freq_precision))
    return render

@scene(particules={'type': 'range', 'range': [1,1023]})
def particules(cfg, particules=32):
    random.seed(0)

    compute_data_version = "310 es" if cfg.glbackend == "gles" else "430"
    compute_data = '#version %s\n' % compute_data_version
    compute_data += get_shader('particules')

    cfg.duration = 6

    x = 64
    p = x * particules

    positions = array.array('f')
    velocities = array.array('f')

    for i in range(p):
        positions.extend([
            random.uniform(-1.0, 1.0),
            random.uniform(0.0, 1.0),
            0.0,
            0.0,
        ])

        velocities.extend([
            random.uniform(-0.01, 0.01),
            random.uniform(-0.05, 0.05),
        ])

    ipositions = BufferVec3()
    ipositions.set_data(positions)
    ipositions.set_stride(4 * 4)
    ivelocities = BufferVec2()
    ivelocities.set_data(velocities)

    opositions = BufferVec3(p)
    opositions.set_stride(4 * 4)

    animkf = [AnimKeyFrameFloat(0, 0),
              AnimKeyFrameFloat(cfg.duration, 1)]
    utime = UniformFloat(anim=AnimatedFloat(animkf))
    uduration = UniformFloat(cfg.duration)

    cp = ComputeProgram(compute_data)

    c = Compute(x, particules, 1, cp)
    c.update_uniforms(
        time=utime,
        duration=uduration,
    )
    c.update_buffers(
        ipositions_buffer=ipositions,
        ivelocities_buffer=ivelocities,
        opositions_buffer=opositions,
    )

    gm = Geometry(opositions)
    gm.set_draw_mode(GL.GL_POINTS)

    m = Media(cfg.medias[0].filename, initial_seek=5)
    p = Program(fragment=get_shader('color'))
    r = Render(gm, p)
    r.update_uniforms(color=UniformVec4(value=(0, .6, .8, 1)))

    g = Group()
    g.add_children(c, r)

    return Camera(g)
