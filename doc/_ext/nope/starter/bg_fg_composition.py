from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def bg_fg_composition(cfg: SceneCfg):
    cfg.aspect_ratio = (1, 1)

    bg = ngl.RenderGradient(color0=(0, 0.5, 0.5), color1=(1, 0.5, 0))
    fg = ngl.Text("Hello World!", bg_opacity=0)
    return ngl.Group(children=[bg, fg])
