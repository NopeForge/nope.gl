from pynopegl_utils.misc import load_media

import pynopegl as ngl


@ngl.scene()
def media_time_remapping(cfg: ngl.SceneCfg):
    cfg.duration = 10

    # Time remapping
    animkf = [
        ngl.AnimKeyFrameFloat(5, 4),
        ngl.AnimKeyFrameFloat(7, 10),
    ]

    # Basic media playback tree
    media = load_media("mire")
    m = ngl.Media(media.filename, time_anim=ngl.AnimatedTime(animkf))
    t = ngl.Texture2D(data_src=m)
    r = ngl.DrawTexture(t)

    # Time range filter
    return ngl.TimeRangeFilter(r, start=2, end=9)
