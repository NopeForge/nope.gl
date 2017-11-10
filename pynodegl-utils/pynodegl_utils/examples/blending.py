from pynodegl import (
        ConfigBlend,
        GraphicConfig,
        Group,
        Media,
        Program,
        Quad,
        Render,
        Texture2D,
        Triangle,
        UniformVec4,
)

from pynodegl_utils.misc import scene, get_shader

from OpenGL import GL

fragment = get_shader('color')

@scene()
def blending_test(cfg):
    g = Group()
    g2 = Group()

    ucolor = UniformVec4(value=(0, 0, 0, .5))

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    ts = Render(q, p)
    ts.update_textures(tex0=t)
    ts.update_uniforms(color=ucolor)
    g.add_children(ts)

    q = Quad((-0.1, 0.0, 0), (1.1, 0, 0), (0, 1, 0))
    p = Program(fragment=fragment)
    ts = Render(q, p)
    ts.update_uniforms(color=ucolor)
    s = GraphicConfig(ts, blend=ConfigBlend(GL.GL_TRUE,
                                            GL.GL_SRC_ALPHA,
                                            GL.GL_ONE_MINUS_SRC_ALPHA,
                                            GL.GL_ONE,
                                            GL.GL_ZERO))
    g.add_children(s)

    q = Quad((-1.0, 0.0, 0), (1.1, 0, 0), (0, 1, 0))
    p = Program(fragment=fragment)
    ts = Render(q, p)
    ts.update_uniforms(color=ucolor)
    s = GraphicConfig(ts, blend=ConfigBlend(GL.GL_TRUE,
                                            GL.GL_ONE,
                                            GL.GL_ONE_MINUS_SRC_ALPHA,
                                            GL.GL_ONE,
                                            GL.GL_ZERO))
    g.add_children(s)

    q = Quad((-0.125, -0.125, 0), (0.25, 0, 0), (0, 0.25, 0))
    p = Program(fragment=fragment)
    ts = Render(q, p)
    ts.update_uniforms(color=ucolor)
    g2.add_children(g, ts)

    return g2
