from pynodegl import TexturedShape, Quad, Triangle, Shape, ShapePrimitive, Texture, Media, Shader, AnimKeyFrameScalar

from pynodegl_utils.misc import scene

from OpenGL import GL

@scene()
def centered_media(cfg):
    cfg.duration = cfg.medias[0].duration

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape

@scene()
def centered_triangle_media(cfg):
    cfg.duration = cfg.medias[0].duration

    q = Triangle((-0.5, -0.5, 0), (-0.5, 0.5, 0), (0.5, -0.5, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape

@scene({'name': 'n', 'type': 'range', 'range': [0,1], 'unit_base': 1000})
def centered_shape_media(cfg, n=0.5):
    cfg.duration = cfg.medias[0].duration

    q = Shape([
        ShapePrimitive((  -n, -n/2, 0), (0,    0.75)),
        ShapePrimitive((  -n,  n/2, 0), (0,    0.25)),
        ShapePrimitive((-n/2,    n, 0), (0.25, 0)),
        ShapePrimitive(( n/2,    n, 0), (0.75, 0)),
        ShapePrimitive((   n,  n/2, 0), (1,    0.25)),
        ShapePrimitive((   n, -n/2, 0), (1,    0.75)),
        ShapePrimitive(( n/2,   -n, 0), (0.75, 1)),
        ShapePrimitive((-n/2,   -n, 0), (0.25, 1)),
    ])
    q.set_draw_mode(GL.GL_TRIANGLE_FAN)

    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape

@scene({'name': 'speed', 'type': 'range', 'range': [0.01,2], 'unit_base': 1000})
def playback_speed(cfg, speed=1.0):
    cfg.duration = cfg.medias[0].duration / speed

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename, initial_seek=5)
    m.add_time_animkf(AnimKeyFrameScalar(0, 0),
                      AnimKeyFrameScalar(cfg.duration, cfg.duration * speed))
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape
