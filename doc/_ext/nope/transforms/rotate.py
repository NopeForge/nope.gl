import pynopegl as ngl


@ngl.scene()
def rotate(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    scene = ngl.RenderColor(geometry=ngl.Quad())
    return ngl.Rotate(scene, angle=45)
