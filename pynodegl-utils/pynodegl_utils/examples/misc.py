import math
import random

from pynodegl import Texture, Shader, TexturedShape, Rotate, Triangle
from pynodegl import Quad, UniformVec4, Camera, Group
from pynodegl import Media, Translate
from pynodegl import AnimationScalar, AnimKeyFrameScalar
from pynodegl import AnimationVec3, AnimKeyFrameVec3

from pynodegl_utils.misc import scene

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
    s = Shader(fragment_data=frag_data)
    node = TexturedShape(triangle, s)
    node.update_textures(tex0=Texture())
    animkf = [AnimKeyFrameScalar(0, 0),
              AnimKeyFrameScalar(cfg.duration, -360*2)]
    node = Rotate(node, axis=(0,0,1), anim=AnimationScalar(animkf))
    return node

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

    s = Shader(fragment_data=frag_data)

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
        tshape = TexturedShape(q, s)
        tshape.update_uniforms(color=UniformVec4(value=color))

        new_g = Group()
        animkf = [AnimKeyFrameScalar(0, 90),
                  AnimKeyFrameScalar(cfg.duration/2, -90, "exp_in_out"),
                  AnimKeyFrameScalar(cfg.duration, 90, "exp_in_out")]
        rot = Rotate(new_g, axis=(0,0,1), anchor=orig, anim=AnimationScalar(animkf))
        if g:
            g.add_children(rot)
        else:
            root = rot
        g = new_g
        new_g.add_children(tshape)
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

    s = Shader()
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)

    for y in range(dim):
        for x in range(dim):
            corner = (-1. + x*qw, 1. - (y+1.)*qh, 0)
            q = Quad(corner, (qw, 0, 0), (0, qh, 0))

            q.set_uv_corner(x*kw, 1. - (y+1.)*kh)
            q.set_uv_width(kw, 0)
            q.set_uv_height(0, kh)

            tshape = TexturedShape(q, s)
            tshape.update_textures(tex0=t)

            startx = random.uniform(-2, 2)
            starty = random.uniform(-2, 2)
            trn_animkf = [AnimKeyFrameVec3(0, (startx, starty, 0)),
                          AnimKeyFrameVec3(cfg.duration*2/3., (0, 0, 0), "exp_out")]
            trn = Translate(tshape, anim=AnimationVec3(trn_animkf))
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

    s = Shader(fragment_data=frag_data)
    tshape = TexturedShape(q, s)
    tshape.update_textures(tex0=audio_tex, tex1=video_tex)
    return tshape
