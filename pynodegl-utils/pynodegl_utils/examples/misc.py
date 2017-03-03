import math

from pynodegl import Texture, Shader, TexturedShape, Rotate, AnimKeyFrameScalar, Triangle
from pynodegl import Quad, UniformVec4, Camera, Group

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
    gl_FragColor = vec4(c.y-c.x, 1.0-c.y, c.x, 1.0);
}'''

    b = size * math.sqrt(3) / 2.0
    c = size * 1/2.

    triangle = Triangle((0, size, 0), (b, -c, 0), (-b, -c, 0))
    s = Shader(fragment_data=frag_data)
    node = TexturedShape(triangle, s, Texture())
    node = Rotate(node, axis=(0,0,1))
    node.add_animkf(AnimKeyFrameScalar(0, 0),
                    AnimKeyFrameScalar(cfg.duration, -360*2))
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
        tshape.add_uniforms(UniformVec4("color", value=color))

        new_g = Group()
        rot = Rotate(new_g, axis=(0,0,1), anchor=orig)
        rot.add_animkf(AnimKeyFrameScalar(0, 90, "exp_in_out"),
                       AnimKeyFrameScalar(cfg.duration/2, -90, "exp_in_out"),
                       AnimKeyFrameScalar(cfg.duration, 90))
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
