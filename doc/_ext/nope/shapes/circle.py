from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def circle(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)
    return ngl.RenderColor(geometry=ngl.Circle(radius=0.7, npoints=64))
