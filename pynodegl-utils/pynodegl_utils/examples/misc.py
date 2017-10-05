import array
import math
import random

from OpenGL import GL

from pynodegl import (
        AnimKeyFrameScalar,
        AnimKeyFrameVec3,
        AnimationScalar,
        AnimationVec3,
        BufferScalar,
        BufferVec2,
        BufferVec3,
        Camera,
        Compute,
        ComputeProgram,
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


@scene(size={'type': 'range', 'range': [0,1.5], 'unit_base': 1000})
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

@scene(n={'type': 'range', 'range': [2,10]})
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

@scene(dim={'type': 'range', 'range': [1,50]})
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

@scene(freq_precision={'type': 'range', 'range': [1,10]},
       overlay={'type': 'range', 'unit_base': 100})
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

@scene(particules={'type': 'range', 'range': [1,1024]})
def particules(cfg, particules=32):
    compute_data_version = "310 es" if cfg.glbackend == "gles" else "430"
    compute_data = '''
#version %s

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 0) buffer ipositions_buffer {
    vec3 ipositions[];
};

layout (std430, binding = 1) buffer ivelocities_buffer {
    vec2 ivelocities[];
};

layout (std430, binding = 2) buffer opositions_buffer {
    vec3 opositions[];
};

uniform float time;
uniform float duration;

float bounceOut(float t)
{
    float c = 1.0;
    float a = 1.70158;

    if (t >= 1.0) {
        return c;
    } else if (t < 4.0 / 11.0) {
        return c * (7.5625 * t * t);
    } else if (t < 8.0 / 11.0) {
        t -= 6.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.75)) + c;
    } else if (t < 10.0 / 11.0) {
        t -= 9.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.9375)) + c;
    } else {
        t -= 21.0 / 22.0;
        return -a * (1.0 - (7.5625 * t * t + 0.984375)) + c;
    }
}

float bounce(float t)
{
    return 1.0 - bounceOut(t);
}

void main(void)
{
    uint i = gl_GlobalInvocationID.x +
             gl_GlobalInvocationID.y * 1024U;

    vec3 iposition = ipositions[i];
    vec2 ivelocity = ivelocities[i];
    float step = time * duration * 30.0;
    vec2 velocity = ivelocity;
    vec3 position = iposition;
    float yoffset = 1.0 - iposition.y;
    float speed = 1.0 + ivelocity.y;

    position.x = iposition.x + step * velocity.x;
    position.y = ((2.0 - yoffset) * bounce(time * speed * (1.0  + yoffset))) - 0.99;

    opositions[i] = position;
}
''' % (compute_data_version)

    fragment_data = '''
#version 100

precision highp float;

void main(void)
{
    gl_FragColor = vec4(0.0, 0.6, 0.8, 1.0);
}'''

    cfg.duration = 6

    x = 1024
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

    animkf = [AnimKeyFrameScalar(0, 0),
              AnimKeyFrameScalar(cfg.duration, 1)]
    utime = UniformScalar(anim=AnimationScalar(animkf))
    uduration = UniformScalar(cfg.duration)

    cp = ComputeProgram(compute_data)

    c = Compute(1024, particules, 1, cp)
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
    p = Program(fragment=fragment_data)
    r = Render(gm, p)

    g = Group()
    g.add_children(c, r)

    return Camera(g)
