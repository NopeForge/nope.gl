import array
import random

from pynodegl import (
        AnimKeyFrameBuffer,
        AnimKeyFrameFloat,
        AnimKeyFrameVec3,
        AnimatedBufferVec3,
        AnimationFloat,
        AnimationVec3,
        Camera,
        GLState,
        Group,
        Identity,
        Media,
        Program,
        Quad,
        Render,
        Rotate,
        Scale,
        Texture2D,
        Translate,
        UniformMat4,
        UniformVec4,
)

from pynodegl_utils.misc import scene, get_shader

from OpenGL import GL

@scene(color={'type': 'color'},
       rotate={'type': 'bool'},
       scale={'type': 'bool'},
       translate={'type': 'bool'})
def animated_square(cfg, color=(1,0.66,0,1), rotate=True, scale=True, translate=True):
    cfg.duration = 5.0

    sz = 1/3.
    q = Quad((-sz/2, -sz/2, 0), (sz, 0, 0), (0, sz, 0))
    p = Program(fragment=get_shader('color'))
    node = Render(q, p)
    ucolor = UniformVec4(value=color)
    node.update_uniforms(color=ucolor)

    coords = [(-1, 1), (1, 1), (1, -1), (-1, -1), (-1, 1)]

    if rotate:
        animkf = (AnimKeyFrameFloat(0,            0),
                  AnimKeyFrameFloat(cfg.duration, 360))
        node = Rotate(node, anim=AnimationFloat(animkf))

    if scale:
        animkf = (AnimKeyFrameVec3(0,              (1, 1, 1)),
                  AnimKeyFrameVec3(cfg.duration/2, (2, 2, 2)),
                  AnimKeyFrameVec3(cfg.duration,   (1, 1, 1)))
        node = Scale(node, anim=AnimationVec3(animkf))

    if translate:
        animkf = []
        tscale = 1. / float(len(coords) - 1) * cfg.duration
        for i, xy in enumerate(coords):
            pos = (xy[0] * .5, xy[1] * .5, 0)
            t = i * tscale
            animkf.append(AnimKeyFrameVec3(t, pos))
        node = Translate(node, anim=AnimationVec3(animkf))

    return node

@scene(rotate={'type': 'bool'},
       scale={'type': 'bool'},
       translate={'type': 'bool'})
def animated(cfg, rotate=True, scale=True, translate=True):
    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    node = Render(q, p)
    node.update_textures(tex0=t)

    if rotate:
        animkf = [AnimKeyFrameFloat(0, 0),
                  AnimKeyFrameFloat(cfg.duration, 360, "exp_in")]
        node = Rotate(node, anim=AnimationFloat(animkf))

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

@scene()
def animated_uniform(cfg):
    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program(fragment=get_shader('matrix-transform'))
    ts = Render(q, p)
    ts.update_textures(tex0=t)

    scale_animkf = [AnimKeyFrameVec3(0, (1,1,1)),
                    AnimKeyFrameVec3(cfg.duration, (0.1,0.1,0.1), "quartic_out")]
    s = Scale(Identity(), anim=AnimationVec3(scale_animkf))

    rotate_animkf = [AnimKeyFrameFloat(0, 0),
                     AnimKeyFrameFloat(cfg.duration, 360, "exp_out")]
    r = Rotate(s, axis=(0, 0, 1), anim=AnimationFloat(rotate_animkf))

    u = UniformMat4(transform=r)
    ts.update_uniforms(matrix=u)

    return ts

@scene(rotate={'type': 'bool'})
def animated_camera(cfg, rotate=False):
    g = Group()
    g.add_glstates(GLState(GL.GL_DEPTH_TEST, GL.GL_TRUE))

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    node = Render(q, p)
    node.update_textures(tex0=t)
    g.add_children(node)

    z = -1
    q = Quad((-1.1, 0.3, z), (1, 0, 0), (0, 1, 0))
    node = Render(q, p)
    node.update_textures(tex0=t)
    g.add_children(node)

    q = Quad((0.1, 0.3, z), (1, 0, 0), (0, 1, 0))
    node = Render(q, p)
    node.update_textures(tex0=t)
    g.add_children(node)

    q = Quad((-1.1, -1.0, z), (1, 0, 0), (0, 1, 0))
    node = Render(q, p)
    node.update_textures(tex0=t)
    g.add_children(node)

    q = Quad((0.1, -1.0, z), (1, 0, 0), (0, 1, 0))
    node = Render(q, p)
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
        rot_animkf = [AnimKeyFrameFloat(0, 0),
                      AnimKeyFrameFloat(cfg.duration, 360, "exp_out")]
        node = Rotate(node, axis=(0, 1, 0), anim=AnimationFloat(rot_animkf))

    camera.set_eye_transform(node)

    fov_animkf = [AnimKeyFrameFloat(0.5, 60.0),
                  AnimKeyFrameFloat(cfg.duration, 45.0, "exp_out")]
    camera.set_fov_anim(AnimationFloat(fov_animkf))

    return camera

@scene(dim={'type': 'range', 'range': [1, 100]})
def animated_buffer(cfg, dim=50):
    cfg.duration = 5.

    random.seed(0)
    get_rand = lambda: array.array('f', [random.random() for i in range(dim ** 2 * 3)])
    nb_kf = int(cfg.duration)
    buffers = [get_rand() for i in range(nb_kf)]
    random_animkf = []
    time_scale = cfg.duration / float(nb_kf)
    for i, buf in enumerate(buffers + [buffers[0]]):
        random_animkf.append(AnimKeyFrameBuffer(i*time_scale, buf))
    random_buffer = AnimatedBufferVec3(animkf=random_animkf)
    random_tex = Texture2D(data_src=random_buffer, width=dim, height=dim)

    quad = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    prog = Program()
    render = Render(quad, prog)
    render.update_textures(tex0=random_tex)
    return render
