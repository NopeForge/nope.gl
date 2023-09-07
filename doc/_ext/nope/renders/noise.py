from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def noise(cfg: SceneCfg):
    cfg.duration = 3
    return ngl.RenderNoise(type="perlin", octaves=4, scale=cfg.aspect_ratio, evolution=ngl.Time())
