from pynodegl import TexturedShape, Quad, Triangle, Shape, ShapePrimitive, Texture, Media, Shader

from pynodegl_utils.misc import scene

from OpenGL import GL

@scene()
def centered_media(args, duration):
    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(args[0])
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape

@scene()
def centered_triangle_media(args, duration):
    q = Triangle((-0.5, -0.5, 0), (-0.5, 0.5, 0), (0.5, -0.5, 0))
    m = Media(args[0])
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape

@scene({'name': 'n', 'type': 'range', 'range': [0,1], 'unit_base': 1000})
def centered_shape_media(args, duration, n=0.5):

    q = Shape([
        ShapePrimitive((  -n, -n/2, 0), (1, 0)),
        ShapePrimitive((  -n,  n/2, 0), (0, 0)),
        ShapePrimitive((-n/2,    n, 0), (1, 1)),
        ShapePrimitive(( n/2,    n, 0), (1, 0)),
        ShapePrimitive((   n,  n/2, 0), (0, 0)),
        ShapePrimitive((   n, -n/2, 0), (1, 1)),
        ShapePrimitive(( n/2,   -n, 0), (1, 1)),
        ShapePrimitive((-n/2,   -n, 0), (1, 1)),
    ])
    q.set_draw_mode(GL.GL_POLYGON)

    m = Media(args[0])
    t = Texture(data_src=m)
    s = Shader()
    tshape = TexturedShape(q, s, t)
    return tshape
