import pynopegl as ngl


@ngl.scene()
def effects(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    keyframes = [
        ngl.PathKeyMove(to=(0, 1, 0)),
        ngl.PathKeyBezier3(to=(3, 0, 0), control1=(1, 3, 0), control2=(3, 2, 0)),
        ngl.PathKeyBezier2(to=(0, -3, 0), control=(3, -2, 0)),
        ngl.PathKeyBezier2(to=(-3, 0, 0), control=(-3, -2, 0)),
        ngl.PathKeyBezier3(to=(0, 1, 0), control1=(-3, 2, 0), control2=(-1, 3, 0)),
    ]

    path = ngl.Path(keyframes)
    draw = ngl.DrawPath(path, viewbox=(-5, -5, 10, 10), color=(0.8, 0.1, 0.1), glow=0.02)

    cfg.duration = 0.85
    scale = 1.1
    animkf = [
        ngl.AnimKeyFrameVec3(0, (1, 1, 1)),
        ngl.AnimKeyFrameVec3(0.1, (scale, scale, 1)),
        ngl.AnimKeyFrameVec3(0.2, (1, 1, 1)),
        ngl.AnimKeyFrameVec3(0.3, (scale, scale, 1)),
        ngl.AnimKeyFrameVec3(0.4, (1, 1, 1)),
    ]

    return ngl.Scale(draw, factors=ngl.AnimatedVec3(animkf))
