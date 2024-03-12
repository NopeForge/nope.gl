import pynopegl as ngl


@ngl.scene()
def circle(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    return ngl.DrawColor(geometry=ngl.Circle(radius=0.7, npoints=64))
