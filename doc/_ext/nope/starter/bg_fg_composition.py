import pynopegl as ngl


@ngl.scene()
def bg_fg_composition(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    bg = ngl.DrawGradient(color0=(0, 0.5, 0.5), color1=(1, 0.5, 0))
    fg = ngl.Text("Hello World!", bg_opacity=0)
    return ngl.Group(children=[bg, fg])
