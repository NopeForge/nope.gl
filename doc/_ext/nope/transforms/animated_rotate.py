import pynopegl as ngl


@ngl.scene()
def animated_rotate(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 360),
    ]

    scene = ngl.RenderColor(geometry=ngl.Quad())
    return ngl.Rotate(scene, angle=ngl.AnimatedFloat(animkf))
