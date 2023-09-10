from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def animated_translate(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameVec3(0, (-1 / 3, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2, (1 / 3, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (-1 / 3, 0, 0)),
    ]

    scene = ngl.RenderColor(geometry=ngl.Circle(0.5))
    return ngl.Translate(scene, vector=ngl.AnimatedVec3(animkf))
