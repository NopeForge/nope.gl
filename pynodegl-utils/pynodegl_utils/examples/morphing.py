import array
import math
import random

from OpenGL import GL

from pynodegl import (
        AnimKeyFrameVec4,
        AnimatedBufferVec3,
        AnimKeyFrameBuffer,
        AnimatedVec4,
        Geometry,
        Program,
        Render,
        UniformVec4,
)

from pynodegl_utils.misc import scene, get_frag


@scene(square_color={'type': 'color'},
       circle_color={'type': 'color'})
def square2circle(cfg, square_color=(0.9, 0.1, 0.3, 1.0), circle_color=(1.0, 1.0, 1.0, 1.0)):
    cfg.duration = 5

    def sqxf(t): # square x coordinates clockwise starting top-left
        if t < 1/4.: return t*4
        if t < 1/2.: return 1
        if t < 3/4.: return 1.-(t-.5)*4
        return 0

    def sqyf(t): # square y coordinates clockwise starting top-left
        if t < 1/4.: return 1
        if t < 1/2.: return 1.-(t-.25)*4
        if t < 3/4.: return 0
        return (t-.75)*4

    n = 1024 # number of vertices
    s = 1.25 # shapes scale
    interp = 'exp_in_out'

    square_vertices = array.array('f')
    for i in range(n):
        x = (sqxf(i / float(n)) - .5) * s
        y = (sqyf(i / float(n)) - .5) * s
        square_vertices.extend([x, y, 0])

    circle_vertices = array.array('f')
    step = 2 * math.pi / float(n)
    for i in range(n):
        angle = i * step - math.pi/4.
        x = math.sin(angle) * .5 * s
        y = math.cos(angle) * .5 * s
        circle_vertices.extend([x, y, 0])

    vertices_animkf = [
            AnimKeyFrameBuffer(0,               square_vertices),
            AnimKeyFrameBuffer(cfg.duration/2., circle_vertices, interp),
            AnimKeyFrameBuffer(cfg.duration,    square_vertices, interp),
    ]
    vertices = AnimatedBufferVec3(vertices_animkf)

    color_animkf = [
            AnimKeyFrameVec4(0,               square_color),
            AnimKeyFrameVec4(cfg.duration/2., circle_color, interp),
            AnimKeyFrameVec4(cfg.duration,    square_color, interp),
    ]
    ucolor = UniformVec4(anim=AnimatedVec4(color_animkf))

    geom = Geometry(vertices)
    geom.set_draw_mode(GL.GL_TRIANGLE_FAN)
    p = Program(fragment=get_frag('color'))
    render = Render(geom, p)
    render.update_uniforms(color=ucolor)
    return render


@scene(npoints={'type': 'range', 'range': [3, 100]})
def urchin(cfg, npoints=25):
    cfg.duration = 5

    random.seed(0)

    def get_vertices(n, radius_func, offset=0):
        vertices = []
        step = 2 * math.pi / n
        for i in range(n):
            angle = (i + offset) * step
            radius = radius_func()
            x, y = math.sin(angle) * radius, math.cos(angle) * radius
            vertices.append([x, y, 0])
        return vertices

    k = 16
    n, m = .1, .9

    inner_rfunc = lambda: n
    inner_vertices = get_vertices(npoints, inner_rfunc)

    vdata = []
    for i in range(k):
        outer_rfunc = lambda: random.uniform(n, m)
        outer_vertices = get_vertices(npoints, outer_rfunc, offset=.5)
        vertices_data = array.array('f')
        for inner_vertex, outer_vertex in zip(inner_vertices, outer_vertices):
            vertices_data.extend(inner_vertex + outer_vertex)
        vertices_data.extend(inner_vertices[0])
        vdata.append(vertices_data)

    animkf = []
    for i, v in enumerate(vdata + [vdata[0]]):
        animkf.append(AnimKeyFrameBuffer(i*cfg.duration/float(k), v))

    vertices = AnimatedBufferVec3(animkf)

    geom = Geometry(vertices)
    geom.set_draw_mode(GL.GL_LINE_STRIP)
    p = Program(fragment=get_frag('color'))
    render = Render(geom, p)
    render.update_uniforms(color=UniformVec4(value=(.9, .1, .3, 1)))
    return render
