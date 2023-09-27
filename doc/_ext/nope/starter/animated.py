import pynopegl as ngl


@ngl.scene()
def animated(cfg: ngl.SceneCfg):
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
    return ngl.Group(children=[bg, fg])
