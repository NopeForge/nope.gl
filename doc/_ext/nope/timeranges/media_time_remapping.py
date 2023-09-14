from pynopegl_utils.misc import SceneCfg, load_media, scene

import pynopegl as ngl


@scene()
def media_time_remapping(cfg: SceneCfg):
    cfg.duration = 10

    # Time remapping
    animkf = [
        ngl.AnimKeyFrameFloat(5, 4),
        ngl.AnimKeyFrameFloat(7, 10),
    ]

    # Basic media playback tree
    media = load_media(cfg, "mire")
    m = ngl.Media(media.filename, time_anim=ngl.AnimatedTime(animkf))
    t = ngl.Texture2D(data_src=m)
    r = ngl.RenderTexture(t)

    # Time range filter
    return ngl.TimeRangeFilter(r, start=2, end=9)
