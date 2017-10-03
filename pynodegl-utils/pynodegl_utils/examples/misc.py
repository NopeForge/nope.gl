import array
import math
import random

from OpenGL import GL

from pynodegl import (
        AnimKeyFrameVec3,
        AnimKeyFrameVec4,
        AnimatedBufferVec3,
        AnimKeyFrameBuffer,
        AnimationVec3,
        AnimationVec4,
        Camera,
        Circle,
        Geometry,
        Group,
        Media,
        Program,
        Quad,
        Render,
        Rotate,
        Texture,
        Translate,
        Triangle,
        UniformScalar,
        UniformVec4,
)

from pynodegl_utils.misc import scene


@scene({'name': 'square_color', 'type': 'color'},
       {'name': 'circle_color', 'type': 'color'})
def square2circle(cfg, square_color=(0.9, 0.1, 0.3, 1.0), circle_color=(1.0, 1.0, 1.0, 1.0)):
    cfg.duration = 5

    frag = '''#version 100
precision mediump float;
uniform vec4 color;
void main(void)
{
    gl_FragColor = color;
}'''

    def sqxf(t): # square x coordinates clockwise starting top-left
        if t < 1/4.: return t*4
        if t < 1/2.: return 1
        if t < 3/4.: return 1.-(t-.5)*4
        return 0

    def sqyf(t): # square y coordinates clockwise starting top-left
        if t < 1/4.: return 1
        if t < 1/2.: return 1.-(t-.25)*4
        if t < 3/4.: return 0
        return (t-.75)*4

    n = 1024 # number of vertices
    s = 1.25 # shapes scale
    interp = 'exp_in_out'

    square_vertices = array.array('f')
    for i in range(n):
        x = (sqxf(i / float(n)) - .5) * s
        y = (sqyf(i / float(n)) - .5) * s
        square_vertices.extend([x, y, 0])

    circle_vertices = array.array('f')
    step = 2 * math.pi / float(n)
    for i in range(n):
        angle = i * step - math.pi/4.
        x = math.sin(angle) * .5 * s
        y = math.cos(angle) * .5 * s
        circle_vertices.extend([x, y, 0])

    vertices_animkf = [
            AnimKeyFrameBuffer(0,               square_vertices),
            AnimKeyFrameBuffer(cfg.duration/2., circle_vertices, interp),
            AnimKeyFrameBuffer(cfg.duration,    square_vertices, interp),
    ]
    vertices = AnimatedBufferVec3(vertices_animkf)

    color_animkf = [
            AnimKeyFrameVec4(0,               square_color),
            AnimKeyFrameVec4(cfg.duration/2., circle_color, interp),
            AnimKeyFrameVec4(cfg.duration,    square_color, interp),
    ]
    ucolor = UniformVec4(anim=AnimationVec4(color_animkf))

    geom = Geometry(vertices)
    geom.set_draw_mode(GL.GL_TRIANGLE_FAN)
    p = Program(fragment=frag)
    render = Render(geom, p)
    render.update_uniforms(color=ucolor)
    return render

def _get_vertices(n, radius_func, offset=0):
    vertices = []
    step = 2 * math.pi / n
    for i in range(n):
        angle = (i + offset) * step
        radius = radius_func()
        x, y = math.sin(angle) * radius, math.cos(angle) * radius
        vertices.append([x, y, 0])
    return vertices


@scene({'name': 'npoints', 'type': 'range', 'range': [3, 100]})
def morphing(cfg, npoints=25):
    cfg.duration = 1

    frag = '''#version 100
precision mediump float;
void main(void)
{
    gl_FragColor = vec4(.9, .1, .3, 1.0);
}'''

    random.seed(0)

    n, m = .1, .9
    mid = n + (m-n)/2.

    inner_rfunc = lambda: n
    inner_vertices = _get_vertices(npoints, inner_rfunc)
    outer_rfunc_0 = lambda: random.uniform(n, mid)
    outer_rfunc_1 = lambda: random.uniform(mid, m)
    outer_vertices_0 = _get_vertices(npoints, outer_rfunc_0, offset=.5)
    outer_vertices_1 = _get_vertices(npoints, outer_rfunc_1, offset=.5)

    animkf = []
    for i, outer_vertices in enumerate([outer_vertices_0, outer_vertices_1, outer_vertices_0]):
        vertices_data = array.array('f')
        for inner_vertex, outer_vertex in zip(inner_vertices, outer_vertices):
            vertices_data.extend(inner_vertex + outer_vertex)
        vertices_data.extend(inner_vertices[0])
        animkf.append(AnimKeyFrameBuffer(i*cfg.duration/2., vertices_data))
    vertices = AnimatedBufferVec3(animkf)

    geom = Geometry(vertices)
    geom.set_draw_mode(GL.GL_LINE_STRIP)
    p = Program(fragment=frag)
    render = Render(geom, p)
    return render

@scene({'name': 'size', 'type': 'range', 'range': [0,1.5], 'unit_base': 1000})
def triangle(cfg, size=0.5):
    frag_data = '''
#version 100
precision mediump float;
varying vec2 var_tex0_coords;
void main(void)
{
    vec2 c = var_tex0_coords;
    gl_FragColor = vec4(c.y-c.x, c.x, 1.0-c.y, 1.0);
}'''

    b = size * math.sqrt(3) / 2.0
    c = size * 1/2.

    triangle = Triangle((-b, -c, 0), (b, -c, 0), (0, size, 0))
    p = Program(fragment=frag_data)
    node = Render(triangle, p)
    node.update_textures(tex0=Texture())
    animkf = [AnimKeyFrameVec3(0, (0, 0, 0)),
              AnimKeyFrameVec3(cfg.duration, (0, 0, -360*2))]
    node = Rotate(node, anim=AnimationVec3(animkf))
    return node

@scene({'name': 'npoints', 'type': 'range', 'range': [3, 300]},
       {'name': 'radius', 'type': 'range', 'range': [0.01, 5], 'unit_base': 100})
def circle(cfg, npoints=64, radius=0.5):
    circle = Circle(npoints=npoints, radius=radius)
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    p = Program()
    render = Render(circle, p)
    render.update_textures(tex0=t)
    return render

@scene({'name': 'n', 'type': 'range', 'range': [2,10]})
def fibo(cfg, n=8):
    frag_data = '''
#version 100
precision mediump float;
uniform vec4 color;
void main(void) {
    gl_FragColor = color;
}'''

    cfg.duration = 5

    p = Program(fragment=frag_data)

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
        animkf = [AnimKeyFrameVec3(0,              (0,0, 90)),
                  AnimKeyFrameVec3(cfg.duration/2, (0,0,-90), "exp_in_out"),
                  AnimKeyFrameVec3(cfg.duration,   (0,0, 90), "exp_in_out")]
        rot = Rotate(new_g, anchor=orig, anim=AnimationVec3(animkf))
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

@scene({'name': 'dim', 'type': 'range', 'range': [1,50]})
def cropboard(cfg, dim=15):
    cfg.duration = 10

    kw = kh = 1. / dim
    qw = qh = 2. / dim
    tqs = []

    p = Program()
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)

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
            trn = Translate(render, anim=AnimationVec3(trn_animkf))
            tqs.append(trn)

    return Group(children=tqs)

@scene({'name': 'freq_precision', 'type': 'range', 'range': [1,10]},
       {'name': 'overlay', 'type': 'range', 'unit_base': 100})
def audiotex(cfg, freq_precision=7, overlay=0.6):
    media = cfg.medias[0]
    cfg.duration = media.duration

    freq_line = 2                           # skip the 2 audio channels
    freq_line += (10 - freq_precision) * 2  # 2x10 lines of FFT

    fft1, fft2 = freq_line + 0.5, freq_line + 1 + 0.5

    frag_data = '''
#version 100
precision mediump float;

uniform sampler2D tex0_sampler;
uniform sampler2D tex1_sampler;
varying vec2 var_tex0_coords;

void main()
{
    vec4 audio_pix;
    vec4 video_pix = texture2D(tex1_sampler, var_tex0_coords);
    vec2 sample_id_ch_1 = vec2(var_tex0_coords.x,      0.5 / 22.);
    vec2 sample_id_ch_2 = vec2(var_tex0_coords.x,      1.5 / 22.);
    vec2  power_id_ch_1 = vec2(var_tex0_coords.x, %(fft1)f / 22.);
    vec2  power_id_ch_2 = vec2(var_tex0_coords.x, %(fft2)f / 22.);
    float sample_ch_1 = texture2D(tex0_sampler, sample_id_ch_1).x;
    float sample_ch_2 = texture2D(tex0_sampler, sample_id_ch_2).x;
    float  power_ch_1 = texture2D(tex0_sampler,  power_id_ch_1).x;
    float  power_ch_2 = texture2D(tex0_sampler,  power_id_ch_2).x;
    power_ch_1 = sqrt(power_ch_1);
    power_ch_2 = sqrt(power_ch_2);
    sample_ch_1 = (sample_ch_1 + 1.) / 8.; // [-1;1] -> [0    ; 0.25]
    sample_ch_2 = (sample_ch_2 + 3.) / 8.; // [-1;1] -> [0.25 ; 0.5 ]
    power_ch_1 = clamp(power_ch_1, 0., 1.) / 4.; // [0 ; +oo] -> [0 ; 0.25]
    power_ch_2 = clamp(power_ch_2, 0., 1.) / 4.; // [0 ; +oo] -> [0 ; 0.25]

    float diff_wave_ch_1 = abs(sample_ch_1 - var_tex0_coords.y);
    float diff_wave_ch_2 = abs(sample_ch_2 - var_tex0_coords.y);
    if (diff_wave_ch_1 < 0.003) {
        audio_pix = vec4(0.5, 1.0, 0.0, 1.0);
    } else if (diff_wave_ch_2 < 0.003) {
        audio_pix = vec4(0.0, 1.0, 0.5, 1.0);
    } else if (var_tex0_coords.y > 0.75 - power_ch_1 && var_tex0_coords.y < 0.75) {
        audio_pix = vec4(1.0, 0.5, 0.0, 1.0);
    } else if (var_tex0_coords.y > 1.   - power_ch_2 && var_tex0_coords.y < 1.) {
        audio_pix = vec4(1.0, 0.0, 0.5, 1.0);
    } else {
        audio_pix = vec4(0.0, 0.0, 0.0, 1.0);
    }
    gl_FragColor = mix(video_pix, audio_pix, %(overlay)f);
}
''' % {'fft1': fft1, 'fft2': fft2, 'overlay': overlay}

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    audio_m = Media(media.filename, audio_tex=1)
    audio_tex = Texture(data_src=audio_m)

    video_m = Media(media.filename)
    video_tex = Texture(data_src=video_m)

    p = Program(fragment=frag_data)
    render = Render(q, p)
    render.update_textures(tex0=audio_tex, tex1=video_tex)
    return render
