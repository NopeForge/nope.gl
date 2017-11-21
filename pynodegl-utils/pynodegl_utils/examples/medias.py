import array
import math

from pynodegl import (
        AnimKeyFrameFloat,
        AnimKeyFrameVec3,
        AnimatedFloat,
        AnimatedVec3,
        ConfigColorMask,
        ConfigStencil,
        Circle,
        GraphicConfig,
        Group,
        Media,
        Program,
        Quad,
        Render,
        Rotate,
        Scale,
        Texture2D,
        UniformFloat,
        UniformVec4,
)

from pynodegl_utils.misc import scene, get_frag

from OpenGL import GL

@scene(uv_corner_x={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_corner_y={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_width={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_height={'type': 'range', 'range': [0,1], 'unit_base': 100},
       progress_bar={'type': 'bool'})
def centered_media(cfg, uv_corner_x=0, uv_corner_y=0, uv_width=1, uv_height=1, progress_bar=True):

    cfg.duration = cfg.medias[0].duration

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0),
             (uv_corner_x, uv_corner_y), (uv_width, 0), (0, uv_height))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)

    if progress_bar:
        p.set_fragment(get_frag('progress-bar'))
        time_animkf = [AnimKeyFrameFloat(0, 0),
                       AnimKeyFrameFloat(cfg.duration, 1)]
        time = UniformFloat(anim=AnimatedFloat(time_animkf))
        ar = UniformFloat(cfg.aspect_ratio[0] / float(cfg.aspect_ratio[1]))
        render.update_uniforms(time=time, ar=ar)
    return render

@scene()
def centered_masked_media(cfg):
    cfg.duration = 2

    g = Group()

    q = Quad((-0.2, -0.2, 0), (0.4, 0.0, 0.0), (0.0, 0.4, 0.0))
    p = Program(fragment=get_frag('color'))
    node = Render(q, p)
    node.update_uniforms(color=UniformVec4(value=(0,0,0,1)))

    scale_animkf = [AnimKeyFrameVec3(0, (0.1,  0.1, 1.0)),
                    AnimKeyFrameVec3(10, (10., 10.0,  3), "exp_out")]
    node = Scale(node, anim=AnimatedVec3(scale_animkf))

    rotate_animkf = [AnimKeyFrameFloat(0, 0),
                     AnimKeyFrameFloat(cfg.duration, 360, "exp_out")]
    node = Rotate(node, anim=AnimatedFloat(rotate_animkf))

    node = GraphicConfig(node,
                         stencil=ConfigStencil(GL.GL_TRUE,
                                               0xFF,
                                               GL.GL_ALWAYS,
                                               1,
                                               0xFF,
                                               GL.GL_KEEP,
                                               GL.GL_KEEP,
                                               GL.GL_REPLACE),
                         colormask=ConfigColorMask(GL.GL_TRUE, 0, 0, 0, 0))

    g.add_children(node)

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    node = Render(q, p)
    node.update_textures(tex0=t)
    node = GraphicConfig(node,
                         stencil=ConfigStencil(GL.GL_TRUE,
                                               0x00,
                                               GL.GL_EQUAL,
                                               1,
                                               0xFF,
                                               GL.GL_KEEP,
                                               GL.GL_KEEP,
                                               GL.GL_KEEP))

    g.add_children(node)
    return g


@scene(npoints={'type': 'range', 'range': [3, 300]},
       radius={'type': 'range', 'range': [0.01, 5], 'unit_base': 100})
def media_in_circle(cfg, npoints=64, radius=0.5):
    circle = Circle(npoints=npoints, radius=radius)
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    render = Render(circle, p)
    render.update_textures(tex0=t)
    return render


@scene(speed={'type': 'range', 'range': [0.01,2], 'unit_base': 1000})
def playback_speed(cfg, speed=1.0):
    cfg.duration = cfg.medias[0].duration / speed

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    time_animkf = [AnimKeyFrameFloat(0, 0),
                   AnimKeyFrameFloat(cfg.duration, cfg.duration * speed)]
    m = Media(cfg.medias[0].filename, initial_seek=5, time_anim=AnimatedFloat(time_animkf))
    t = Texture2D(data_src=m)
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)
    return render
