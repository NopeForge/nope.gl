import pynopegl as ngl


@ngl.scene()
def noise(cfg: ngl.SceneCfg):
    cfg.duration = 3
    return ngl.DrawNoise(type="perlin", octaves=4, scale=cfg.aspect_ratio, evolution=ngl.Time())
