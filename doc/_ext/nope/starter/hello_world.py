from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def hello_world(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)

    return ngl.Text("Hello World!")
