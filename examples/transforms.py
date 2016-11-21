from pynodegl import Quad, Texture, Shader, TexturedShape, Media, Camera
from pynodegl import Scale, Rotate, Translate
from pynodegl import UniformSampler, UniformVec4, AttributeVec2
from pynodegl import AnimKeyFrameScalar, AnimKeyFrameVec3

from misc import scene

frag_data = """
uniform vec4 blend_color;
void main(void)
{
    gl_FragColor = blend_color;
}"""

@scene({'name': 'color', 'type': 'color'},
       {'name': 'width',  'type': 'range', 'range': [0.01,1], 'unit_base': 100},
       {'name': 'height', 'type': 'range', 'range': [0.01,1], 'unit_base': 100},
       {'name': 'translate', 'type': 'bool'},
       {'name': 'translate_x', 'type': 'range', 'range': [-1,1], 'unit_base': 100},
       {'name': 'translate_y', 'type': 'range', 'range': [-1,1], 'unit_base': 100},
       {'name': 'scale', 'type': 'bool'},
       {'name': 'scale_x', 'type': 'range', 'range': [0.01,2], 'unit_base': 100},
       {'name': 'scale_y', 'type': 'range', 'range': [0.01,2], 'unit_base': 100},
       {'name': 'rotate', 'type': 'bool'},
       {'name': 'rotate_deg', 'type': 'range', 'range': [0,360], 'unit_base': 10})
def static(args, duration, color=(0.5, 0.0, 1.0, 1.0), width=0.5, height=0.5,
           translate=False, translate_x=0, translate_y=0,
           scale=False, scale_x=1, scale_y=1,
           rotate=False, rotate_deg=0):
    q = Quad((-width/2, -height/2., 0), (width, 0, 0), (0, height, 0))
    s = Shader()

    ucolor = UniformVec4("blend_color", value=color)

    s.set_fragment_data(frag_data)
    node = TexturedShape(q, s)
    node.add_uniforms(ucolor)

    if rotate:
        node = Rotate(node, axis=(0,0,1), angle=rotate_deg)

    if scale:
        node = Scale(node, factors=(scale_x, scale_y, 0))

    if translate:
        node = Translate(node, vector=(translate_x, translate_y, 0))

    return node

@scene({'name': 'rotate', 'type': 'bool'},
       {'name': 'scale', 'type': 'bool'},
       {'name': 'translate', 'type': 'bool'})
def animated(args, duration, rotate=True, scale=True, translate=True):
    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(args[0])
    t = Texture(data_src=m)
    s = Shader()
    node = TexturedShape(q, s, t)

    if rotate:
        node = Rotate(node, axis=(0,0,1))
        node.add_animkf(AnimKeyFrameScalar(0,  0,   "exp_in"),
                        AnimKeyFrameScalar(duration, 360))

    if scale:
        node = Scale(node)
        node.add_animkf(AnimKeyFrameVec3(0, (16/9., 0.5, 1.0), "exp_out"),
                        AnimKeyFrameVec3(duration, (4/3.,  1.0, 0.5)))

    if translate:
        node = Translate(node)
        node.add_animkf(AnimKeyFrameVec3(0,          (-0.5,  0.5, -0.7), "circular_in"),
                        AnimKeyFrameVec3(duration/2, ( 0.5, -0.5,  0.7), "sinus_in_out:0:.7"),
                        AnimKeyFrameVec3(duration,   (-0.5, -0.3, -0.5)))

    return node
