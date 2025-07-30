import pynopegl as ngl


@ngl.scene()
def animated(cfg: ngl.SceneCfg):
    cfg.duration = 3
    cfg.aspect_ratio = (1, 1)

    keyframes = [
        ngl.PathKeyMove(to=(0, 1, 0)),
        ngl.PathKeyBezier3(to=(3, 0, 0), control1=(1, 3, 0), control2=(3, 2, 0)),
        ngl.PathKeyBezier2(to=(0, -3, 0), control=(3, -2, 0)),
        ngl.PathKeyBezier2(to=(-3, 0, 0), control=(-3, -2, 0)),
        ngl.PathKeyBezier3(to=(0, 1, 0), control1=(-3, 2, 0), control2=(-1, 3, 0)),
    ]

    path = ngl.Path(keyframes)
    heart = ngl.DrawPath(path, viewbox=(-5, -5, 10, 10), color=(0.8, 0.1, 0.1), outline=0.03, outline_color=(1, 1, 1))

    # This animation defines the speed at which the path is walked
    anim_kf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1, "cubic_in_out"),
    ]

    # We re-use the same shape but we could use anything
    small_heart = ngl.Translate(heart, vector=ngl.AnimatedPath(anim_kf, path))

    # Readjust to fit the viewbox
    small_heart = ngl.Scale(small_heart, (1 / 5, 1 / 5, 1))

    return ngl.Group(children=[heart, small_heart])
