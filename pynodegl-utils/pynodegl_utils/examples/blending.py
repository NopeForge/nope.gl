from pynodegl import TexturedShape, Quad, Triangle, Shape, ShapePrimitive, Texture, Media, Shader, Group
from pynodegl import GLState, GLBlendState

from pynodegl_utils.misc import scene

from OpenGL import GL

fragment_data="""
void main() {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.5);
}
"""

@scene()
def blending_test(args, duration):
    g = Group()
    g2 = Group()

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(args[0])
    t = Texture(data_src=m)
    s = Shader()
    ts = TexturedShape(q, s, t)
    g.add_children(ts)

    q = Quad((-0.1, 0.0, 0), (1.1, 0, 0), (0, 1, 0))
    s = Shader(fragment_data=fragment_data)
    ts = TexturedShape(q, s)
    ts.add_glstates(GLBlendState(GL.GL_BLEND, GL.GL_TRUE,
                                 GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA,
                                 GL.GL_ONE, GL.GL_ZERO))
    g.add_children(ts)

    q = Quad((-1.0, 0.0, 0), (1.1, 0, 0), (0, 1, 0))
    s = Shader(fragment_data=fragment_data)
    ts = TexturedShape(q, s)
    ts.add_glstates(GLBlendState(GL.GL_BLEND, GL.GL_TRUE,
                                 GL.GL_ONE, GL.GL_ONE_MINUS_SRC_ALPHA,
                                 GL.GL_ONE, GL.GL_ZERO))
    g.add_children(ts)

    q = Quad((-0.125, -0.125, 0), (0.25, 0, 0), (0, 0.25, 0))
    s = Shader(fragment_data=fragment_data)
    ts = TexturedShape(q, s)
    ts.add_glstates(GLBlendState(GL.GL_BLEND, GL.GL_FALSE,
                                 GL.GL_ONE, GL.GL_ONE_MINUS_SRC_ALPHA,
                                 GL.GL_ONE, GL.GL_ZERO))

    g2.add_children(g, ts)

    return g2
