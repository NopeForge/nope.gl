import pynopegl as ngl


@ngl.scene()
def hello_world(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    return ngl.Text("Hello World!")
