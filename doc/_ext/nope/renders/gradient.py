import pynopegl as ngl


@ngl.scene()
def gradient(cfg: ngl.SceneCfg):
    return ngl.RenderGradient(
        color0=(0, 0.5, 0.5),
        color1=(1, 0.5, 0),
    )
