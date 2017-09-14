from pynodegl import Quad, Texture, Shader, TexturedShape, Media, Camera, Group, GLState
from pynodegl import Scale, Rotate, Translate, Identity
from pynodegl import UniformVec4, UniformMat4
from pynodegl import AnimationScalar, AnimKeyFrameScalar
from pynodegl import AnimationVec3, AnimKeyFrameVec3

from pynodegl_utils.misc import scene

from OpenGL import GL

frag_data = """
#version 100
precision mediump float;
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
       {'name': 'rotate_deg', 'type': 'range', 'range': [0,360], 'unit_base': 10},
       {'name': 'rotate_anchor_x', 'type': 'range', 'range': [-1, 1], 'unit_base': 100},
       {'name': 'rotate_anchor_y', 'type': 'range', 'range': [-1, 1], 'unit_base': 100},
       {'name': 'rotate_anchor_z', 'type': 'range', 'range': [-1, 1], 'unit_base': 100})
def static(cfg, color=(0.5, 0.0, 1.0, 1.0), width=0.5, height=0.5,
           translate=False, translate_x=0, translate_y=0,
           scale=False, scale_x=1, scale_y=1,
           rotate=False, rotate_deg=0, rotate_anchor_x=0, rotate_anchor_y=0, rotate_anchor_z=0):
    q = Quad((-width/2., -height/2., 0), (width, 0, 0), (0, height, 0))
    s = Shader()

    ucolor = UniformVec4(value=color)

    s.set_fragment_data(frag_data)
    node = TexturedShape(q, s)
    node.update_uniforms(blend_color=ucolor)

    if rotate:
        node = Rotate(node, angles=(0, 0, rotate_deg),
                      anchor=(rotate_anchor_x, rotate_anchor_y, rotate_anchor_z))

    if scale:
        node = Scale(node, factors=(scale_x, scale_y, 0))

    if translate:
        node = Translate(node, vector=(translate_x, translate_y, 0))

    return node

@scene({'name': 'rotate', 'type': 'bool'},
       {'name': 'scale', 'type': 'bool'},
       {'name': 'translate', 'type': 'bool'})
def animated(cfg, rotate=True, scale=True, translate=True):
    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    s = Shader()
    node = TexturedShape(q, s)
    node.update_textures(tex0=t)

    if rotate:
        animkf = [AnimKeyFrameVec3(0, (0, 0, 0)),
                  AnimKeyFrameVec3(cfg.duration, (0, 0, 360), "exp_in")]
        node = Rotate(node, anim=AnimationVec3(animkf))

    if scale:
        animkf = [AnimKeyFrameVec3(0, (16/9., 0.5, 1.0)),
                  AnimKeyFrameVec3(cfg.duration, (4/3.,  1.0, 0.5), "exp_out")]
        node = Scale(node, anim=AnimationVec3(animkf))

    if translate:
        animkf = [AnimKeyFrameVec3(0,              (-0.5,  0.5, -0.7)),
                  AnimKeyFrameVec3(cfg.duration/2, ( 0.5, -0.5,  0.7), "circular_in"),
                  AnimKeyFrameVec3(cfg.duration,   (-0.5, -0.3, -0.5), "sinus_in_out", easing_args=(0, .7))]
        node = Translate(node, anim=AnimationVec3(animkf))

    return node

animated_frag_data = """
#version 100
precision mediump float;
uniform mat4 matrix;
uniform sampler2D tex0_sampler;
varying vec2 var_tex0_coords;

void main(void)
{
    vec2 coords = var_tex0_coords * 2.0 - 1.0;
    coords = (matrix * vec4(coords.xy, 1.0, 1.0)).xy;
    coords = (coords + 1.0) / 2.0;

    gl_FragColor = texture2D(tex0_sampler, coords);
}
"""

@scene()
def animated_uniform(cfg):
    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    s = Shader(fragment_data=animated_frag_data)
    ts = TexturedShape(q, s)
    ts.update_textures(tex0=t)

    scale_animkf = [AnimKeyFrameVec3(0, (1,1,1)),
                    AnimKeyFrameVec3(cfg.duration, (0.1,0.1,0.1), "quartic_out")]
    s = Scale(Identity(), anim=AnimationVec3(scale_animkf))

    rotate_animkf = [AnimKeyFrameVec3(0, (0, 0, 0)),
                     AnimKeyFrameVec3(cfg.duration, (0, 0, 360), "exp_out")]
    r = Rotate(s, anim=AnimationVec3(rotate_animkf))

    u = UniformMat4(transform=r)
    ts.update_uniforms(matrix=u)

    return ts

@scene({'name': 'rotate', 'type': 'bool'})
def animated_camera(cfg, rotate=False):
    g = Group()
    g.add_glstates(GLState(GL.GL_DEPTH_TEST, GL.GL_TRUE))

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    s = Shader()
    node = TexturedShape(q, s)
    node.update_textures(tex0=t)
    g.add_children(node)

    z = -1
    q = Quad((-1.1, 0.3, z), (1, 0, 0), (0, 1, 0))
    node = TexturedShape(q, s)
    node.update_textures(tex0=t)
    g.add_children(node)

    q = Quad((0.1, 0.3, z), (1, 0, 0), (0, 1, 0))
    node = TexturedShape(q, s)
    node.update_textures(tex0=t)
    g.add_children(node)

    q = Quad((-1.1, -1.0, z), (1, 0, 0), (0, 1, 0))
    node = TexturedShape(q, s)
    node.update_textures(tex0=t)
    g.add_children(node)

    q = Quad((0.1, -1.0, z), (1, 0, 0), (0, 1, 0))
    node = TexturedShape(q, s)
    node.update_textures(tex0=t)
    g.add_children(node)

    camera = Camera(g)
    camera.set_eye(0, 0, 2)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio, 0.1, 10.0)

    tr_animkf = [AnimKeyFrameVec3(0,  (0.0, 0.0, 0.0)),
                 AnimKeyFrameVec3(10, (0.0, 0.0, 3.0), "exp_out")]
    node = Translate(Identity(), anim=AnimationVec3(tr_animkf))

    if rotate:
        rot_animkf = [AnimKeyFrameVec3(0, (0, 0, 0)),
                      AnimKeyFrameVec3(cfg.duration, (0, 360, 0), "exp_out")]
        node = Rotate(node, anim=AnimationVec3(rot_animkf))

    camera.set_eye_transform(node)

    fov_animkf = [AnimKeyFrameScalar(0.5, 60.0),
                  AnimKeyFrameScalar(cfg.duration, 45.0, "exp_out")]
    camera.set_fov_anim(AnimationScalar(fov_animkf))

    return camera
