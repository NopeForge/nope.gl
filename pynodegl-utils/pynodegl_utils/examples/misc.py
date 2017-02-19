import math

from pynodegl import Texture, Shader, TexturedShape, Rotate, AnimKeyFrameScalar, Triangle

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
