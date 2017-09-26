import array

from pynodegl import (
        AnimKeyFrameScalar,
        AnimationScalar,
        BufferVec3,
        Group,
        Render,
        Shader,
        Shape,
        UniformVec4,
)

from pynodegl_utils.misc import scene

from OpenGL import GL

_frag_data = """
#version 100
precision mediump float;
uniform vec4 color;
void main(void)
{
    gl_FragColor = color;
}"""

_colors = [
    (1, 0, 0, 0),
    (0, 1, 0, 0),
    (1, 0, 1, 0),
    (1, 1, 0, 0),
]

def _get_func(name, flags=0):

    widgets = [
        {'name': 'nb_points', 'type': 'range', 'range': [2, 200]},
        {'name': 'zoom', 'type': 'range', 'range': [0.1, 2], 'unit_base': 100}
    ]

    versions = []
    if flags & 1:
        versions += ['in', 'out']
    if flags & 2:
        versions += ['in_out', 'out_in']

    if not flags:
        versions = [None]

    for ext in versions:
        if ext is not None:
            widgets.append({'name': 'draw_' + ext, 'type': 'bool'})

    @scene(*widgets)
    def ret_func(cfg, nb_points=100, zoom=1,
                 draw_in=True, draw_out=True,
                 draw_in_out=True, draw_out_in=True):

        g = Group()
        shader = Shader(fragment_data=_frag_data)

        for idx, ext in enumerate(versions):

            interp = name
            if ext is not None:
                interp += '_' + ext
                if not eval('draw_' + ext):
                    continue

            anim = AnimationScalar([AnimKeyFrameScalar(-1,-1),
                                    AnimKeyFrameScalar( 1, 1, interp)])

            vertices_data = array.array('f')
            for i in range(nb_points + 1):
                x = (i/float(nb_points) * 2 - 1)
                y = anim.evaluate(x * 1/zoom) * zoom
                vertices_data.extend([x, y, 0])

            vertices = BufferVec3(data=vertices_data)
            shape = Shape(vertices, draw_mode=GL.GL_LINE_STRIP)
            render = Render(shape, shader)
            render.update_uniforms(color=UniformVec4(_colors[idx]))

            g.add_children(render)
        return g
    return ret_func

linear    = _get_func('linear')
quadratic = _get_func('quadratic', 3)
cubic     = _get_func('cubic',     3)
quartic   = _get_func('quartic',   3)
quintic   = _get_func('quintic',   3)
sinus     = _get_func('sinus',     3)
exp       = _get_func('exp',       3)
circular  = _get_func('circular',  3)
bounce    = _get_func('bounce',    1)
elastic   = _get_func('elastic',   1)
back      = _get_func('back',      3)
