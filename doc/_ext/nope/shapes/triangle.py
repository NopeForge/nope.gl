import pynopegl as ngl


@ngl.scene()
def triangle(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    return ngl.DrawColor(geometry=ngl.Triangle())
