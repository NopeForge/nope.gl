from pynopegl_utils.misc import load_media

import pynopegl as ngl


@ngl.scene()
def waveform(cfg: ngl.SceneCfg):
    image = load_media("rooster")

    stats = ngl.ColorStats(texture=ngl.Texture2D(data_src=ngl.Media(image.filename)))
    return ngl.RenderWaveform(stats, mode="luma_only")
