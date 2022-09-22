from pynodegl_utils.misc import SceneCfg, scene
from pynodegl_utils.toolbox.colors import get_random_color_buffer

import pynodegl as ngl


@scene(color=scene.Color(), rotate=scene.Bool(), scale=scene.Bool(), translate=scene.Bool())
def animated_square(cfg: SceneCfg, color=(1, 0.66, 0), rotate=True, scale=True, translate=True):
    """Animated Translate/Scale/Rotate on a square"""
    cfg.duration = 5.0
    cfg.aspect_ratio = (1, 1)

    sz = 1 / 3.0
    q = ngl.Quad((-sz / 2, -sz / 2, 0), (sz, 0, 0), (0, sz, 0))
    node = ngl.RenderColor(color, geometry=q)

    coords = [(-1, 1), (1, 1), (1, -1), (-1, -1), (-1, 1)]

    if rotate:
        animkf = (ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360))
        node = ngl.Rotate(node, angle=ngl.AnimatedFloat(animkf))

    if scale:
        animkf = (
            ngl.AnimKeyFrameVec3(0, (1, 1, 1)),
            ngl.AnimKeyFrameVec3(cfg.duration / 2, (2, 2, 2)),
            ngl.AnimKeyFrameVec3(cfg.duration, (1, 1, 1)),
        )
        node = ngl.Scale(node, factors=ngl.AnimatedVec3(animkf))

    if translate:
        animkf = []
        tscale = 1.0 / float(len(coords) - 1) * cfg.duration
        for i, xy in enumerate(coords):
            pos = (xy[0] * 0.5, xy[1] * 0.5, 0)
            t = i * tscale
            animkf.append(ngl.AnimKeyFrameVec3(t, pos))
        node = ngl.Translate(node, vector=ngl.AnimatedVec3(animkf))

    return node


@scene()
def animated_uniform(cfg: SceneCfg):
    """Uniform mat4 animated with a transform chain"""
    m0 = cfg.medias[0]
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = ngl.Media(m0.filename)
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert("texture"), fragment=cfg.get_frag("matrix-transform"))
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    ts = ngl.Render(q, p)
    ts.update_frag_resources(tex0=t)

    scale_animkf = [
        ngl.AnimKeyFrameVec3(0, (1, 1, 1)),
        ngl.AnimKeyFrameVec3(cfg.duration, (0.1, 0.1, 0.1), "quartic_out"),
    ]
    s = ngl.Scale(ngl.Identity(), factors=ngl.AnimatedVec3(scale_animkf))

    rotate_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360, "exp_out")]
    r = ngl.Rotate(s, axis=(0, 0, 1), angle=ngl.AnimatedFloat(rotate_animkf))

    u = ngl.UniformMat4(transform=r)
    ts.update_frag_resources(matrix=u)

    return ts


@scene(rotate=scene.Bool())
def animated_camera(cfg: SceneCfg, rotate=True):
    """Animated camera around a scene"""
    g = ngl.Group()

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = ngl.Media(cfg.medias[0].filename)
    t = ngl.Texture2D(data_src=m)
    node = ngl.RenderTexture(t, geometry=q)
    g.add_children(node)

    translate = ngl.Translate(node, vector=(-0.6, 0.8, -1))
    g.add_children(translate)

    translate = ngl.Translate(node, vector=(0.6, 0.8, -1))
    g.add_children(translate)

    translate = ngl.Translate(node, vector=(-0.6, -0.5, -1))
    g.add_children(translate)

    translate = ngl.Translate(node, vector=(0.6, -0.5, -1))
    g.add_children(translate)

    g = ngl.GraphicConfig(g, depth_test=True)
    camera = ngl.Camera(g)
    camera.set_eye(0, 0, 2)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_clipping(0.1, 10.0)

    tr_animkf = [ngl.AnimKeyFrameVec3(0, (0.0, 0.0, 0.0)), ngl.AnimKeyFrameVec3(10, (0.0, 0.0, 3.0), "exp_out")]
    node = ngl.Translate(ngl.Identity(), vector=ngl.AnimatedVec3(tr_animkf))

    if rotate:
        rot_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360, "exp_out")]
        node = ngl.Rotate(node, axis=(0, 1, 0), angle=ngl.AnimatedFloat(rot_animkf))

    camera.set_eye_transform(node)

    perspective_animkf = [
        ngl.AnimKeyFrameVec2(0.5, (60.0, cfg.aspect_ratio_float)),
        ngl.AnimKeyFrameVec2(cfg.duration, (45.0, cfg.aspect_ratio_float), "exp_out"),
    ]
    camera.set_perspective(ngl.AnimatedVec2(perspective_animkf))

    return camera


@scene(dim=scene.Range(range=[1, 100]))
def animated_buffer(cfg: SceneCfg, dim=50):
    """Transform a random buffer content using animations"""
    cfg.duration = 5.0

    nb_kf = int(cfg.duration)
    buffers = [get_random_color_buffer(cfg.rng, dim) for _ in range(nb_kf)]
    random_animkf = []
    time_scale = cfg.duration / float(nb_kf)
    for i, buf in enumerate(buffers + [buffers[0]]):
        random_animkf.append(ngl.AnimKeyFrameBuffer(i * time_scale, buf))
    random_buffer = ngl.AnimatedBufferVec4(keyframes=random_animkf)
    random_tex = ngl.Texture2D(data_src=random_buffer, width=dim, height=dim)
    return ngl.RenderTexture(random_tex)


@scene()
def animated_circles(cfg: SceneCfg):
    """Simple cyclic circles animation"""
    group = ngl.Group()

    cfg.duration = 5.0
    cfg.aspect_ratio = (1, 1)
    radius = 0.2
    n = 10
    step = 360.0 / n

    shape = ngl.Circle(radius=radius, npoints=128)
    render = ngl.RenderColor(geometry=shape)

    for i in range(n):
        mid_time = cfg.duration / 2.0
        start_time = mid_time / (i + 2)
        end_time = cfg.duration - start_time

        scale_animkf = [
            ngl.AnimKeyFrameVec3(start_time, (0, 0, 0)),
            ngl.AnimKeyFrameVec3(mid_time, (1.0, 1.0, 1.0), "exp_out"),
            ngl.AnimKeyFrameVec3(end_time, (0, 0, 0), "exp_in"),
        ]

        angle = i * step
        rotate_animkf = [
            ngl.AnimKeyFrameFloat(start_time, 0),
            ngl.AnimKeyFrameFloat(mid_time, angle, "exp_out"),
            ngl.AnimKeyFrameFloat(end_time, 0, "exp_in"),
        ]

        tnode = render
        tnode = ngl.Scale(tnode, factors=ngl.AnimatedVec3(scale_animkf))
        tnode = ngl.Translate(tnode, vector=(1 - radius, 0, 0))
        tnode = ngl.Rotate(tnode, angle=ngl.AnimatedFloat(rotate_animkf))

        group.add_children(tnode)

    return group
