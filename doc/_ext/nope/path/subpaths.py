import pynopegl as ngl


@ngl.scene()
def subpaths(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    keyframes = [
        ngl.PathKeyMove(to=(0, 1, 0)),
        ngl.PathKeyBezier3(to=(3, 0, 0), control1=(1, 3, 0), control2=(3, 2, 0)),
        ngl.PathKeyBezier2(to=(0, -3, 0), control=(3, -2, 0)),
        ngl.PathKeyBezier2(to=(-3, 0, 0), control=(-3, -2, 0)),
        ngl.PathKeyBezier3(to=(0, 1, 0), control1=(-3, 2, 0), control2=(-1, 3, 0)),
        ngl.PathKeyMove(to=(0, -2, 0)),
        ngl.PathKeyLine(to=(1, -1, 0)),
        ngl.PathKeyBezier3(to=(0, 0, 0), control1=(2, 0, 0), control2=(0.5, 1, 0)),
        ngl.PathKeyBezier3(to=(-1, -1, 0), control1=(-0.5, 1, 0), control2=(-2, 0, 0)),
        ngl.PathKeyClose(),  # close the sub-path of the small heart with a straight line
    ]

    path = ngl.Path(keyframes)
    return ngl.DrawPath(path, viewbox=(-5, -5, 10, 10), color=(0.8, 0.1, 0.1), outline_color=(1, 1, 1))
