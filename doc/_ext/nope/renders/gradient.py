from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def gradient(cfg: SceneCfg):
    return ngl.RenderGradient(
        color0=(0, 0.5, 0.5),
        color1=(1, 0.5, 0),
    )
