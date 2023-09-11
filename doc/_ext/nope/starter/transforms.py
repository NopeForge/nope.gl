from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def transforms(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    d = cfg.duration

    color0_animkf = [
        ngl.AnimKeyFrameColor(time=0, color=(0.0, 0.5, 0.5)),
        ngl.AnimKeyFrameColor(time=d / 2, color=(1.0, 0.0, 1.0)),
        ngl.AnimKeyFrameColor(time=d, color=(0.0, 0.5, 0.5)),
    ]

    color1_animkf = [
        ngl.AnimKeyFrameColor(time=0, color=(1.0, 0.5, 0.5)),
        ngl.AnimKeyFrameColor(time=d / 2, color=(1.0, 1.0, 0.5)),
        ngl.AnimKeyFrameColor(time=d, color=(1.0, 0.5, 0.5)),
    ]

    color0 = ngl.AnimatedColor(keyframes=color0_animkf)
    color1 = ngl.AnimatedColor(keyframes=color1_animkf)

    bg = ngl.RenderGradient(color0=color0, color1=color1)
    fg = ngl.Text("Hello World!", bg_opacity=0)

    rotate_animkf = [
        ngl.AnimKeyFrameFloat(time=0, value=15),
        ngl.AnimKeyFrameFloat(time=d / 2, value=-15, easing="exp_in"),
        ngl.AnimKeyFrameFloat(time=d, value=15, easing="exp_out"),
    ]

    scaled_fg = ngl.Scale(fg, factors=(0.7, 0.7, 0.7))
    rotated_fg = ngl.Rotate(scaled_fg, angle=ngl.AnimatedFloat(rotate_animkf))

    return ngl.Group(children=[bg, rotated_fg])
