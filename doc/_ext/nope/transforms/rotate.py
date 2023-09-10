from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def rotate(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)

    scene = ngl.RenderColor(geometry=ngl.Quad())
    return ngl.Rotate(scene, angle=45)
