from pynopegl_utils.misc import SceneCfg, load_media, scene

import pynopegl as ngl


@scene()
def histogram(cfg: SceneCfg):
    image = load_media(cfg, "rooster")

    stats = ngl.ColorStats(texture=ngl.Texture2D(data_src=ngl.Media(image.filename)))
    return ngl.RenderHistogram(stats, mode="luma_only")
