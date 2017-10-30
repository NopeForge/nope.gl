from OpenGL import GL

from pynodegl import (
        AnimKeyFrameFloat,
        AnimationFloat,
        Camera,
        GLState,
        Group,
        Media,
        Program,
        Quad,
        Render,
        RenderToTexture,
        Rotate,
        Texture2D,
        UniformFloat,
        UniformVec3,
)

from pynodegl_utils.misc import scene, get_shader

def _get_cube_quads():
            # corner             width        height      color
    return (((-0.5, -0.5,  0.5), ( 1, 0,  0), (0, 1,  0), (1, 1, 0)), # front
            (( 0.5, -0.5, -0.5), (-1, 0,  0), (0, 1,  0), (0, 0, 1)), # back
            ((-0.5, -0.5, -0.5), ( 0, 0,  1), (0, 1,  0), (0, 1, 0)), # left
            (( 0.5, -0.5,  0.5), ( 0, 0, -1), (0, 1,  0), (0, 1, 1)), # right
            ((-0.5, -0.5, -0.5), ( 1, 0,  0), (0, 0,  1), (1, 0, 0)), # bottom
            ((-0.5,  0.5,  0.5), ( 1, 0,  0), (0, 0, -1), (1, 0, 1))) # top

def _get_cube_side(texture, program, corner, width, height, color):
    ts = Render(Quad(corner, width, height), program)
    ts.update_textures(tex0=texture)
    ts.update_uniforms(blend_color=UniformVec3(value=color))
    ts.update_uniforms(mix_factor=UniformFloat(value=0.2))
    return ts

@scene()
def rotating_cube(cfg):
    cube = Group(name="cube")
    cube.add_glstates(GLState(GL.GL_DEPTH_TEST, GL.GL_TRUE))

    frag_data = get_shader('tex-tint')
    p = Program(fragment=frag_data)

    t = Texture2D(data_src=Media(cfg.medias[0].filename))
    cube_quads_info = _get_cube_quads()
    children = [_get_cube_side(t, p, qi[0], qi[1], qi[2], qi[3]) for qi in _get_cube_quads()]
    cube.add_children(*children)

    for i in range(3):
        rot_animkf = AnimationFloat([AnimKeyFrameFloat(0, 0),
                                     AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))])
        axis = [int(i == x) for x in range(3)]
        cube = Rotate(cube, axis=axis, anim=rot_animkf)

    camera = Camera(cube)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio, 1.0, 10.0)

    return camera

@scene()
def scene_with_framebuffer(cfg):
    g = Group()

    d = Texture2D()
    d.set_format(GL.GL_DEPTH_COMPONENT)
    d.set_internal_format(GL.GL_DEPTH_COMPONENT)
    d.set_type(GL.GL_FLOAT)
    d.set_width(640)
    d.set_height(480)

    t = Texture2D()
    t.set_width(640)
    t.set_height(480)
    rtt = RenderToTexture(rotating_cube(cfg), t)
    rtt.set_depth_texture(d)

    q = Quad((-1.0, -1.0, 0), (1, 0, 0), (0, 1, 0))
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)
    g.add_children(rtt, render)

    q = Quad((0.0, 0.0, 0), (1, 0, 0), (0, 1, 0))
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=d)
    g.add_children(rtt, render)

    return g
